// ===============================
// ADMIN DASHBOARD
// Questions are persisted in the C++ backend via /api/questions.
// Test cases have no backend endpoint yet, so they stay in localStorage.
// ===============================

// Backend base URL. Empty string = same origin (when pages are served by
// the C++ server). Set to the server address if you serve the frontend
// from a different origin.
const API_BASE = "";

// STATE
let questions = [];
let testcases = JSON.parse(localStorage.getItem("testcases")) || [];
let editId = null; // null = add mode, otherwise the id being edited

// ===============================
// INIT
// ===============================
document.addEventListener("DOMContentLoaded", () => {
    initNavigation();
    loadQuestions();
    renderTestCases();
    updateCounts();
});

// ===============================
// NAVIGATION
// ===============================
function initNavigation() {
    const links = document.querySelectorAll(".sidebar li");

    links.forEach((link) => {
        link.addEventListener("click", () => {
            const page = link.getAttribute("data-page");
            if (!page) return;

            document.querySelectorAll(".page").forEach(p => p.style.display = "none");
            document.getElementById(page).style.display = "block";
        });
    });
}

// Create a table cell with safe text content (prevents XSS).
function textCell(value) {
    const td = document.createElement("td");
    td.textContent = value == null ? "" : String(value);
    return td;
}

// ===============================
// QUESTIONS (backend-backed)
// ===============================
async function loadQuestions() {
    try {
        const res = await fetch(`${API_BASE}/api/questions`);
        const data = await res.json();
        questions = data.questions || [];
    } catch (e) {
        questions = [];
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
            res = await fetch(`${API_BASE}/api/questions`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload),
            });
        } else {
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

function editQuestion(id) {
    const q = questions.find((item) => item.id === id);
    if (!q) return;
    document.getElementById("title").value = q.title;
    document.getElementById("category").value = q.category;
    document.getElementById("difficulty").value = q.difficulty;
    editId = id;
}

async function deleteQuestion(id) {
    if (!confirm("Delete this question?")) return;
    try {
        const res = await fetch(`${API_BASE}/api/questions/${id}`, { method: "DELETE" });
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

function clearQuestionForm() {
    document.getElementById("title").value = "";
    document.getElementById("category").value = "";
    document.getElementById("difficulty").selectedIndex = 0;
    editId = null;
}

// ===============================
// TEST CASES (localStorage only — no backend endpoint yet)
// ===============================
function saveTestCases() {
    localStorage.setItem("testcases", JSON.stringify(testcases));
}

function addTestCase() {
    const input = document.getElementById("testInput").value;
    const output = document.getElementById("expectedOutput").value;

    if (!input || !output) {
        alert("Fill fields");
        return;
    }

    testcases.push({
        id: Date.now(),
        input,
        output,
        visibility: "public"
    });

    saveTestCases();
    renderTestCases();
    updateCounts();
}

function renderTestCases() {
    const table = document.getElementById("testcaseTable");
    table.innerHTML = "";

    testcases.forEach((t) => {
        const row = document.createElement("tr");
        row.appendChild(textCell(t.id));
        row.appendChild(textCell(t.input));
        row.appendChild(textCell(t.visibility));

        const actions = document.createElement("td");
        const deleteBtn = document.createElement("button");
        deleteBtn.textContent = "Delete";
        deleteBtn.addEventListener("click", () => deleteTestCase(t.id));
        actions.appendChild(deleteBtn);
        row.appendChild(actions);

        table.appendChild(row);
    });
}

function deleteTestCase(id) {
    testcases = testcases.filter((t) => t.id !== id);
    saveTestCases();
    renderTestCases();
    updateCounts();
}

// ===============================
// COUNTS
// ===============================
function updateCounts() {
    document.getElementById("questionCount").innerText = questions.length;
    document.getElementById("testcaseCount").innerText = testcases.length;
}

// ===============================
// LOGOUT
// ===============================
function logout() {
    localStorage.clear();
    location.href = "/public/login.html";
}