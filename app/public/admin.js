document.addEventListener("DOMContentLoaded", () => {
    loadQuestionCount();
});

async function loadQuestionCount() {
    const countEl = document.getElementById("questionCount");
    if (!countEl) return;
    try {
        const res = await fetch(`${API_BASE}/api/questions`);
        if (!res.ok) throw new Error("Failed to load questions");
        const data = await res.json();
        countEl.textContent = (data.questions || []).length;
    } catch (e) {
        showError("Failed to load question count");
        countEl.textContent = "0";
    }
}
