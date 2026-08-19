# Documentation Index

Complete guide to the Online Compiler project architecture, implementation, and deployment.

## Quick Links

- **[ARCHITECTURE.md](ARCHITECTURE.md)** — System design, data flow, security model
- **[MODULES.md](MODULES.md)** — Module reference, dependencies, file organization
- **[EXECUTOR.md](EXECUTOR.md)** — Code execution engine (Docker, process management, I/O)
- **[DATABASE.md](DATABASE.md)** — SQLite persistence (submissions, questions, schema)
- **[FRONTEND.md](FRONTEND.md)** — Web UI (auth, editor, admin dashboard)
- **[DEPLOYMENT.md](DEPLOYMENT.md)** — Local dev, Docker, production setup
- **[README.md](README.md)** — LaTeX compilation instructions

## Reading Guide

### For New Developers

1. Start with **[ARCHITECTURE.md](ARCHITECTURE.md)** for the big picture
2. Read **[MODULES.md](MODULES.md)** for component overview
3. Deep-dive into specific modules:
   - **[EXECUTOR.md](EXECUTOR.md)** if working on code execution
   - **[DATABASE.md](DATABASE.md)** if working on persistence
   - **[FRONTEND.md](FRONTEND.md)** if working on UI

### For DevOps / Deployment

1. Read **[DEPLOYMENT.md](DEPLOYMENT.md)** for setup instructions
2. Refer to **[ARCHITECTURE.md](ARCHITECTURE.md)** for security model
3. Check **[MODULES.md](MODULES.md)** for configuration options

### For Code Review

1. **[MODULES.md](MODULES.md)** — understand module responsibilities
2. **[EXECUTOR.md](EXECUTOR.md)** — review process/I/O handling
3. **[DATABASE.md](DATABASE.md)** — review thread safety, SQL
4. **[FRONTEND.md](FRONTEND.md)** — review XSS prevention, API calls

## Project Structure

```
OnlineCompiler/
├── README.md                    # Root pointer
├── docs/
│   ├── INDEX.md                 # This file
│   ├── ARCHITECTURE.md          # System design
│   ├── MODULES.md               # Module reference
│   ├── EXECUTOR.md              # Code execution
│   ├── DATABASE.md              # Persistence
│   ├── FRONTEND.md              # Web UI
│   ├── DEPLOYMENT.md            # Setup & deployment
│   ├── README.md                # LaTeX instructions
│   ├── project_report.tex       # Full design report (LaTeX)
│   └── screenshot.png           # UI screenshot
└── app/
    ├── README.md                # Build & run instructions
    ├── CMakeLists.txt           # Build configuration
    ├── Dockerfile               # Container image
    ├── docker-compose.yml       # Container orchestration
    ├── src/                     # C++ backend
    │   ├── main.cpp             # HTTP server
    │   ├── executor.hpp/cpp     # Code execution
    │   ├── database.hpp/cpp     # SQLite persistence
    │   ├── rate_limiter.hpp/cpp # Request throttling
    │   ├── config.hpp           # Configuration
    │   ├── validator.hpp        # Input validation
    │   └── languages.hpp        # Language configs
    └── public/                  # Frontend
        ├── index.html           # Entry point
        ├── login.html           # User login
        ├── signup.html          # User signup
        ├── admin-login.html     # Admin login
        ├── compiler.html        # Code editor
        ├── admin_dashboard.html # Admin panel
        ├── auth.js / auth.css   # Authentication
        ├── script.js            # Editor logic
        ├── compiler.css         # Editor styles
        ├── admin.js / admin.css # Admin logic
        └── styles.css           # Shared styles
```

## Key Concepts

### Architecture

- **C++17 backend** with Crow web framework
- **Docker sandboxing** for code execution
- **SQLite database** for persistence
- **Static frontend** with client-side auth (demo only)
- **POSIX process control** for execution management

### Security

- **Network isolation**: Docker `--network none`
- **Filesystem isolation**: Docker `--read-only`
- **Resource limits**: Memory, CPU, PID caps
- **Timeout enforcement**: SIGKILL after N seconds
- **Input validation**: JSON schema checks
- **Rate limiting**: Per-IP token bucket
- **Path traversal prevention**: Canonical path checks
- **XSS prevention**: Safe DOM manipulation

### Performance

- **Compiled binary**: Sub-millisecond routing
- **Non-blocking I/O**: `select()` for concurrent reads
- **WAL mode**: Concurrent database reads
- **Output truncation**: 64KB cap per stream
- **Stale cleanup**: Periodic rate limiter purge

## Common Tasks

### Build & Run Locally

```bash
cd app
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./online_compiler
```

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for details.

### Execute Code

```bash
curl -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(42)"}'
```

See **[EXECUTOR.md](EXECUTOR.md)** for details.

### Manage Questions

