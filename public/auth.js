function signup()
{
    const name =
    document.getElementById("name").value;

    const email =
    document.getElementById("email").value;

    const password =
    document.getElementById("password").value;

    const user = {
        name,
        email,
        password
    };

    localStorage.setItem(
        email,
        JSON.stringify(user)
    );

    alert("Signup Successful");

    location.href = "login.html";
}

function login()
{
    const email =
    document.getElementById("email").value;

    const password =
    document.getElementById("password").value;

    const user =
    JSON.parse(localStorage.getItem(email));

    if(user && user.password === password)
    {
        localStorage.setItem(
            "loggedIn",
            "true"
        );

        localStorage.setItem(
            "role",
            "user"
        );

        location.href =
        "compiler.html";
    }
    else
    {
        alert("Invalid Credentials");
    }
}

function adminLogin()
{
    const email =
    document.getElementById("adminEmail").value;

    const password =
    document.getElementById("adminPassword").value;

    if(
        email === "admin@gmail.com" &&
        password === "admin123"
    )
    {
        localStorage.setItem(
            "loggedIn",
            "true"
        );

        localStorage.setItem(
            "role",
            "admin"
        );

        location.href =
        "compiler.html";
    }
    else
    {
        alert("Invalid Admin Credentials");
    }
}