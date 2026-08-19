// ===============================
// SIGNUP
// ===============================
function signup() {
    const name = document.getElementById("name").value;
    const email = document.getElementById("email").value;
    const password = document.getElementById("password").value;

    if (!name || !email || !password) {
        alert("Please fill all fields");
        return;
    }

    const user = { name, email, password };

    localStorage.setItem(email, JSON.stringify(user));

    alert("Signup Successful");
    location.href = "/public/login.html";
}


// ===============================
// USER LOGIN
// ===============================
function login() {
    const email = document.getElementById("email").value;
    const password = document.getElementById("password").value;

    const user = JSON.parse(localStorage.getItem(email));

    if (user && user.password === password) {

        localStorage.setItem("loggedIn", "true");
        localStorage.setItem("role", "user");
        localStorage.setItem("currentUser", email);

        location.href = "/public/compiler.html";

    } else {
        alert("Invalid Credentials");
    }
}


// ===============================
// ADMIN LOGIN
// ===============================
function adminLogin() {
    const email = document.getElementById("adminEmail").value;
    const password = document.getElementById("adminPassword").value;

    if (email === "admin@gmail.com" && password === "admin123") {

        localStorage.setItem("loggedIn", "true");
        localStorage.setItem("role", "admin");
        localStorage.setItem("currentUser", email);

        // ⚡ IMPORTANT FIX
        location.href = "/public/admin_dashboard.html";

    } else {
        alert("Invalid Admin Credentials");
    }
}


// ===============================
// LOGOUT
// ===============================
function logout() {
    localStorage.clear();
    location.href = "/public/login.html";
}