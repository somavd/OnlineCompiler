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

// Load submission history
async function loadHistory() {
  const listEl = document.getElementById("historyList");
  listEl.innerHTML = "<em>Loading...</em>";
  try {
    const res = await fetch(`${API_BASE}/api/submissions?limit=5`);
    const data = await res.json();
    if (!data.submissions || data.submissions.length === 0) {
      listEl.innerHTML = "<em>No submissions yet.</em>";
      return;
    }
    listEl.innerHTML = data.submissions.map(function (s) {
      const status = s.timedOut ? "timeout" : s.exitCode === 0 ? "ok" : "error";
      const icon = status === "ok" ? "\u2705" : status === "timeout" ? "\u23F1" : "\u274C";
      const preview = s.code.split("\n").slice(0, 50);
      return '<div class="history-item history-' + status + '" onclick=\'loadSubmission(' + JSON.stringify(JSON.stringify(s)) + ')\'>'
        + '<span class="history-icon">' + icon + '</span>'
        + '<span class="history-lang">' + s.language + '</span>'
        + '<span class="history-preview">' + escapeHtml(preview) + '</span>'
        + '<span class="history-time">' + s.createdAt + '</span>'
        + '</div>';
    }).join("");
  } catch (e) {
    listEl.innerHTML = "<em>Failed to load history.</em>";
  }
}

function escapeHtml(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function loadSubmission(jsonStr) {
  const s = JSON.parse(jsonStr);
  document.getElementById("language").value = s.language;
  editor.setOption("mode", MODES[s.language]);
  editor.setValue(s.code);
  document.getElementById("userInput").value = s.input || "";
  document.getElementById("stdout").textContent = s.stdout || "";
  document.getElementById("stderr").textContent = s.stderr || "";
}

// Load history on page load
loadHistory();
