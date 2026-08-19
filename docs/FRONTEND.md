# Frontend Module

**Files**: `app/public/` (HTML, CSS, JavaScript)

## Overview

The frontend is a **static HTML/CSS/JavaScript** application served by the C++ backend. It provides:
- User authentication (client-side, localStorage-based)
- Code editor with syntax highlighting (CodeMirror)
- Code execution interface
- Submission history viewer
- Admin dashboard for question management

## Architecture

```
public/
├── index.html              # Entry point (role-based redirect)
├── login.html              # User login form
├── signup.html             # User registration form
├── admin-login.html        # Admin login form
├── compiler.html           # Main code editor UI
├── admin_dashboard.html    # Admin Q&A management panel
├── auth.js                 # Authentication logic
├── auth.css                # Auth page styling
├── script.js               # Code editor & execution logic
├── compiler.css            # Editor UI styling
├── admin.js                # Admin dashboard logic
├── admin.css               # Admin UI styling
└── styles.css              # Shared styles
```

## Authentication (auth.js)

**⚠️ WARNING: Client-side auth is for demo only. NOT production-ready.**

### User Signup

```javascript
function signup() {
    const name = document.getElementById("name").value;
    const email = document.getElementById("email").value;
    const password = document.getElementById("password").value;
    
    if (!name || !email || !password) {
        alert("Please fill all fields");
        return;
    }
    
    // Store user in localStorage (plaintext!)
    const user = { name, email, password };
    localStorage.setItem(email, JSON.stringify(user));
    
    alert("Signup Successful");
    location.href = "login.html";
}
```

**Issues:**
- Passwords stored in plaintext in browser storage
- No server-side validation
- No email verification
- No password hashing

### User Login

```javascript
function login() {
    const email = document.getElementById("email").value;
    const password = document.getElementById("password").value;
    
    // Retrieve from localStorage
    const user = JSON.parse(localStorage.getItem(email));
    
    if (user && user.password === password) {
        localStorage.setItem("loggedIn", "true");
        localStorage.setItem("role", "user");
        localStorage.setItem("currentUser", email);
        location.href = "compiler.html";
    } else {
        alert("Invalid Credentials");
    }
}
```

### Admin Login

```javascript
function adminLogin() {
    const email = document.getElementById("adminEmail").value;
    const password = document.getElementById("adminPassword").value;
    
    // Hardcoded credentials
    if (email === "admin@gmail.com" && password === "admin123") {
        localStorage.setItem("loggedIn", "true");
        localStorage.setItem("role", "admin");
        localStorage.setItem("currentUser", email);
        location.href = "admin_dashboard.html";
    } else {
        alert("Invalid Admin Credentials");
    }
}
```

**Issues:**
- Hardcoded admin credentials
- No rate limiting on login attempts
- Credentials visible in browser dev tools

### Logout

```javascript
function logout() {
    localStorage.clear();
    location.href = "login.html";
}
```

## Code Editor (script.js)

### Initialization

```javascript
const MODES = {
    cpp: "text/x-c++src",
    python: "python",
    javascript: "javascript",
};

const TEMPLATES = {
    cpp: '#include <iostream>\n...',
    python: 'print("Hello, World!")\n',
    javascript: 'console.log("Hello, World!");\n',
};

// Initialize CodeMirror editor
const editor = CodeMirror.fromTextArea(
    document.getElementById("editor"),
    {
        mode: MODES.cpp,
        theme: "dracula",
        lineNumbers: true,
        indentUnit: 4,
        tabSize: 4,
        matchBrackets: true,
        autoCloseBrackets: true,
        extraKeys: {
            "Ctrl-Enter": runCode,
            "Cmd-Enter": runCode,
        },
    }
);

editor.setValue(TEMPLATES.cpp);
```

**Features:**
- Syntax highlighting via CodeMirror (CDN-loaded)
- Dracula dark theme
- Auto-closing brackets
- Ctrl+Enter / Cmd+Enter to run

### Code Execution

