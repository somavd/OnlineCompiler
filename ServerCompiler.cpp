#include<crow.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <filesystem>
#include <chrono>
#include <random>
#include <future>

namespace fs = std::filesystem;

std::string exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error: failed to run command\n";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Generate unique ID for temp folder
std::string gen_unique_id() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::to_string(now);
}

// URL decode helper
std::string url_decode(const std::string& s) {
    std::string out;
    char ch;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            ch = static_cast<char>(std::strtol(s.substr(i+1,2).c_str(), nullptr, 16));
            out += ch;
            i += 2;
        } else out += s[i];
    }
    return out;
}

int main() {
    crow::SimpleApp app;

    // Serve HTML editor
    CROW_ROUTE(app, "/")([]() {
        std::ifstream file("htmls/editor.html");
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    });

    // Handle code execution
    CROW_ROUTE(app, "/run").methods("POST"_method)
    ([](const crow::request& req) {
        std::string body = req.body;
        auto lang_pos = body.find("language=");
        auto code_pos = body.find("&code=");
        if (lang_pos == std::string::npos || code_pos == std::string::npos)
            return crow::response(400, "Bad Request");

        std::string lang = body.substr(lang_pos + 9, code_pos - (lang_pos + 9));
        std::string code = body.substr(code_pos + 6);
        lang = url_decode(lang);
        code = url_decode(code);

        // Unique temp dir
        std::string id = gen_unique_id();
        fs::create_directories("tmp/" + id);

        std::ostringstream result;
        if (lang == "cpp") {
            std::string cppFile = "tmp/" + id + "/program.cpp";
            std::ofstream ofs(cppFile);
            ofs << code;
            ofs.close();

            // Compile + run inside container
            std::string cmd =
                "sudo docker run --rm "
                "-v " + fs::absolute("tmp/" + id).string() + ":/workspace "
                "-w /workspace gcc:latest "
                "/bin/bash -c \"g++ program.cpp -o program 2>compile_err.txt && ./program >prog_out.txt 2>prog_err.txt\"";

            exec_cmd(cmd);

            std::string compile_err = exec_cmd("cat tmp/" + id + "/compile_err.txt");
            std::string prog_out = exec_cmd("cat tmp/" + id + "/prog_out.txt");
            std::string prog_err = exec_cmd("cat tmp/" + id + "/prog_err.txt");

            if (!compile_err.empty()) {
                result << "<h3>Compilation Error:</h3><pre>" << compile_err << "</pre>";
            } else {
                result << "<h3>Program Output:</h3><pre>" << prog_out << "</pre>";
                if (!prog_err.empty())
                    result << "<h3>Runtime Error:</h3><pre>" << prog_err << "</pre>";
            }
        } else {
            result << "<h3>Only C++ supported for now</h3>";
        }

        // Cleanup temp folder
        fs::remove_all("tmp/" + id);

        result << "<a href='/'>Back</a>";
        return crow::response(result.str());
    });

    app.port(18080).multithreaded().run();
}

