# Module Reference

Quick reference for all backend modules and their responsibilities.

## Backend Modules (C++)

### main.cpp

**Purpose**: HTTP server entry point, route handlers, request orchestration

**Key responsibilities:**
- Initialize Crow web framework
- Load configuration from environment
- Set up signal handlers (SIGINT, SIGTERM)
- Define all HTTP routes
- Coordinate executor, database, rate limiter

**Routes:**
- `GET /` → serve `public/index.html`
- `GET /public/<path>` → serve static files (CSS, JS, HTML)
- `POST /api/run` → execute code
- `GET /api/submissions` → retrieve submission history
- `GET /api/questions` → list all questions
- `POST /api/questions` → create question
- `PUT /api/questions/<id>` → update question
- `DELETE /api/questions/<id>` → delete question
- `OPTIONS /api/<path>` → CORS preflight

**Dependencies:**
- `crow.h` (web framework)
- `executor.hpp` (code execution)
- `database.hpp` (persistence)
- `rate_limiter.hpp` (throttling)
- `config.hpp` (configuration)
- `validator.hpp` (input validation)

**Error handling:**
- 400: Bad request (validation failed)
- 404: Not found (route or resource)
- 413: Payload too large (body > 1MB)
- 429: Too many requests (rate limited)
- 500: Server error (execution failed)

---

### executor.hpp / executor.cpp

**Purpose**: Execute user code in Docker containers with resource limits

**Key functions:**
- `isDockerAvailable()` → check Docker daemon
- `executeCode(language, code, input, config)` → run code, return result

**Key structures:**
- `ExecResult` → stdout, stderr, exit code, timeout flag

**Implementation details:**
- Fork child process
- Exec `docker run` with language-specific image
- Use `select()` for non-blocking I/O on stdout/stderr
- Enforce timeout with `SIGKILL`
- Truncate output at 64KB
- Clean up session directory

**Configuration:**
- `DOCKER_TIMEOUT_SECONDS` (default: 10)
- `DOCKER_MEMORY_LIMIT` (default: 128m)
- `DOCKER_CPU_LIMIT` (default: 0.5)
- `DOCKER_PIDS_LIMIT` (default: 50)

**Security:**
- Network isolation (`--network none`)
- Read-only filesystem (`--read-only`)
- Resource limits (memory, CPU, PIDs)
- Timeout enforcement (SIGKILL)

---

### database.hpp / database.cpp

**Purpose**: Persistent storage for submissions and questions using SQLite

**Key structures:**
- `Submission` → code, output, exit code, timestamp
- `Question` → title, category, difficulty

**Key methods:**
- `saveSubmission(sub)` → insert, return ID
- `getSubmissions(limit, language)` → retrieve history
- `addQuestion(q)` → insert question
- `getQuestions()` → retrieve all questions
- `updateQuestion(q)` → update by ID
- `deleteQuestion(id)` → delete by ID

**Thread safety:**
- All methods protected by `std::mutex`
- Lock guard pattern (RAII)

**Database features:**
- WAL mode (concurrent reads)
- Indexes on language and creation time
- Parameterized queries (SQL injection prevention)
- Auto-generated IDs and timestamps

**Schema:**
- `submissions` table (code, output, metadata)
- `questions` table (title, category, difficulty)

---

### rate_limiter.hpp / rate_limiter.cpp

**Purpose**: Per-IP request throttling using token bucket algorithm

**Key methods:**
- `allow(ip_address)` → return true if request allowed
- `purgeStale()` → clean up old IP entries

**Algorithm:**
- Token bucket: each IP gets N tokens per window
- Refill every window seconds
- Stale entries (>2x window old) removed periodically

**Configuration:**
- `RATE_LIMIT_WINDOW_SECONDS` (default: 60)
- `RATE_LIMIT_MAX_REQUESTS` (default: 10)

**Thread safety:**
- Protected by `std::mutex`
- Safe for concurrent requests

---

### config.hpp

**Purpose**: Load and validate configuration from environment variables

**Key structure:**
- `Config` struct with all settings
- `Config::fromEnv()` static method

**Configuration variables:**
- `PORT` (default: 3000)
- `DOCKER_TIMEOUT_SECONDS` (default: 10)
- `DOCKER_MEMORY_LIMIT` (default: 128m)
- `DOCKER_CPU_LIMIT` (default: 0.5)
- `DOCKER_PIDS_LIMIT` (default: 50)
- `RATE_LIMIT_WINDOW_SECONDS` (default: 60)
- `RATE_LIMIT_MAX_REQUESTS` (default: 10)

**Validation:**
- Range checks (e.g., port 1-65535)
- Fallback to defaults if invalid
- Warning logged for invalid values

