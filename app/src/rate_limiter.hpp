#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

class RateLimiter {
public:
    RateLimiter(int windowSeconds, int maxRequests, size_t maxEntries = 10000)
        : windowSeconds_(windowSeconds), maxRequests_(maxRequests), maxEntries_(maxEntries) {}

    // Returns true if request is allowed
    bool allow(const std::string& clientIp);

private:
    // Remove entries older than 2x window to prevent unbounded memory growth
    void purgeStale(std::chrono::steady_clock::time_point now);

    struct Bucket {
        int count = 0;
        std::chrono::steady_clock::time_point windowStart;
    };

    int windowSeconds_;
    int maxRequests_;
    size_t maxEntries_;
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
    int callsSincePurge_ = 0;
};
