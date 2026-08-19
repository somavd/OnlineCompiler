# Executor Module

**File**: `app/src/executor.hpp` / `app/src/executor.cpp`

## Purpose

Executes user-submitted code inside isolated Docker containers with resource limits and timeout enforcement. Handles process spawning, I/O capture, and cleanup.

## Public Interface

```cpp
struct ExecResult {
    std::string stdoutStr;      // Program output (capped at 64KB)
    std::string stderrStr;      // Error output (capped at 64KB)
    int exitCode = 0;           // Process exit code
    bool timedOut = false;      // True if execution exceeded timeout
};

bool isDockerAvailable();       // Check if Docker daemon is reachable

ExecResult executeCode(
    const std::string& language,  // "cpp", "python", "javascript"
    const std::string& code,      // Source code to execute
    const std::string& input,     // Stdin (optional)
    const Config& config          // Timeout, memory, CPU limits
);
```

## Implementation Details

### Session Management

Each execution creates a **temporary session directory** (`tmp/<session-id>/`):

```
tmp/a1b2c3d4e5f6/
├── source.cpp          (or .py, .js)
├── input.txt           (if stdin provided)
└── [compiled binary]   (if language requires compilation)
```

Session ID is a **random 128-bit hex string** (thread-safe via static mutex + MT19937).

### Docker Invocation

For C++:
```bash
docker run --rm \
  --network none \
  --read-only \
  --memory 128m \
  --cpus 0.5 \
  --pids-limit 50 \
  -v /path/to/session:/code \
  gcc:latest \
  bash -c "cd /code && g++ -O2 source.cpp -o bin && ./bin < input.txt"
```

**Flags explained:**
- `--rm`: Auto-delete container on exit
- `--network none`: No network access (prevents exfiltration)
- `--read-only`: Filesystem is read-only (prevents persistence)
- `--memory 128m`: RAM cap (default, configurable)
- `--cpus 0.5`: CPU core limit (default, configurable)
- `--pids-limit 50`: Max process count (prevents fork bombs)
- `-v /path/to/session:/code`: Mount session dir as `/code` inside container

### I/O Capture (select()-based)

The executor spawns a **child process** (`docker run`) and reads both stdout/stderr **concurrently** using `select()`:

```
┌─────────────────────────────────────────┐
│ Parent Process (executor)               │
│                                         │
│  fork()                                 │
│    ↓                                    │
│  ┌─────────────────────────────────┐   │
│  │ Child (docker run)              │   │
│  │ stdout → pipe[1]                │   │
│  │ stderr → pipe[1]                │   │
│  └─────────────────────────────────┘   │
│    ↓                                    │
│  select(stdout_fd, stderr_fd, timeout) │
│    ↓                                    │
│  read() from ready pipes               │
│  append to outStr / errStr             │
│    ↓                                    │
│  waitpid() for child exit              │
│                                         │
└─────────────────────────────────────────┘
```

**Why `select()`?**
- Avoids deadlock: if child writes >64KB to stdout, parent can still read stderr
- Timeout-aware: `select()` respects the timeout parameter
- Portable: Standard POSIX, works on Linux/macOS

**Output truncation:**
- Each stream capped at 64KB (`MAX_OUTPUT_BYTES`)
- If exceeded, appends `"\n...[output truncated]"`

### Timeout Enforcement

1. **Calculate deadline**: `start_time + timeout_seconds`
2. **select() loop**: Each iteration checks remaining time
3. **If timeout exceeded**: 
   - Send `SIGKILL` to child process
   - Set `result.timedOut = true`
   - Continue reading any buffered output
4. **Return**: Result with timeout flag set

**Code snippet:**
```cpp
auto deadline = std::chrono::steady_clock::now() + 
                std::chrono::seconds(timeoutSeconds);

while (!outDone || !errDone) {
    auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining.count() <= 0) {
        kill(pid, SIGKILL);
        timedOut = true;
        break;
    }
    // select() with remaining time...
}
```

### Error Handling

| Scenario | Behavior |
|----------|----------|
| Pipe creation fails | Return error, exit code 1 |
| Fork fails | Return error, exit code 1 |
| Docker not available | `isDockerAvailable()` returns false (checked in main) |
| Compilation error | Captured in stderr, exit code != 0 |
| Runtime error | Captured in stderr, exit code != 0 |
| Timeout | `timedOut = true`, partial output returned |
| Output exceeds 64KB | Truncated with `...[output truncated]` message |

### Cleanup

After execution:
1. Close all pipe file descriptors
2. Wait for child process with `waitpid()`
3. Delete session directory recursively (`fs::remove_all()`)

**Exception safety**: Uses `std::lock_guard` for RNG mutex; filesystem operations wrapped in error code checks.

## Configuration

Executor respects these environment variables (via `Config`):

| Variable | Default | Purpose |
|----------|---------|---------|
| `DOCKER_TIMEOUT_SECONDS` | 10 | Max execution time |
| `DOCKER_MEMORY_LIMIT` | 128m | Container RAM cap |
| `DOCKER_CPU_LIMIT` | 0.5 | Container CPU cores |
| `DOCKER_PIDS_LIMIT` | 50 | Max processes in container |

## Performance Characteristics

- **Startup**: ~500ms (Docker image pull on first run, then ~100ms)
- **I/O**: Non-blocking via `select()`, minimal latency
- **Memory**: Parent process ~5MB, child container ~50-100MB
- **Cleanup**: ~50ms (filesystem removal)

## Testing

Manual test:
```bash
cd app/build
./online_compiler &
curl -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(42)","input":""}'
```

Expected response:
```json
{
  "stdout": "42\n",
  "stderr": "",
  "exitCode": 0,
  "timedOut": false
}
```

## Known Limitations

1. **Docker required**: Cannot execute without Docker daemon
2. **Single-threaded execution**: One code execution at a time per request (but Crow handles concurrent requests via thread pool)
3. **No persistent state**: Session dirs deleted after execution
4. **Output truncation**: Large outputs (>64KB) are silently truncated
5. **No interactive input**: stdin is provided upfront, no interactive prompts

## Future Enhancements

- [ ] Support more languages (Go, Rust, Java)
- [ ] Streaming output (WebSocket) for long-running code
- [ ] Memory/CPU profiling
- [ ] Custom Docker images per language
- [ ] Persistent session storage (for debugging)
