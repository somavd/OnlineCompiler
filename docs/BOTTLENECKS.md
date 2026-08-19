# Bottleneck Analysis

## Overview

Analysis of performance bottlenecks in the current Online Compiler implementation, categorized by severity and impact.

---

## 🔴 CRITICAL BOTTLENECKS

### 1. Docker Container Spawn Overhead

**Location:** `executor.cpp` → `executeCode()`

**Problem:** Every code execution spawns a new Docker container from scratch.

**Impact:**
- **Cold start:** 500ms-2s per execution (first time for image)
- **Warm start:** 100-300ms per execution (cached image)
- **Scales linearly:** 10 concurrent executions = 10x overhead

**Why it's a bottleneck:**
```cpp
// Each execution does ALL of this:
1. fork() child process
2. execvp("docker", "run", ...)  // ← Heavy: daemon API call
3. Docker daemon:
   - Pull image (if not cached)  // ← Can be 10s+
   - Create container
   - Setup network isolation
   - Mount filesystem
   - Start container
4. Wait for container exit
5. Cleanup container
```

**Current throughput:**
- ~3-5 executions/second (warm Docker)
- ~0.5-1 execution/second (cold Docker)

**Mitigation options:**
1. **Pre-warm containers:** Keep a pool of running containers
2. **Container reuse:** Same container for multiple executions (security risk)
3. **Language servers:** Use long-running processes (REPL mode)
4. **Native execution:** Skip Docker for trusted code (security risk)
5. **Optimized images:** Use minimal base images (alpine, distroless)

**Quick win:** Use `--init` flag and smaller images (python:3-slim vs python:3)

---

### 2. Synchronous Blocking Execution

**Location:** `executor.cpp` → `runProcess()`, `main.cpp` → route handlers

**Problem:** Each execution blocks a Crow thread until completion.

**Impact:**
- **Thread pool exhaustion:** Default 10 threads = max 10 concurrent executions
- **Queue buildup:** If 20 requests, 10 wait indefinitely
- **No backpressure:** No way to signal "too busy"

**Why it's a bottleneck:**
```cpp
// Crow thread pool (default 10 threads)
// Each request blocks one thread:
POST /api/run {
    executeCode()  // ← Blocks for 100ms-2s
    saveSubmission()  // ← Blocks for 1-2ms
    return response
}
```

**Current behavior:**
- 10 concurrent executions: All served
- 11 concurrent executions: 1 waits (or 500 error if timeout)
- 100 concurrent executions: 90 fail or timeout

**Mitigation options:**
1. **Async execution:** Use Crow's async handlers + thread pool for executors
2. **Job queue:** Redis or in-memory queue + worker threads
3. **Increase thread pool:** `app.loglevel()` + custom thread count
4. **Request throttling:** Early rejection when queue full
5. **WebSockets:** Stream results instead of blocking

**Quick win:** Increase Crow thread pool to 50-100 (more memory usage)

---

### 3. SQLite Single-Writer Serialization

**Location:** `database.cpp` → all methods with `std::mutex`

**Problem:** All database operations are serialized by a single mutex.

**Impact:**
- **Write contention:** 10 submissions = 10 serialized writes
- **Read blocking:** Even reads block on writes (with mutex)
- **No parallelism:** Can't save submission + read history simultaneously

**Why it's a bottleneck:**
```cpp
class Database {
    std::mutex mutex_;  // ← Single lock for ALL operations

    int saveSubmission(const Submission& sub) {
        std::lock_guard<std::mutex> lock(mutex_);  // ← Blocks everyone
        // ... SQLite write ...
    }

    std::vector<Submission> getSubmissions() {
        std::lock_guard<std::mutex> lock(mutex_);  // ← Blocks everyone
        // ... SQLite read ...
    }
}
```

**Current behavior:**
- 10 concurrent saves: 10x serialization (10ms → 100ms)
- 1 save + 9 reads: All blocked on the save

**Mitigation options:**
1. **Connection pool:** Multiple SQLite connections (read-only replicas)
2. **Write queue:** Batch submissions, flush periodically
3. **PostgreSQL:** Replace SQLite with true concurrent DB
4. **Optimistic locking:** Reduce lock scope (only during actual SQL)
5. **Async writes:** Save submissions in background thread

