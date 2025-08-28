#include<crow.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <filesystem>
#include <chrono>
#include <random>
#include <future>

namespace fs = std::filesystem;

std::string exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error: failed to run command\n";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Generate unique ID for temp folder
std::string gen_unique_id() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::to_string(now);
}

int main() {
    crow::SimpleApp app;

    // Serve HTML editor
    CROW_ROUTE(app, "/")([]() {
        std::ifstream file("htmls/editor.html");
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    });

    // Handle code execution
    CROW_ROUTE(app, "/run").methods("POST"_method)
    ([](const crow::request& req) {

        auto body = crow::json::load(req.body);
	std::string language = body["language"].s();
	std::string code = body["code"].s();

	crow::json::wvalue res;

        // Unique temp dir
        std::string id = gen_unique_id();
        fs::create_directories("tmp/" + id);

        std::ostringstream result;
        if (language == "cpp") {
            std::string cppFile = "tmp/" + id + "/program.cpp";
            std::ofstream ofs(cppFile);
            ofs << code;
            ofs.close();

            // Compile + run inside container
            std::string cmd =
                "sudo docker run --rm "
                "-v " + fs::absolute("tmp/" + id).string() + ":/workspace "
                "-w /workspace gcc:latest "
                "/bin/bash -c \"g++ program.cpp -o program 2>compile_err.txt && ./program >prog_out.txt 2>prog_err.txt\"";

            exec_cmd(cmd);

            res["stderr"] = exec_cmd("cat tmp/" + id + "/compile_err.txt");
            res["stdout"] = exec_cmd("cat tmp/" + id + "/prog_out.txt");
            res["status"] = exec_cmd("cat tmp/" + id + "/prog_err.txt");

        } else {
	    res["stdout"] = "Only C++ Supported";
	    res["stderr"] = "";
	    res["status"] = "";
        }

        // Cleanup temp folder
        // fs::remove_all("tmp/" + id);

	return crow::response(res);
    });

    app.port(18080).multithreaded().run();
}

