// STATE
let questions = [];
let testcases = [];
let editId = null;

// ===============================
// INIT
// ===============================
document.addEventListener("DOMContentLoaded", () => {
    loadQuestions();
});

// ===============================
// QUESTIONS
// ===============================
async function loadQuestions() {
    try {
        const res = await fetch(`${API_BASE}/api/questions`);
        const data = await res.json();
        questions = data.questions || [];
    } catch (e) {
        questions = [];
        showError("Failed to load questions from server.");
    }
    renderQuestions();
}

async function saveQuestion() {
    const title = document.getElementById("title").value.trim();
    const description = document.getElementById("description").value.trim();
    const category = document.getElementById("category").value.trim();
    const difficulty = document.getElementById("difficulty").value;

    if (!title) {
        showError("Title required");
        return;
    }

    const payload = { title, description, category, difficulty };

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
            showError(data.error || "Failed to save question");
            return;
        }
    } catch (e) {
        showError("Server error while saving question.");
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

        const viewBtn = document.createElement("button");
        viewBtn.textContent = "View";
        viewBtn.addEventListener("click", () => {
            location.href = `/public/question-detail.html?id=${q.id}`;
        });

        const deleteBtn = document.createElement("button");
        deleteBtn.textContent = "Delete";
        deleteBtn.addEventListener("click", () => deleteQuestion(q.id));

        actions.appendChild(editBtn);
        actions.appendChild(viewBtn);
        actions.appendChild(deleteBtn);
        row.appendChild(actions);

        table.appendChild(row);
    });
}

function editQuestion(id) {
    const q = questions.find((item) => item.id === id);
    if (!q) return;
    document.getElementById("title").value = q.title;
    document.getElementById("description").value = q.description || "";
    document.getElementById("category").value = q.category;
    document.getElementById("difficulty").value = q.difficulty;
    editId = id;

    document.getElementById("editTestCases").style.display = "block";
    loadEditTestCases(id);
}

async function deleteQuestion(id) {
    if (!confirm("Delete this question?")) return;
    try {
        const res = await fetch(`${API_BASE}/api/questions/${id}`, { method: "DELETE" });
        const data = await res.json();
        if (!data.success) {
            showError("Failed to delete question");
            return;
        }
    } catch (e) {
        showError("Server error while deleting question.");
        return;
    }
    if (editId === id) clearQuestionForm();
    await loadQuestions();
}

function clearQuestionForm() {
    document.getElementById("title").value = "";
    document.getElementById("description").value = "";
    document.getElementById("category").value = "";
    document.getElementById("difficulty").selectedIndex = 0;
    editId = null;
    document.getElementById("editTestCases").style.display = "none";
    testcases = [];
    renderEditTestCases();
}

async function loadEditTestCases(questionId) {
    if (!questionId) {
        testcases = [];
        renderEditTestCases();
        return;
    }
    try {
        const res = await fetch(`${API_BASE}/api/questions/${questionId}/testcases`);
        const data = await res.json();
        testcases = data.testcases || [];
    } catch (e) {
        testcases = [];
        showError("Failed to load test cases from server.");
    }
    renderEditTestCases();
}

async function addEditTestCase() {
    if (!editId) {
        showError("No question selected for editing");
        return;
    }
    const input = document.getElementById("editTestInput").value;
    const output = document.getElementById("editExpectedOutput").value;
    const isHidden = document.getElementById("editTestHidden").checked;

    if (!input || !output) {
        showError("Fill input and expected output fields");
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/questions/${editId}/testcases`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ input, expected_output: output, is_hidden: isHidden }),
        });
        const data = await res.json();
        if (!res.ok || !data.success) {
            showError(data.error || "Failed to add test case");
            return;
        }
    } catch (e) {
        showError("Server error while adding test case.");
        return;
    }

    document.getElementById("editTestInput").value = "";
    document.getElementById("editExpectedOutput").value = "";
    document.getElementById("editTestHidden").checked = false;
    await loadEditTestCases(editId);
}

function renderEditTestCases() {
    const table = document.getElementById("editTestcaseTable");
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
        deleteBtn.addEventListener("click", () => deleteEditTestCase(t.id));
        actions.appendChild(deleteBtn);
        row.appendChild(actions);

        table.appendChild(row);
    });
}

async function deleteEditTestCase(id) {
    if (!confirm("Delete this test case?")) return;
    try {
        const res = await fetch(`${API_BASE}/api/testcases/${id}`, { method: "DELETE" });
        const data = await res.json();
        if (!data.success) {
            showError("Failed to delete test case");
            return;
        }
    } catch (e) {
        showError("Server error while deleting test case.");
        return;
    }
    await loadEditTestCases(editId);
}