```javascript
async function runCode() {
    const language = document.getElementById("language").value;
    const code = editor.getValue();
    const input = document.getElementById("userInput").value;
    
    // Clear previous output
    document.getElementById("stdout").textContent = "";
    document.getElementById("stderr").textContent = "";
    
    // Disable button, show "Running..."
    const runBtn = document.getElementById("runBtn");
    runBtn.disabled = true;
    runBtn.textContent = "Running...";
    
    const startTime = performance.now();
    
    try {
        // POST to /api/run
        const response = await fetch(`${API_BASE}/api/run`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ language, code, input }),
        });
        
        // Parse response (with error handling for non-JSON)
        const text = await response.text();
        let data;
        try {
            data = JSON.parse(text);
        } catch {
            document.getElementById("stderr").textContent = 
                "Server returned invalid response: " + text.slice(0, 200);
            return;
        }
        
        const elapsed = ((performance.now() - startTime) / 1000).toFixed(2);
        
        if (!response.ok) {
            document.getElementById("stderr").textContent = 
                data.error || "Request failed";
            return;
        }
        
        // Display output
        document.getElementById("stdout").textContent = 
            data.stdout || "(no output)";
        document.getElementById("stderr").textContent = 
            data.stderr || "";
        
        // Update status
        const statusEl = document.getElementById("status");
        if (data.timedOut) {
            statusEl.textContent = "Timed out";
            statusEl.className = "status status-error";
        } else if (data.exitCode !== 0) {
            statusEl.textContent = `Exit code: ${data.exitCode} (${elapsed}s)`;
            statusEl.className = "status status-error";
        } else {
            statusEl.textContent = `Done (${elapsed}s)`;
            statusEl.className = "status status-ok";
        }
    } catch (error) {
        document.getElementById("stderr").textContent = 
            "Connection error: " + error.message;
    } finally {
        runBtn.disabled = false;
        runBtn.textContent = "▶ Run";
        loadHistory();  // Refresh submission history
    }
}
```

**Key points:**
- Uses `fetch()` API (modern, promise-based)
- Handles both successful and error responses
- Catches JSON parse errors gracefully
- Measures execution time client-side
- Refreshes history after each run
- Displays timeout/exit code status

### Submission History

```javascript
let historyCache = [];

async function loadHistory() {
    const listEl = document.getElementById("historyList");
    listEl.textContent = "Loading...";
    
    try {
        const res = await fetch(`${API_BASE}/api/submissions?limit=10`);
        const data = await res.json();
        historyCache = data.submissions || [];
        
        if (historyCache.length === 0) {
            listEl.textContent = "No submissions yet.";
            return;
        }
        
        // Build DOM safely (no innerHTML with user data)
        listEl.innerHTML = "";
        historyCache.forEach((s, idx) => {
            const status = s.timedOut ? "timeout" : 
                          s.exitCode === 0 ? "ok" : "error";
            const icon = status === "ok" ? "✅" : 
                        status === "timeout" ? "⏱" : "❌";
            const preview = s.code.split("\n")[0].slice(0, 50);
            
            const row = document.createElement("div");
            row.className = `history-item history-${status}`;
            row.dataset.index = idx;
            row.addEventListener("click", () => loadSubmission(idx));
            
            // Build cells with textContent (XSS-safe)
            const iconSpan = document.createElement("span");
            iconSpan.className = "history-icon";
            iconSpan.textContent = icon;
            
            const langSpan = document.createElement("span");
            langSpan.className = "history-lang";
            langSpan.textContent = s.language;
            
            const previewSpan = document.createElement("span");
            previewSpan.className = "history-preview";
            previewSpan.textContent = preview;
            
            const timeSpan = document.createElement("span");
            timeSpan.className = "history-time";
            timeSpan.textContent = s.createdAt;
            
            row.appendChild(iconSpan);
            row.appendChild(langSpan);
            row.appendChild(previewSpan);
            row.appendChild(timeSpan);
            listEl.appendChild(row);
        });
    } catch (e) {
        listEl.textContent = "Failed to load history.";
    }
}

function loadSubmission(idx) {
    const s = historyCache[idx];
    if (!s) return;
    
    document.getElementById("language").value = s.language;
    editor.setOption("mode", MODES[s.language]);
    editor.setValue(s.code);
    document.getElementById("userInput").value = s.input || "";
    document.getElementById("stdout").textContent = s.stdout || "";
    document.getElementById("stderr").textContent = s.stderr || "";
}
```

**Security:**
- Uses `createElement()` + `textContent` (not `innerHTML`)
- Stores submissions in `historyCache` array
- Loads submission by index (not by parsing JSON in onclick)
- Prevents XSS via user-controlled code/output

## Admin Dashboard (admin.js)

### Question Management