---

### validator.hpp

**Purpose**: Validate incoming JSON requests

**Key function:**
- `validateRunRequest(json)` → parse and validate code execution request

**Validation rules:**
- `language` required, must be "cpp", "python", or "javascript"
- `code` required, non-empty
- `input` optional, defaults to empty string
- All fields must be strings

**Error messages:**
- "Missing or invalid 'language' field"
- "Missing or invalid 'code' field"
- etc.

---

### languages.hpp

**Purpose**: Language-specific configuration (Docker image, compile/run commands)

**Key structure:**
- `LangConfig` struct with image, file name, compile command, run command
- `getLanguages()` function returning map of language configs

**Supported languages:**
- `cpp` → gcc:latest, g++ compiler
- `python` → python:3-slim, direct interpreter
- `javascript` → node:20-slim, direct interpreter

**Example:**
```cpp
{
    "image": "gcc:latest",
    "filename": "source.cpp",
    "compileCmd": "g++ -O2 source.cpp -o bin",
    "runCmd": "./bin"
}
```

---

## Frontend Modules (JavaScript)

### auth.js

**Purpose**: User and admin authentication

**Key functions:**
- `signup()` → register new user
- `login()` → authenticate user
- `adminLogin()` → authenticate admin
- `logout()` → clear session

**Storage:**
- Users stored in `localStorage` (plaintext passwords)
- Admin credentials hardcoded

**Session data:**
- `loggedIn` → "true" or "false"
- `role` → "user" or "admin"
- `currentUser` → email address

---

### script.js

**Purpose**: Code editor UI and execution

**Key functions:**
- `runCode()` → POST code to `/api/run`, display output
- `clearCode()` → reset editor
- `loadHistory()` → fetch submission history
- `loadSubmission(idx)` → load submission from history

**Dependencies:**
- CodeMirror (CDN-loaded)
- Dracula theme

**Features:**
- Syntax highlighting
- Ctrl+Enter to run
- Auto-closing brackets
- Submission history panel

---

### admin.js

**Purpose**: Admin dashboard for question management

**Key functions:**
- `loadQuestions()` → fetch from `/api/questions`
- `saveQuestion()` → POST (create) or PUT (update)
- `renderQuestions()` → build table with safe DOM
- `deleteQuestion(id)` → DELETE from backend
- `editQuestion(id)` → populate form
- `clearQuestionForm()` → reset form

**Features:**
- Full CRUD for questions
- Backend-persisted (SQLite)
- Safe DOM rendering (no XSS)
- Confirmation dialogs

---

## Dependency Graph

```
main.cpp
├── crow.h
├── config.hpp
├── executor.hpp
│   └── languages.hpp
├── database.hpp
├── rate_limiter.hpp
├── validator.hpp
└── [standard library]

executor.cpp
├── executor.hpp
├── languages.hpp
└── [POSIX syscalls, filesystem]

database.cpp
├── database.hpp
├── sqlite3.h
└── [standard library]

rate_limiter.cpp
├── rate_limiter.hpp
└── [standard library]
```

## Build Dependencies

- **CMake** 3.14+ (build system)
- **C++17 compiler** (g++ 9+ or clang++ 10+)
- **Crow** (auto-fetched via FetchContent)
- **Asio** (auto-fetched via Crow)
- **SQLite** (auto-fetched via FetchContent)
- **Docker** (runtime, for code execution)

## Runtime Dependencies

- **Docker daemon** (must be running)
- **Docker images**: gcc:latest, python:3-slim, node:20-slim
- **Linux kernel** (for POSIX syscalls)

## File Organization

```
app/
├── src/
│   ├── main.cpp
│   ├── executor.hpp / executor.cpp
│   ├── database.hpp / database.cpp
│   ├── rate_limiter.hpp / rate_limiter.cpp
│   ├── config.hpp
│   ├── validator.hpp
│   └── languages.hpp
├── public/
│   ├── index.html
│   ├── login.html / signup.html / admin-login.html
│   ├── compiler.html / admin_dashboard.html
│   ├── auth.js / auth.css
│   ├── script.js / compiler.css
│   ├── admin.js / admin.css
│   └── styles.css
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
└── README.md
```

## Testing Checklist

- [ ] Build succeeds from `app/build/`
- [ ] Server starts without errors
- [ ] Static files served (HTML, CSS, JS)
- [ ] Code execution works (all 3 languages)
- [ ] Submission history persists
- [ ] Admin CRUD works (create, read, update, delete)
- [ ] Rate limiting blocks excessive requests
- [ ] Timeout kills long-running code
- [ ] Docker isolation prevents escapes
- [ ] CORS headers present on API responses
