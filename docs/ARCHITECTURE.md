# Architecture Overview

## System Design

The Online Compiler is a **web-based code execution platform** with a C++17 backend and a static HTML/JS frontend. It allows users to write and execute code in multiple languages (C++, Python, JavaScript) inside isolated Docker containers.

```
┌─────────────────────────────────────────────────────────────┐
│                     Browser (Frontend)                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ index.html   │  │ compiler.html│  │admin_dash.html│     │
│  │ (redirect)   │  │ (editor UI)  │  │(Q&A mgmt)    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│         ↓                  ↓                  ↓              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  JavaScript (auth.js, script.js, admin.js)          │  │
│  │  - Client-side auth (localStorage)                  │  │
│  │  - CodeMirror editor integration                    │  │
│  │  - API calls to backend                             │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↓ HTTP/REST
┌─────────────────────────────────────────────────────────────┐
│              C++ Backend (Crow Web Framework)                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  main.cpp: Route handlers & request orchestration   │  │
│  │  - GET  /              → serve index.html           │  │
│  │  - GET  /public/<path> → serve static files         │  │
│  │  - POST /api/run       → execute code               │  │
│  │  - GET  /api/submissions → retrieve history         │  │
│  │  - GET/POST/PUT/DELETE /api/questions → Q&A CRUD   │  │
│  └──────────────────────────────────────────────────────┘  │
│                            ↓                                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Core Modules (src/)                                │  │
│  │  ┌────────────────────────────────────────────────┐ │  │
│  │  │ executor.cpp: Docker container execution      │ │  │
│  │  │  - fork/execvp/waitpid POSIX syscalls        │ │  │
│  │  │  - select() for non-blocking pipe I/O        │ │  │
│  │  │  - SIGKILL timeout enforcement               │ │  │
│  │  └────────────────────────────────────────────────┘ │  │
│  │  ┌────────────────────────────────────────────────┐ │  │
│  │  │ database.cpp: SQLite persistence              │ │  │
│  │  │  - Submissions table (code + output history)  │ │  │
│  │  │  - Questions table (Q&A management)           │ │  │
│  │  │  - Thread-safe CRUD operations                │ │  │
│  │  └────────────────────────────────────────────────┘ │  │
│  │  ┌────────────────────────────────────────────────┐ │  │
│  │  │ rate_limiter.cpp: Per-IP request throttling   │ │  │
│  │  │  - Token bucket algorithm                     │ │  │
│  │  │  - Stale entry cleanup                        │ │  │
│  │  └────────────────────────────────────────────────┘ │  │
│  │  ┌────────────────────────────────────────────────┐ │  │
│  │  │ config.hpp: Environment-based configuration   │ │  │
│  │  │ validator.hpp: JSON request validation        │ │  │
│  │  │ languages.hpp: Language-specific config       │ │  │
│  │  └────────────────────────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↓ Docker API
┌─────────────────────────────────────────────────────────────┐
│              Docker Daemon (Sandboxing)                      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Ephemeral containers (gcc, python, node images)    │  │
│  │  - Network isolation (--network none)               │  │
│  │  - Read-only filesystem (--read-only)               │  │
│  │  - Resource limits (memory, CPU, PIDs)              │  │
│  │  - Auto-cleanup on exit                             │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

### Code Execution Flow

1. **User submits code** via the browser (POST `/api/run`)
2. **Validation** — check language, code length, input size
3. **Rate limiting** — enforce per-IP request throttle
4. **Execution** — call `executeCode()` with language config
5. **Docker spawn** — fork child process, exec `docker run` with:
   - Mounted session directory (code + input files)
   - Resource limits (memory, CPU, PIDs, timeout)
   - Network disabled
6. **I/O capture** — use `select()` to read stdout/stderr concurrently
7. **Timeout** — if child exceeds limit, send `SIGKILL`
8. **Cleanup** — close pipes, wait for child, remove session dir
9. **Storage** — save submission (code, output, exit code) to SQLite
10. **Response** — return JSON with stdout, stderr, exit code, timeout flag

### Question Management Flow

1. **Admin loads dashboard** → `loadQuestions()` fetches from `/api/questions`
2. **Create** → POST with title/category/difficulty → stored in SQLite
3. **Edit** → PUT `/api/questions/<id>` → updates row
4. **Delete** → DELETE `/api/questions/<id>` → removes row
5. **List** → GET `/api/questions` → returns all questions

## Key Design Decisions

### Why C++?
- **Performance**: Compiled binary, sub-millisecond routing
- **Control**: Direct POSIX syscalls for process/I/O management
- **Simplicity**: Single executable, no runtime dependencies

### Why Docker?
- **Isolation**: Network-disabled, read-only containers prevent escape
- **Resource limits**: Memory, CPU, PID caps prevent DoS
- **Ephemeral**: Auto-cleanup, no persistent state leaks

### Why select() for I/O?
- **Deadlock-free**: Read both stdout/stderr concurrently
- **Timeout-aware**: Can enforce execution limits
- **Portable**: Standard POSIX, works on Linux/macOS

### Why SQLite?
- **Embedded**: No separate database server
- **Persistent**: Survives container restarts
- **Thread-safe**: WAL mode + mutex protection
- **Simple**: Single file, easy backup/migration

### Why Crow?
- **Header-only**: No build complexity
- **Modern C++**: Leverages C++17 features
- **Lightweight**: Minimal dependencies (just Asio)

## Security Model

| Layer | Mechanism | Threat |
|-------|-----------|--------|
| **Network** | Docker `--network none` | Network exfiltration |
| **Filesystem** | Docker `--read-only` | File modification |
| **Resources** | Memory/CPU/PID limits | Resource exhaustion |
| **Timeout** | SIGKILL after N seconds | Infinite loops |
| **Input** | JSON schema validation | Malformed requests |
| **Rate limit** | Per-IP token bucket | Brute force / DoS |
| **Path traversal** | Canonical path check | Directory escape |

## Deployment

- **Single binary**: `app/build/online_compiler`
- **Docker image**: Multi-stage build, minimal runtime (Ubuntu 22.04 + Docker CLI)
- **Volumes**: Persistent SQLite DB at `/app/data/submissions.db`
- **Compose**: Orchestrates compiler service + Docker socket mount

See `docs/DEPLOYMENT.md` for detailed setup.
