# Online Compiler

A high-performance web-based code compiler written in **C++17** using the [Crow](https://crowcpp.org/) web framework. Executes user code inside Docker containers with resource limits and timeout enforcement.

## Features

- **C++ backend** — Compiled server binary, sub-millisecond routing, native multithreading
- **Multi-language execution** — C++, Python, JavaScript
- **Docker sandboxing** — Isolated containers with `--network none`, `--read-only`, memory/CPU/PID limits
- **POSIX execution engine** — Direct `fork/execvp/waitpid` with `SIGKILL` timeout, `select()`-based pipe I/O (no deadlocks)
- **Rate limiting** — Thread-safe per-IP with automatic stale entry cleanup
- **CodeMirror editor** — Syntax highlighting, Dracula theme, Ctrl+Enter to run
- **Graceful shutdown** — Handles SIGINT/SIGTERM cleanly
- **Input validation** — Rejects malformed requests before any execution
- **Output truncation** — Caps stdout/stderr at 64KB

## Tech Stack

- **Backend**: C++17, Crow web framework, POSIX syscalls
- **Build**: CMake 3.14+ with FetchContent (auto-downloads Crow & Asio)
- **Frontend**: HTML/CSS/JS with CodeMirror 5 (loaded via CDN)
- **Execution**: Docker containers (`gcc:latest`, `python:3-slim`, `node:20-slim`)

## Requirements

- C++17 compiler (g++ 9+ / clang++ 10+)
- CMake 3.14+
- Docker installed and running
- Current user in the `docker` group (no sudo needed)
- Internet connection (first build downloads Crow & Asio; frontend loads CodeMirror from CDN)

## Build & Run

```bash
# Clone
git clone <repository-url>
cd OnlineCompiler

# Build (first build takes ~60s to fetch dependencies)
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run (must run from build/ directory)
./online_compiler
```

Open **http://localhost:3000** in your browser.

> **Important:** The server looks for `public/` relative to the working directory.
> CMake copies `public/` into `build/` automatically, so always run from the `build/` directory.

### macOS Notes

- On macOS, use `make -j$(sysctl -n hw.ncpu)` instead of `make -j$(nproc)`.
- If using Colima for Docker, ensure it is running: `colima start`

### Quick Test (without browser)

```bash
# Health check — should return the HTML page
curl -s http://localhost:3000 | head -5

# Run Python code
curl -s -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(\"hello\")"}'
```

## API

### POST /api/run

**Request:**
```json
{
  "language": "python",
  "code": "print('hello')",
  "input": "optional stdin"
}
```

**Response:**
```json
{
  "stdout": "hello\n",
  "stderr": "",
  "exitCode": 0,
  "timedOut": false
}
```

**Supported languages:** `cpp`, `python`, `javascript`

**Error responses** (400/413/429):
```json
{
  "error": "Missing or invalid 'language' field"
}
```

## Project Structure

```
OnlineCompiler/
├── src/
│   ├── main.cpp             # Crow app, routes, signal handling, static serving
│   ├── executor.hpp/cpp     # Docker runner (fork/exec, select(), timeout, cleanup)
│   ├── languages.hpp        # Language config (image, compile/run commands)
│   ├── config.hpp           # Env-based configuration with safe parsing
│   ├── validator.hpp        # JSON request validation
│   └── rate_limiter.hpp/cpp # Thread-safe rate limiter with stale purge
├── public/
│   ├── index.html           # CodeMirror editor UI
│   ├── styles.css           # Dark theme, side-by-side layout
│   └── script.js            # Editor init, language switching, API calls
├── CMakeLists.txt           # Build system (auto-fetches Crow + Asio)
├── .env.example
├── .gitignore
└── README.md
```

## Configuration (environment variables)

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | Server port |
| `DOCKER_TIMEOUT_SECONDS` | `10` | Max execution time per request |
| `DOCKER_MEMORY_LIMIT` | `128m` | Container memory cap |
| `DOCKER_CPU_LIMIT` | `0.5` | CPU cores allowed per container |
| `DOCKER_PIDS_LIMIT` | `50` | Max processes in container |
| `RATE_LIMIT_MAX_REQUESTS` | `10` | Requests per window per IP |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Rate limit window (seconds) |

Set these in your shell or copy `.env.example` (note: the C++ server reads `std::getenv()`, not `.env` files directly — export them or use `env $(cat .env) ./online_compiler`).

## Stopping the Server

Press **Ctrl+C** — the server handles SIGINT gracefully and shuts down cleanly.

## License

ISC