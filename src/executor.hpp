#pragma once

#include <string>
#include "config.hpp"

struct ExecResult {
    std::string stdoutStr;
    std::string stderrStr;
    int exitCode = 0;
    bool timedOut = false;
};

// Check if Docker daemon is reachable
bool isDockerAvailable();

// Execute user code inside a Docker container
ExecResult executeCode(
    const std::string& language,
    const std::string& code,
    const std::string& input,
    const Config& config
);
