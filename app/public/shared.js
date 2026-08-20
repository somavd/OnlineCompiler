const API_BASE = "";

function textCell(value) {
    const td = document.createElement("td");
    td.textContent = value == null ? "" : String(value);
    return td;
}

function showStatus(message, isError) {
    const el = document.getElementById("status");
    if (!el) return;
    el.textContent = message;
    el.className = isError ? "status status-error" : "status status-ok";
    setTimeout(() => { el.textContent = ""; el.className = "status"; }, 5000);
}

function showError(message) {
    showStatus(message, true);
}
