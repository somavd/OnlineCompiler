// ===============================
// ADMIN DASHBOARD JS (FIXED)
// ===============================

// STATE
let questions = JSON.parse(localStorage.getItem("questions")) || [];
let testcases = JSON.parse(localStorage.getItem("testcases")) || [];
let editIndex = -1;

// ===============================
// INIT
// ===============================
document.addEventListener("DOMContentLoaded", () => {
    initNavigation();
    renderQuestions();
    renderTestCases();
    updateCounts();
});

// ===============================
// NAVIGATION FIXED
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

// ===============================
// SAVE
// ===============================
function saveData() {
    localStorage.setItem("questions", JSON.stringify(questions));
    localStorage.setItem("testcases", JSON.stringify(testcases));
}

// ===============================
// QUESTION - ADD / EDIT
// ===============================
function saveQuestion() {
    const title = document.getElementById("title").value;
    const category = document.getElementById("category").value;
    const difficulty = document.getElementById("difficulty").value;

    if (!title) {
        alert("Title required");
        return;
    }

    if (editIndex === -1) {
        questions.push({
            id: Date.now(),
            title,
            category,
            difficulty
        });
    } else {
        questions[editIndex].title = title;
        questions[editIndex].category = category;
        questions[editIndex].difficulty = difficulty;
        editIndex = -1;
    }

    saveData();
    renderQuestions();
    updateCounts();
}

// ===============================
// RENDER QUESTIONS
// ===============================
function renderQuestions() {
    const table = document.getElementById("questionTable");
    table.innerHTML = "";

    questions.forEach((q, i) => {
        table.innerHTML += `
        <tr>
            <td>${q.id}</td>
            <td>${q.title}</td>
            <td>${q.difficulty}</td>
            <td>${q.category}</td>
            <td>
                <button onclick="editQuestion(${i})">Edit</button>
                <button onclick="deleteQuestion(${i})">Delete</button>
            </td>
        </tr>
        `;
    });
}

// EDIT
function editQuestion(i) {
    const q = questions[i];
    document.getElementById("title").value = q.title;
    document.getElementById("category").value = q.category;
    document.getElementById("difficulty").value = q.difficulty;
    editIndex = i;
}

// DELETE
function deleteQuestion(i) {
    questions.splice(i, 1);
    saveData();
    renderQuestions();
    updateCounts();
}

// ===============================
// TEST CASES
// ===============================
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

    saveData();
    renderTestCases();
    updateCounts();
}

// RENDER TEST CASES
function renderTestCases() {
    const table = document.getElementById("testcaseTable");
    table.innerHTML = "";

    testcases.forEach((t, i) => {
        table.innerHTML += `
        <tr>
            <td>${t.id}</td>
            <td>${t.input}</td>
            <td>${t.visibility}</td>
            <td>
                <button onclick="deleteTestCase(${i})">Delete</button>
            </td>
        </tr>
        `;
    });
}

// DELETE TEST CASE
function deleteTestCase(i) {
    testcases.splice(i, 1);
    saveData();
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
    location.href = "login.html";
}