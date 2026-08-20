#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

struct Config {
    int port = 3000;
    int dockerTimeoutSeconds = 10;
    std::string dockerMemoryLimit = "128m";
    std::string dockerCpuLimit = "0.5";
    int dockerPidsLimit = 50;
    int rateLimitWindowSeconds = 60;
    int rateLimitMaxRequests = 10;

    static Config fromEnv() {
        Config c;
        auto readInt = [](const char* name, int fallback) -> int {
            auto* v = std::getenv(name);
            if (!v) return fallback;
            try { return std::stoi(v); }
            catch (...) {
                std::cerr << "WARNING: Invalid integer for " << name
                          << "=\"" << v << "\", using default " << fallback << std::endl;
                return fallback;
            }
        };
        c.port                   = readInt("PORT", c.port);
        c.dockerTimeoutSeconds   = readInt("DOCKER_TIMEOUT_SECONDS", c.dockerTimeoutSeconds);
        c.dockerPidsLimit        = readInt("DOCKER_PIDS_LIMIT", c.dockerPidsLimit);
        c.rateLimitWindowSeconds = readInt("RATE_LIMIT_WINDOW_SECONDS", c.rateLimitWindowSeconds);
        c.rateLimitMaxRequests   = readInt("RATE_LIMIT_MAX_REQUESTS", c.rateLimitMaxRequests);
        if (auto* v = std::getenv("DOCKER_MEMORY_LIMIT")) c.dockerMemoryLimit = v;
        if (auto* v = std::getenv("DOCKER_CPU_LIMIT"))     c.dockerCpuLimit = v;
        return c;
    }
};