```javascript
const API_BASE = "http://172.17.162.152:3000";

async function loadQuestions() {
    try {
        const res = await fetch(`${API_BASE}/api/questions`);
        const data = await res.json();
        questions = data.questions || [];
    } catch (e) {
        alert("Failed to load questions from server.");
    }
    renderQuestions();
    updateCounts();
}

async function saveQuestion() {
    const title = document.getElementById("title").value.trim();
    const category = document.getElementById("category").value.trim();
    const difficulty = document.getElementById("difficulty").value;
    
    if (!title) {
        alert("Title required");
        return;
    }
    
    const payload = { title, category, difficulty };
    
    try {
        let res;
        if (editId === null) {
            // POST (create)
            res = await fetch(`${API_BASE}/api/questions`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload),
            });
        } else {
            // PUT (update)
            res = await fetch(`${API_BASE}/api/questions/${editId}`, {
                method: "PUT",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload),
            });
        }
        
        const data = await res.json();
        if (!res.ok || !data.success) {
            alert(data.error || "Failed to save question");
            return;
        }
    } catch (e) {
        alert("Server error while saving question.");
        return;
    }
    
    clearQuestionForm();
    await loadQuestions();
}

function renderQuestions() {
    const table = document.getElementById("questionTable");
    table.innerHTML = "";
    
    questions.forEach((q) => {
        const row = document.createElement("tr");
        
        // Safe text cells
        function textCell(value) {
            const td = document.createElement("td");
            td.textContent = value == null ? "" : String(value);
            return td;
        }
        
        row.appendChild(textCell(q.id));
        row.appendChild(textCell(q.title));
        row.appendChild(textCell(q.difficulty));
        row.appendChild(textCell(q.category));
        
        const actions = document.createElement("td");
        const editBtn = document.createElement("button");
        editBtn.textContent = "Edit";
        editBtn.addEventListener("click", () => editQuestion(q.id));
        
        const deleteBtn = document.createElement("button");
        deleteBtn.textContent = "Delete";
        deleteBtn.addEventListener("click", () => deleteQuestion(q.id));
        
        actions.appendChild(editBtn);
        actions.appendChild(deleteBtn);
        row.appendChild(actions);
        
        table.appendChild(row);
    });
}

async function deleteQuestion(id) {
    if (!confirm("Delete this question?")) return;
    
    try {
        const res = await fetch(`${API_BASE}/api/questions/${id}`, {
            method: "DELETE"
        });
        const data = await res.json();
        if (!data.success) {
            alert("Failed to delete question");
            return;
        }
    } catch (e) {
        alert("Server error while deleting question.");
        return;
    }
    
    if (editId === id) clearQuestionForm();
    await loadQuestions();
}
```

**Key points:**
- Wired to `/api/questions` backend (single source of truth)
- Full CRUD: Create (POST), Read (GET), Update (PUT), Delete (DELETE)
- Safe DOM rendering with `createElement()` + `textContent`
- Confirmation dialog before delete

## Styling

### Themes

- **Dark theme**: Dracula color scheme (dark background, bright accents)
- **Responsive**: Flexbox layout, mobile-friendly
- **Accessible**: Sufficient color contrast, semantic HTML

### Key CSS Classes

| Class | Purpose |
|-------|---------|
| `.container` | Max-width wrapper (1200px) |
| `.panel` | Card-like container (border, rounded) |
| `.panel-editor` | Code editor panel (flex: 3) |
| `.panel-output` | Output panel (flex: 2) |
| `.status-ok` | Green text (success) |
| `.status-error` | Red text (error) |
| `.history-item` | Submission list item (clickable) |
| `.history-ok` | Green border (success) |
| `.history-error` | Red border (error) |

## Configuration

**API_BASE** (in `script.js` and `admin.js`):
```javascript
const API_BASE = "http://172.17.162.152:3000";
```

Change this to:
- `""` if serving frontend from the same C++ server
- `"http://localhost:3000"` for local development
- Your production server URL for deployment

## Security Considerations

| Issue | Severity | Mitigation |
|-------|----------|-----------|
| Plaintext passwords in localStorage | **CRITICAL** | Use server-side auth (JWT, sessions) |
| Hardcoded admin credentials | **CRITICAL** | Use database + password hashing |
| No HTTPS | **HIGH** | Deploy with TLS/SSL |
| XSS via user code | **MEDIUM** | Using `textContent` instead of `innerHTML` |
| CSRF | **MEDIUM** | Add CSRF tokens to forms |
| No input validation | **MEDIUM** | Validate on both client and server |

## Testing

Manual test:
```bash
# Start server
cd app/build
./online_compiler &

# Open browser
open http://localhost:3000

# Signup: test@example.com / password
# Login and run code
# Admin login: admin@gmail.com / admin123
# Create/edit/delete questions
```

## Known Limitations

1. **No real authentication**: Credentials in localStorage
2. **No offline support**: Requires server connection
3. **No code persistence**: Only submission history saved
4. **Limited language support**: C++, Python, JavaScript only
5. **No collaborative editing**: Single-user per session

## Future Enhancements

- [ ] Real authentication (JWT, OAuth)
- [ ] Service Worker for offline support
- [ ] Code snippets/templates library
- [ ] Collaborative editing (WebSocket)
- [ ] Dark/light theme toggle
- [ ] Code sharing via URL
- [ ] Syntax error highlighting
- [ ] Code formatting (Prettier)
