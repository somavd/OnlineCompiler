const API_BASE = "http://172.17.162.152:3000";
// CodeMirror mode map
const MODES = {
  cpp: "text/x-c++src",
  python: "python",
  javascript: "javascript",
};

// Default code templates per language
const TEMPLATES = {
  cpp: '#include <iostream>\nusing namespace std;\n\nint main() {\n    cout << "Hello, World!" << endl;\n    return 0;\n}\n',
  python: 'print("Hello, World!")\n',
  javascript: 'console.log("Hello, World!");\n',
};

// Initialize CodeMirror
const editor = CodeMirror.fromTextArea(document.getElementById("editor"), {
  mode: MODES.cpp,
  theme: "dracula",
  lineNumbers: true,
  indentUnit: 4,
  tabSize: 4,
  indentWithTabs: false,
  lineWrapping: false,
  matchBrackets: true,
  autoCloseBrackets: true,
  extraKeys: {
    "Ctrl-Enter": function () { runCode(); },
    "Cmd-Enter": function () { runCode(); },
    Tab: function (cm) {
      cm.replaceSelection("    ", "end");
    },
  },
});

editor.setValue(TEMPLATES.cpp);

// Switch language mode
document.getElementById("language").addEventListener("change", function () {
  const lang = this.value;
  editor.setOption("mode", MODES[lang]);
  editor.setValue(TEMPLATES[lang] || "");
  editor.focus();
});

// Run code
async function runCode() {
  const language = document.getElementById("language").value;
  const code = editor.getValue();
  const input = document.getElementById("userInput").value;
  const stdoutEl = document.getElementById("stdout");
  const stderrEl = document.getElementById("stderr");
  const runBtn = document.getElementById("runBtn");
  const statusEl = document.getElementById("status");

  stdoutEl.textContent = "";
  stderrEl.textContent = "";
  statusEl.textContent = "";

  runBtn.disabled = true;
  runBtn.textContent = "Running...";
  const startTime = performance.now();

  try {
    const response = await fetch(`${API_BASE}/api/run`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ language, code, input }),
    });

    const text = await response.text();
    let data;
    try {
      data = JSON.parse(text);
    } catch {
      stderrEl.textContent = "Server returned invalid response: " + text.slice(0, 200);
      statusEl.textContent = "Error";
      statusEl.className = "status status-error";
      return;
    }
    const elapsed = ((performance.now() - startTime) / 1000).toFixed(2);

    if (!response.ok) {
      stderrEl.textContent = data.error || "Request failed";
      statusEl.textContent = "Failed";
      statusEl.className = "status status-error";
      return;
    }

    stdoutEl.textContent = data.stdout || "(no output)";
    stderrEl.textContent = data.stderr || "";

    if (data.timedOut) {
      stderrEl.textContent = "Execution timed out";
      statusEl.textContent = "Timed out";
      statusEl.className = "status status-error";
    } else if (data.exitCode !== 0) {
      statusEl.textContent = "Exit code: " + data.exitCode + " (" + elapsed + "s)";
      statusEl.className = "status status-error";
    } else {
      statusEl.textContent = "Done (" + elapsed + "s)";
      statusEl.className = "status status-ok";
    }
  } catch (error) {
    stderrEl.textContent = "Connection error: " + error.message;
    statusEl.textContent = "Error";
    statusEl.className = "status status-error";
  } finally {
    runBtn.disabled = false;
    runBtn.textContent = "\u25B6 Run";
    loadHistory();
  }
}

// Clear
function clearCode() {
  const lang = document.getElementById("language").value;
  editor.setValue(TEMPLATES[lang] || "");
  document.getElementById("userInput").value = "";
  document.getElementById("stdout").textContent = "";
  document.getElementById("stderr").textContent = "";
  document.getElementById("status").textContent = "";
  editor.focus();
}

// Cached submissions for click-to-load
let historyCache = [];

// Load submission history
async function loadHistory() {
  const listEl = document.getElementById("historyList");
  listEl.textContent = "Loading...";
  try {
    const res = await fetch(`${API_BASE}/api/submissions?limit=5`);
    const data = await res.json();
    historyCache = data.submissions || [];
    if (historyCache.length === 0) {
      listEl.textContent = "No submissions yet.";
      return;
    }
    // Build DOM safely — no innerHTML with user data
    listEl.innerHTML = "";
    historyCache.forEach(function (s, idx) {
      const status = s.timedOut ? "timeout" : s.exitCode === 0 ? "ok" : "error";
      const icon = status === "ok" ? "\u2705" : status === "timeout" ? "\u23F1" : "\u274C";
      const preview = s.code.split("\n")[0].slice(0, 50);

      const row = document.createElement("div");
      row.className = "history-item history-" + status;
      row.dataset.index = idx;
      row.addEventListener("click", function () { loadSubmission(idx); });

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

// Load history on page load
loadHistory();
