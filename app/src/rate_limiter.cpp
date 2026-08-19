#include "rate_limiter.hpp"

static const int PURGE_INTERVAL = 100;

bool RateLimiter::allow(const std::string& clientIp) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    // Purge periodically OR when at capacity
    if (++callsSincePurge_ >= PURGE_INTERVAL || buckets_.size() >= maxEntries_) {
        purgeStale(now);
        callsSincePurge_ = 0;
    }

    auto it = buckets_.find(clientIp);
    if (it == buckets_.end()) {
        // Reject new IPs if still at capacity after purge
        if (buckets_.size() >= maxEntries_) {
            return false;
        }
        buckets_[clientIp] = {1, now};
        return true;
    }

    auto& bucket = it->second;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - bucket.windowStart
    ).count();

    if (elapsed >= windowSeconds_) {
        bucket.count = 1;
        bucket.windowStart = now;
        return true;
    }

    if (bucket.count >= maxRequests_) {
        return false;
    }

    bucket.count++;
    return true;
}

void RateLimiter::purgeStale(std::chrono::steady_clock::time_point now) {
    auto staleThreshold = std::chrono::seconds(windowSeconds_ * 2);
    for (auto it = buckets_.begin(); it != buckets_.end(); ) {
        if (now - it->second.windowStart > staleThreshold) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}