```bash
# Create
curl -X POST http://localhost:3000/api/questions \
  -H 'Content-Type: application/json' \
  -d '{"title":"Two Sum","category":"Arrays","difficulty":"Easy"}'

# List
curl http://localhost:3000/api/questions

# Update
curl -X PUT http://localhost:3000/api/questions/1 \
  -H 'Content-Type: application/json' \
  -d '{"title":"Two Sum II","category":"DP","difficulty":"Medium"}'

# Delete
curl -X DELETE http://localhost:3000/api/questions/1
```

See **[DATABASE.md](DATABASE.md)** for schema details.

### Deploy with Docker

```bash
cd app
docker build -t online-compiler:latest .
docker run -d \
  -p 3000:3000 \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v compiler-data:/app/data \
  online-compiler:latest
```

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for production setup.

## Configuration

All settings are environment variables:

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | 3000 | Server port |
| `DOCKER_TIMEOUT_SECONDS` | 10 | Execution timeout |
| `DOCKER_MEMORY_LIMIT` | 128m | Container RAM |
| `DOCKER_CPU_LIMIT` | 0.5 | Container CPU cores |
| `DOCKER_PIDS_LIMIT` | 50 | Max processes |
| `RATE_LIMIT_WINDOW_SECONDS` | 60 | Rate limit window |
| `RATE_LIMIT_MAX_REQUESTS` | 10 | Requests per window |

See **[DEPLOYMENT.md](DEPLOYMENT.md)** for setup.

## API Reference

### POST /api/run

Execute code.

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

### GET /api/submissions

Retrieve submission history.

**Query parameters:**
- `limit` (default: 20, max: 100)
- `language` (optional, filter by language)

**Response:**
```json
{
  "submissions": [
    {
      "id": 1,
      "language": "python",
      "code": "print(42)",
      "input": "",
      "stdout": "42\n",
      "stderr": "",
      "exitCode": 0,
      "timedOut": false,
      "createdAt": "2026-07-26T10:00:00Z"
    }
  ]
}
```

### GET /api/questions

List all questions.

**Response:**
```json
{
  "questions": [
    {
      "id": 1,
      "title": "Two Sum",
      "category": "Arrays",
      "difficulty": "Easy"
    }
  ]
}
```

### POST /api/questions

Create a question.

**Request:**
```json
{
  "title": "Two Sum",
  "category": "Arrays",
  "difficulty": "Easy"
}
```

**Response:**
```json
{
  "success": true
}
```

### PUT /api/questions/<id>

Update a question.

**Request:**
```json
{
  "title": "Two Sum II",
  "category": "DP",
  "difficulty": "Medium"
}
```

**Response:**
```json
{
  "success": true
}
```

### DELETE /api/questions/<id>

Delete a question.

**Response:**
```json
{
  "success": true,
  "id": 1
}
```

## Testing

### Unit Tests

Currently no automated unit tests. See **[MODULES.md](MODULES.md)** for manual test commands.

### Integration Tests

```bash
# Start server
cd app/build
./online_compiler &

# Test code execution
curl -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"cpp","code":"#include <iostream>\nint main() { std::cout << 42; }"}'

# Test question CRUD
curl -X POST http://localhost:3000/api/questions \
  -H 'Content-Type: application/json' \
  -d '{"title":"Test","category":"Test","difficulty":"Easy"}'

curl http://localhost:3000/api/questions

# Kill server
pkill online_compiler
```

## Known Issues & Limitations

### Authentication
- **Client-side only**: Passwords in plaintext localStorage
- **Hardcoded admin**: Credentials visible in code
- **No HTTPS**: Credentials transmitted in clear text

### Database
- **Single file**: No replication or sharding
- **No transactions**: Multi-step operations not atomic
- **No migrations**: Schema changes require manual SQL

### Frontend
- **No offline support**: Requires server connection
- **No code persistence**: Only submission history saved
- **Limited languages**: C++, Python, JavaScript only

### Executor
- **Docker required**: Cannot run without Docker daemon
- **Output truncation**: Large outputs (>64KB) silently truncated
- **No streaming**: Output returned all at once

## Future Enhancements

- [ ] Real authentication (JWT, OAuth)
- [ ] More languages (Go, Rust, Java)
- [ ] Streaming output (WebSocket)
- [ ] Code sharing via URL
- [ ] Collaborative editing
- [ ] Memory/CPU profiling
- [ ] Custom Docker images
- [ ] Database migrations
- [ ] Automated tests
- [ ] API documentation (OpenAPI/Swagger)

## Contributing

1. Read **[ARCHITECTURE.md](ARCHITECTURE.md)** to understand the design
2. Check **[MODULES.md](MODULES.md)** for module responsibilities
3. Make changes following existing code style
4. Test locally before submitting
5. Update relevant documentation

## Support

For questions or issues:
1. Check the relevant documentation file
2. Review the code comments
3. Run manual tests (see **[MODULES.md](MODULES.md)**)
4. Check Docker logs: `docker logs compiler`

## License

ISC (see root README.md)
