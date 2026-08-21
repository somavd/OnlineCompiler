const urlParams = new URLSearchParams(window.location.search);
const questionId = urlParams.get('id');

async function loadQuestion() {
    if (!questionId) {
        showError("No question ID provided");
        location.href = "/public/admin-questions.html";
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/questions`);
        const data = await res.json();
        const questions = data.questions || [];
        const q = questions.find(item => item.id == questionId);

        if (!q) {
            showError("Question not found");
            location.href = "/public/admin-questions.html";
            return;
        }

        document.getElementById("questionTitle").textContent = q.title;
        document.getElementById("questionDescription").textContent = q.description || "No description";
        document.getElementById("questionCategory").textContent = q.category;
        document.getElementById("questionDifficulty").textContent = q.difficulty;

        await loadTestCases(questionId);
    } catch (e) {
        showError("Failed to load question");
    }
}

async function loadTestCases(questionId) {
    try {
        const res = await fetch(`${API_BASE}/api/questions/${questionId}/testcases`);
        const data = await res.json();
        const testcases = data.testcases || [];
        renderTestCases(testcases);
    } catch (e) {
        showError("Failed to load test cases");
    }
}

function renderTestCases(testcases) {
    const table = document.getElementById("testcaseTable");
    table.innerHTML = "";

    testcases.forEach((t) => {
        const row = document.createElement("tr");
        row.appendChild(textCell(t.id));
        row.appendChild(textCell(t.input));
        row.appendChild(textCell(t.expected_output));
        row.appendChild(textCell(t.is_hidden ? "Hidden" : "Public"));
        table.appendChild(row);
    });
}

document.addEventListener("DOMContentLoaded", loadQuestion);