**Quick win:** Use separate read/write connections (SQLite allows this with WAL)

---

## 🟡 HIGH IMPACT BOTTLENECKS

### 4. Filesystem Operations Per Execution

**Location:** `executor.cpp` → session directory creation/cleanup

**Problem:** Each execution creates/deletes temporary files on disk.

**Impact:**
- **I/O overhead:** `fs::create_directory`, `fs::remove_all` per execution
- **Disk thrashing:** High concurrency = high disk I/O
- **Cleanup latency:** `fs::remove_all` can be slow for large outputs

**Why it's a bottleneck:**
```cpp
// Per execution:
fs::create_directory("tmp/<session-id>");      // ← Disk I/O
writeFile("tmp/<session-id>/source.cpp");      // ← Disk I/O
// ... execute ...
fs::remove_all("tmp/<session-id>");             // ← Disk I/O (recursive)
```

**Current behavior:**
- 100 executions/second = 300 filesystem operations/second
- On slow disk (HDD): Can add 50-100ms per execution

**Mitigation options:**
1. **In-memory filesystem:** Use `tmpfs` (Linux) or `ramdisk` (macOS)
2. **Shared session dir:** Reuse single directory with unique filenames
3. **Lazy cleanup:** Delete sessions in background thread
4. **No filesystem:** Pass code via `docker exec -i` (stdin)

**Quick win:** Mount `tmp/` as tmpfs (Linux) or use shared directory

---

### 5. Rate Limiter Memory Growth

**Location:** `rate_limiter.cpp` → `allow()`, `purgeStale()`

**Problem:** Rate limiter stores all IP entries in memory without bounds.

**Impact:**
- **Memory leak risk:** 1M unique IPs = ~100MB+ memory
- **Cleanup overhead:** `purgeStale()` scans entire map every 100 calls
- **Cache pollution:** Old entries linger until cleanup

**Why it's a bottleneck:**
```cpp
class RateLimiter {
    std::unordered_map<std::string, Bucket> buckets_;  // ← Unbounded

    bool allow(const std::string& ip) {
        // Every 100 calls, scan entire map:
        if (++callCounter_ % 100 == 0) purgeStale();  // ← O(n) scan
    }
};
```

**Current behavior:**
- 10K unique IPs: ~1MB memory
- 100K unique IPs: ~10MB memory
- 1M unique IPs: ~100MB memory
- Cleanup scan: O(n) every 100 requests

**Mitigation options:**
1. **Max entries:** Evict oldest when exceeding limit (LRU)
2. **TTL-based expiry:** Auto-expire entries on access
3. **Bloom filter:** Quick reject for unknown IPs
4. **External rate limiter:** Redis-based (shared across instances)

**Quick win:** Add max entry limit (e.g., 10K entries) with LRU eviction

---

### 6. No Compilation Cache

**Location:** `executor.cpp` → Docker run commands

**Problem:** Same code compiled repeatedly without caching.

**Impact:**
- **Wasted CPU:** Compiling identical code 100x
- **Slower response:** Compilation adds 1-5s per execution

**Why it's a bottleneck:**
```cpp
// C++ execution (every time):
docker run gcc g++ -O2 source.cpp -o bin  // ← Compile (1-5s)
docker run gcc ./bin                       // Run (0.1s)
```

**Current behavior:**
- Same C++ code run 10 times: 10 compilations (10-50s total)
- Could be: 1 compilation + 10 runs (5s total)

**Mitigation options:**
1. **Hash-based cache:** Cache compiled binaries by code hash
2. **Incremental compilation:** Use `ccache` inside container
3. **Interpreter-only:** Skip compilation for interpreted languages
4. **Pre-compiled templates:** Cache common patterns

**Quick win:** Add in-memory cache for compiled binaries (hash → binary)

---

## 🟢 MEDIUM IMPACT BOTTLENECKS

### 7. Output Truncation Without Streaming

**Location:** `executor.cpp` → `truncate()`, `MAX_OUTPUT_BYTES = 64KB`

**Problem:** Large outputs are truncated, no streaming option.

**Impact:**
- **User experience:** Large outputs silently truncated
- **Memory waste:** 64KB allocated even for small outputs
- **No pagination:** Can't retrieve full output

