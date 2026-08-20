// ===============================
// ADMIN DASHBOARD
// Questions and test cases are persisted in the C++ backend via /api/questions.
// ===============================

// Backend base URL. Empty string = same origin (when pages are served by
// the C++ server). Set to the server address if you serve the frontend
// from a different origin.
const API_BASE = "";

// STATE
let questions = [];
let testcases = [];
let editId = null; // null = add mode, otherwise the id being edited
let selectedQuestionId = null; // ID of question currently viewing test cases for

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

        const viewTestCasesBtn = document.createElement("button");
        viewTestCasesBtn.textContent = "Test Cases";
        viewTestCasesBtn.addEventListener("click", () => {
            loadTestCases(q.id);
            document.querySelectorAll(".sidebar li").forEach(l => l.classList.remove("active"));
            document.querySelector('[data-page="testcases"]').classList.add("active");
            document.querySelectorAll(".page").forEach(p => p.style.display = "none");
            document.getElementById("testcases").style.display = "block";
        });

        const deleteBtn = document.createElement("button");
        deleteBtn.textContent = "Delete";
        deleteBtn.addEventListener("click", () => deleteQuestion(q.id));

        actions.appendChild(editBtn);
        actions.appendChild(viewTestCasesBtn);
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
// TEST CASES (backend-backed via /api/questions/:id/testcases)
// ===============================
async function loadTestCases(questionId) {
    if (!questionId) {
        testcases = [];
        renderTestCases();
        updateCounts();
        return;
    }
    selectedQuestionId = questionId;
    try {
        const res = await fetch(`${API_BASE}/api/questions/${questionId}/testcases`);
        const data = await res.json();
        testcases = data.testcases || [];
    } catch (e) {
        testcases = [];
        alert("Failed to load test cases from server.");
    }
    renderTestCases();
    updateCounts();
}

async function addTestCase() {
    if (!selectedQuestionId) {
        alert("Please select a question first");
        return;
    }
    const input = document.getElementById("testInput").value;
    const output = document.getElementById("expectedOutput").value;
    const isHidden = document.getElementById("testHidden").checked;

    if (!input || !output) {
        alert("Fill input and expected output fields");
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/questions/${selectedQuestionId}/testcases`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ input, expected_output: output, is_hidden: isHidden }),
        });
        const data = await res.json();
        if (!res.ok || !data.success) {
            alert(data.error || "Failed to add test case");
            return;
        }
    } catch (e) {
        alert("Server error while adding test case.");
        return;
    }

    document.getElementById("testInput").value = "";
    document.getElementById("expectedOutput").value = "";
    document.getElementById("testHidden").checked = false;
    await loadTestCases(selectedQuestionId);
}

function renderTestCases() {
    const table = document.getElementById("testcaseTable");
    table.innerHTML = "";

    testcases.forEach((t) => {
        const row = document.createElement("tr");
        row.appendChild(textCell(t.id));
        row.appendChild(textCell(t.input));
        row.appendChild(textCell(t.expected_output));
        row.appendChild(textCell(t.is_hidden ? "Hidden" : "Public"));

        const actions = document.createElement("td");
        const deleteBtn = document.createElement("button");
        deleteBtn.textContent = "Delete";
        deleteBtn.addEventListener("click", () => deleteTestCase(t.id));
        actions.appendChild(deleteBtn);
        row.appendChild(actions);

        table.appendChild(row);
    });
}

async function deleteTestCase(id) {
    if (!confirm("Delete this test case?")) return;
    try {
        const res = await fetch(`${API_BASE}/api/testcases/${id}`, { method: "DELETE" });
        const data = await res.json();
        if (!data.success) {
            alert("Failed to delete test case");
            return;
        }
    } catch (e) {
        alert("Server error while deleting test case.");
        return;
    }
    await loadTestCases(selectedQuestionId);
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