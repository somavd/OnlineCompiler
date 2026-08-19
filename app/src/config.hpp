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
        auto readInt = [](const char* name, int fallback, int minVal, int maxVal) -> int {
            auto* v = std::getenv(name);
            if (!v) return fallback;
            try {
                int val = std::stoi(v);
                if (val < minVal || val > maxVal) {
                    std::cerr << "WARNING: " << name << "=" << val
                              << " out of range [" << minVal << "," << maxVal
                              << "], using default " << fallback << std::endl;
                    return fallback;
                }
                return val;
            }
            catch (...) {
                std::cerr << "WARNING: Invalid integer for " << name
                          << "=\"" << v << "\", using default " << fallback << std::endl;
                return fallback;
            }
        };
        c.port                   = readInt("PORT", c.port, 1, 65535);
        c.dockerTimeoutSeconds   = readInt("DOCKER_TIMEOUT_SECONDS", c.dockerTimeoutSeconds, 1, 300);
        c.dockerPidsLimit        = readInt("DOCKER_PIDS_LIMIT", c.dockerPidsLimit, 1, 1000);
        c.rateLimitWindowSeconds = readInt("RATE_LIMIT_WINDOW_SECONDS", c.rateLimitWindowSeconds, 1, 3600);
        c.rateLimitMaxRequests   = readInt("RATE_LIMIT_MAX_REQUESTS", c.rateLimitMaxRequests, 1, 10000);
        if (auto* v = std::getenv("DOCKER_MEMORY_LIMIT")) c.dockerMemoryLimit = v;
        if (auto* v = std::getenv("DOCKER_CPU_LIMIT"))     c.dockerCpuLimit = v;
        return c;
    }
};