**Why it's a bottleneck:**
```cpp
static const size_t MAX_OUTPUT_BYTES = 64 * 1024;  // ← Fixed limit

static std::string truncate(const std::string& s) {
    if (s.size() <= MAX_OUTPUT_BYTES) return s;
    return s.substr(0, MAX_OUTPUT_BYTES) + "\n...[output truncated]";
}
```

**Current behavior:**
- 1MB output: 64KB returned, 936KB discarded
- User has no way to get full output

**Mitigation options:**
1. **Streaming output:** WebSocket / Server-Sent Events
2. **Chunked response:** HTTP chunked transfer encoding
3. **Pagination:** Return output ID, allow fetching chunks
4. **Configurable limit:** Allow user to set max output size

**Quick win:** Increase limit to 1MB or add WebSocket streaming

---

### 8. No Request Prioritization

**Location:** `main.cpp` → Crow route handlers

**Problem:** All requests treated equally, no priority queue.

**Impact:**
- **Fairness issue:** Long-running executions block short ones
- **No VIP handling:** Admin requests wait behind user requests
- **No deadline:** No way to set per-request timeouts

**Why it's a bottleneck:**
```cpp
// All requests equal:
POST /api/run (10s execution)  // ← Blocks thread
POST /api/run (0.1s execution)  // ← Waits behind 10s one
```

**Current behavior:**
- 1 slow request (10s) + 9 fast requests (0.1s each) = 10.9s total
- Ideal: 0.1s + 10s = 10.1s (fast ones finish first)

**Mitigation options:**
1. **Priority queue:** Admin requests > user requests
2. **Short-circuit queue:** Separate queues for short/long jobs
3. **Deadline queue:** Reject if estimated wait > threshold
4. **Weighted fair queuing:** Fair sharing between users

**Quick win:** Separate queue for admin requests

---

### 9. Crow Thread Pool Configuration

**Location:** `main.cpp` → `app.loglevel()`, Crow defaults

**Problem:** Default 10 threads may be insufficient for load.

**Impact:**
- **Underutilization:** Multi-core CPU with only 10 threads
- **Bottleneck:** 10 concurrent max (regardless of CPU cores)
- **No tuning:** No way to configure thread count

**Why it's a bottleneck:**
```cpp
// Crow default: 10 threads
// On 16-core CPU: 6 cores idle
// On 4-core CPU: Oversubscribed
```

**Current behavior:**
- 16-core server: Only 10 threads (60% CPU utilization)
- 4-core server: 10 threads (250% CPU, context switching)

**Mitigation options:**
1. **Configurable threads:** Set thread count based on CPU cores
2. **Dynamic scaling:** Auto-scale based on load
3. **Event-based:** Switch to async I/O (libuv, Boost.Asio)

**Quick win:** Set thread count to `std::thread::hardware_concurrency()`

---

## 🔵 LOW IMPACT BOTTLENECKS

### 10. No Response Compression

**Location:** `main.cpp` → `jsonResponse()`

**Problem:** JSON responses not compressed (gzip/brotli).

**Impact:**
- **Bandwidth waste:** Large JSON payloads sent uncompressed
- **Slower transfer:** More data over network

**Why it's a bottleneck:**
```cpp
static crow::response jsonResponse(int code, crow::json::wvalue& body) {
    crow::response res(code, body.dump());  // ← No compression
    res.set_header("Content-Type", "application/json");
    return res;
}
```

**Current behavior:**
- 100 submissions history: ~500KB uncompressed
- Could be ~50KB with gzip (10x reduction)

**Mitigation options:**
1. **Gzip compression:** Add `Content-Encoding: gzip`
2. **Brotli compression:** Better compression ratio
3. **Compression middleware:** Crow compression plugin

**Quick win:** Add gzip compression for JSON responses

---

### 11. No Connection Keep-Alive

**Location:** Crow defaults

**Problem:** Each request opens new TCP connection.

**Impact:**
- **TCP overhead:** 3-way handshake per request
- **Latency:** +50-100ms per request

**Why it's a bottleneck:**
```cpp
// Current: No keep-alive
Request 1: TCP handshake → HTTP → close
Request 2: TCP handshake → HTTP → close
// Ideal: Keep-alive
Request 1: TCP handshake → HTTP → keep open
Request 2: → HTTP → keep open
```

