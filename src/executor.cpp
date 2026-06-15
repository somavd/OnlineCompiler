#include "executor.hpp"
#include "languages.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <array>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

static const size_t MAX_OUTPUT_BYTES = 64 * 1024;  // 64KB per stream
static const std::string TMP_BASE = "tmp";

static std::string generateSessionId() {
    static std::mutex rngMutex;
    static std::mt19937_64 rng(std::random_device{}());
    std::lock_guard<std::mutex> lock(rngMutex);
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

static std::string truncate(const std::string& s) {
    if (s.size() <= MAX_OUTPUT_BYTES) return s;
    return s.substr(0, MAX_OUTPUT_BYTES) + "\n...[output truncated]";
}

// Run a command, capture stdout + stderr via pipes, enforce timeout.
// Uses select() to read both pipes concurrently, avoiding deadlock
// when child output exceeds the OS pipe buffer.
static int runProcess(
    const std::vector<std::string>& args,
    int timeoutSeconds,
    std::string& outStr,
    std::string& errStr,
    bool& timedOut
) {
    int stdoutPipe[2], stderrPipe[2];
    if (pipe(stdoutPipe) != 0) {
        errStr = "Failed to create stdout pipe";
        return 1;
    }
    if (pipe(stderrPipe) != 0) {
        errStr = "Failed to create stderr pipe";
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        errStr = "Failed to fork";
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return 1;
    }

    if (pid == 0) {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        std::vector<const char*> argv;
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(127);  // exec failed
    }

    // Parent: close write ends
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    timedOut = false;
    outStr.clear();
    errStr.clear();

    int outFd = stdoutPipe[0];
    int errFd = stderrPipe[0];
    bool outDone = false, errDone = false;
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeoutSeconds);
    std::array<char, 4096> buf;

    // Read both pipes concurrently via select() until EOF or timeout
    while (!outDone || !errDone) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()
        );
        if (remaining.count() <= 0) {
            kill(pid, SIGKILL);
            timedOut = true;
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        int maxFd = 0;
        if (!outDone) { FD_SET(outFd, &readfds); maxFd = std::max(maxFd, outFd); }
        if (!errDone) { FD_SET(errFd, &readfds); maxFd = std::max(maxFd, errFd); }

        long ms = std::min(remaining.count(), (long long)500);
        struct timeval tv;
        tv.tv_sec  = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;

        int sel = select(maxFd + 1, &readfds, nullptr, nullptr, &tv);
        if (sel < 0) break;

        if (!outDone && FD_ISSET(outFd, &readfds)) {
            ssize_t n = read(outFd, buf.data(), buf.size());
            if (n <= 0) outDone = true;
            else if (outStr.size() < MAX_OUTPUT_BYTES) outStr.append(buf.data(), n);
        }
        if (!errDone && FD_ISSET(errFd, &readfds)) {
            ssize_t n = read(errFd, buf.data(), buf.size());
            if (n <= 0) errDone = true;
            else if (errStr.size() < MAX_OUTPUT_BYTES) errStr.append(buf.data(), n);
        }
    }

    close(outFd);
    close(errFd);

    // Reap child
    int status = 0;
    waitpid(pid, &status, 0);

    outStr = truncate(outStr);
    errStr = truncate(errStr);

    if (timedOut) return 124;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
}

bool isDockerAvailable() {
    std::string out, err;
    bool timedOut;
    int code = runProcess({"docker", "info"}, 5, out, err, timedOut);
    return code == 0 && !timedOut;
}

ExecResult executeCode(
    const std::string& language,
    const std::string& code,
    const std::string& input,
    const Config& config
) {
    ExecResult result;
    std::error_code ec;

    const auto& langs = getLanguages();
    auto it = langs.find(language);
    if (it == langs.end()) {
        result.stderrStr = "Unsupported language";
        result.exitCode = 1;
        return result;
    }

    const auto& lang = it->second;

    // Create session directory
    fs::create_directories(TMP_BASE);
    std::string sessionId = generateSessionId();
    std::string sessionDir = fs::absolute(TMP_BASE + "/" + sessionId).string();
    fs::create_directory(sessionDir);

    // Write source file
    {
        std::ofstream ofs(sessionDir + "/" + lang.fileName);
        if (!ofs || !(ofs << code)) {
            result.stderrStr = "Failed to write source file";
            result.exitCode = 1;
            fs::remove_all(sessionDir, ec);
            return result;
        }
    }

    // Write input file if provided
    bool hasInput = !input.empty();
    if (hasInput) {
        std::ofstream ofs(sessionDir + "/input.txt");
        ofs << input;
    }

    // Build container command
    std::string containerCmd;
    if (!lang.compileCmd.empty()) {
        containerCmd = lang.compileCmd + " && " + lang.runCmd;
    } else {
        containerCmd = lang.runCmd;
    }
    if (hasInput) {
        containerCmd += " < input.txt";
    }

    // Build docker args
    std::vector<std::string> args = {
        "docker", "run", "--rm",
        "--network", "none",
        "--memory", config.dockerMemoryLimit,
        "--cpus", config.dockerCpuLimit,
        "--pids-limit", std::to_string(config.dockerPidsLimit),
        "--read-only",
        "--tmpfs", "/tmp:size=16m",
        "-v", sessionDir + ":/workspace",
        "-w", "/workspace",
        lang.image,
        "/bin/sh", "-c", containerCmd,
    };

    result.exitCode = runProcess(
        args, config.dockerTimeoutSeconds,
        result.stdoutStr, result.stderrStr, result.timedOut
    );

    if (result.timedOut) {
        result.stdoutStr = "";
        result.stderrStr = "Execution timed out";
    }

    // Cleanup
    fs::remove_all(sessionDir, ec);

    return result;
}