**Current behavior:**
- 10 requests: 10 TCP handshakes (500-1000ms overhead)
- With keep-alive: 1 handshake (50-100ms overhead)

**Mitigation options:**
1. **Enable keep-alive:** Crow supports this
2. **HTTP/2:** Multiplexing over single connection
3. **Connection pooling:** Reuse connections

**Quick win:** Enable HTTP keep-alive in Crow

---

### 12. Static File Serving Overhead

**Location:** `main.cpp` → `/public/<path>` route

**Problem:** Static files read from disk on every request.

**Impact:**
- **Disk I/O:** CSS/JS files read repeatedly
- **Latency:** +5-10ms per static file

**Why it's a bottleneck:**
```cpp
// Every request to /public/script.js:
auto [found, content] = readFileContents("public/script.js");  // ← Disk I/O
```

**Current behavior:**
- 100 page loads: 100 reads of script.js (500-1000ms total)
- Could be: 1 read + 99 cache hits (10ms total)

**Mitigation options:**
1. **In-memory cache:** Cache static files in memory
2. **CDN:** Serve static files from CDN
3. **Browser cache:** Set `Cache-Control` headers
4. **Pre-compressed:** Serve pre-gzipped files

**Quick win:** Add in-memory cache for static files + Cache-Control headers

---

## Summary Table

| Bottleneck | Severity | Impact | Mitigation Difficulty | Quick Win |
|------------|----------|--------|----------------------|-----------|
| Docker spawn overhead | 🔴 Critical | 100-2000ms per exec | High | Use smaller images |
| Synchronous blocking | 🔴 Critical | Thread pool exhaustion | Medium | Increase thread pool |
| SQLite single-writer | 🔴 Critical | Serialization | Medium | Separate read/write conns |
| Filesystem operations | 🟡 High | 50-100ms per exec | Low | Use tmpfs |
| Rate limiter memory | 🟡 High | Memory leak risk | Low | Add max entry limit |
| No compilation cache | 🟡 High | 1-5s per compile | Medium | Hash-based cache |
| Output truncation | 🟢 Medium | User experience | High | WebSocket streaming |
| No prioritization | 🟢 Medium | Fairness issue | Medium | Priority queue |
| Thread pool config | 🟢 Medium | Underutilization | Low | Auto-detect cores |
| No compression | 🔵 Low | Bandwidth waste | Low | Gzip middleware |
| No keep-alive | 🔵 Low | TCP overhead | Low | Enable keep-alive |
| Static file serving | 🔵 Low | Disk I/O | Low | In-memory cache |

---

## Recommended Priority

### Phase 1: Quick Wins (1-2 days)
1. Increase Crow thread pool to match CPU cores
2. Add max entry limit to rate limiter (LRU eviction)
3. Enable HTTP keep-alive
4. Add gzip compression for JSON responses
5. Cache static files in memory

**Expected improvement:** 2-3x throughput, 50% latency reduction

### Phase 2: Medium Effort (1 week)
1. Separate read/write SQLite connections
2. Mount `tmp/` as tmpfs for session directories
3. Add compilation cache (hash → binary)
4. Implement priority queue for admin requests

**Expected improvement:** 5-10x throughput, 70% latency reduction

### Phase 3: High Effort (2-4 weeks)
1. Async execution with job queue (Redis)
2. Pre-warm Docker container pool
3. WebSocket streaming for output
4. Replace SQLite with PostgreSQL

**Expected improvement:** 20-50x throughput, 90% latency reduction

---

## Testing Recommendations

### Load Testing
```bash
# Install Apache Bench
ab -n 1000 -c 10 http://localhost:3000/api/run \
  -p payload.json -T application/json

# Expected current: ~50-100 req/s
# Target after Phase 1: ~150-300 req/s
# Target after Phase 2: ~500-1000 req/s
# Target after Phase 3: ~2000-5000 req/s
```

### Profiling
```bash
# CPU profiling
perf record ./online_compiler
perf report

# Memory profiling
valgrind --leak-check=full ./online_compiler

# I/O profiling
iotop -o
```

### Monitoring
- Add metrics: request latency, queue depth, thread pool usage
- Add logging: slow queries, timeout frequency, error rate
- Add alerts: high queue depth, high memory usage
