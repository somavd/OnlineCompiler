#include "crow.h"
#include "config.hpp"
#include "executor.hpp"
#include "validator.hpp"
#include "rate_limiter.hpp"
#include "database.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace fs = std::filesystem;

static fs::path getPublicDir() {
    const char* env = std::getenv("PUBLIC_DIR");
    if (env) return fs::weakly_canonical(env);
    return fs::weakly_canonical("public");
}

// JSON response helper — sets Content-Type and CORS properly
static crow::response jsonResponse(int code, crow::json::wvalue& body) {
    crow::response res(code, body.dump());
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    return res;
}

static crow::response jsonError(int code, const std::string& message) {
    crow::json::wvalue body;
    body["error"] = message;
    return jsonResponse(code, body);
}

struct FileReadResult {
    bool found = false;
    std::string content;
};

static FileReadResult readFileContents(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {false, ""};
    std::ostringstream ss;
    ss << file.rdbuf();
    return {true, ss.str()};
}

static std::string getMimeType(const std::string& ext) {
    if (ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    return "application/octet-stream";
}

// Global app pointer for graceful shutdown
static crow::SimpleApp* g_app = nullptr;

static void signalHandler(int /*sig*/) {
    const char msg[] = "\nShutdown requested...\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    if (g_app) g_app->stop();
}

int main() {
    Config config = Config::fromEnv();

    // Check Docker availability
    if (!isDockerAvailable()) {
        std::cerr << "WARNING: Docker is not available. Code execution will fail." << std::endl;
    }

    RateLimiter limiter(config.rateLimitWindowSeconds, config.rateLimitMaxRequests);
    Database db("data/submissions.db");
    std::cout << "Database initialized (data/submissions.db)" << std::endl;

    crow::SimpleApp app;
    g_app = &app;

    // Graceful shutdown on SIGINT / SIGTERM
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const fs::path publicDir = getPublicDir();

    // Serve frontend — compiler.html at root
    CROW_ROUTE(app, "/")([&publicDir]() {
        auto [found, content] = readFileContents((publicDir / "compiler.html").string());
        if (!found) {
            return crow::response(404, "Frontend not found");
        }
        auto res = crow::response(200, content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve static files: /public/<path>
    CROW_ROUTE(app, "/public/<path>")([&publicDir](const std::string& filePath) {
        fs::path fullPath = publicDir / filePath;

        // Prevent path traversal via canonical path check
        auto canonical = fs::weakly_canonical(fullPath);
        auto publicRoot = fs::weakly_canonical(publicDir);
        if (canonical.string().rfind(publicRoot.string(), 0) != 0) {
            return crow::response(403, "Forbidden");
        }

        auto [found, content] = readFileContents(fullPath.string());
        if (!found) {
            return crow::response(404, "Not found");
        }
        std::string ext = fs::path(fullPath).extension().string();
        auto res = crow::response(200, content);
        res.set_header("Content-Type", getMimeType(ext));
        return res;
    });

    // POST /api/run — execute code
    CROW_ROUTE(app, "/api/run").methods("POST"_method)
    ([&config, &limiter, &db](const crow::request& req) {
        // Rate limiting
        std::string clientIp = req.remote_ip_address;
        if (!limiter.allow(clientIp)) {
            return jsonError(429, "Too many requests. Please try again later.");
        }

        // Request body size check (1MB)
        if (req.body.size() > 1024 * 1024) {
            return jsonError(413, "Request body too large");
        }

        // Validate
        auto v = validateRunRequest(req);
        if (!v.ok) {
            return jsonError(400, v.error);
        }

        // Execute
        ExecResult result = executeCode(v.language, v.code, v.input, config);

        // Save to database
        Submission sub;
        sub.language  = v.language;
        sub.code      = v.code;
        sub.input     = v.input;
        sub.stdoutStr = result.stdoutStr;
        sub.stderrStr = result.stderrStr;
        sub.exitCode  = result.exitCode;
        sub.timedOut  = result.timedOut;
        db.saveSubmission(sub);

        crow::json::wvalue body;
        body["stdout"] = result.stdoutStr;
        body["stderr"] = result.stderrStr;
        body["exitCode"] = result.exitCode;
        body["timedOut"] = result.timedOut;
        return jsonResponse(200, body);
    });

    // GET /api/submissions — retrieve submission history
    CROW_ROUTE(app, "/api/submissions").methods("GET"_method)
    ([&db](const crow::request& req) {
        int limit = 20;
        std::string language;

        if (auto* v = req.url_params.get("limit")) {
            try { limit = std::stoi(v); } catch (...) {}
        }
        if (auto* v = req.url_params.get("language")) {
            language = v;
        }

        auto submissions = db.getSubmissions(limit, language);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& s : submissions) {
            crow::json::wvalue item;
            item["id"] = s.id;
            item["language"] = s.language;
            item["code"] = s.code;
            item["input"] = s.input;
            item["stdout"] = s.stdoutStr;
            item["stderr"] = s.stderrStr;
            item["exitCode"] = s.exitCode;
            item["timedOut"] = s.timedOut;
            item["createdAt"] = s.createdAt;
            items.push_back(std::move(item));
        }
        body["submissions"] = std::move(items);
        return jsonResponse(200, body);
    });

    // Admin: questions
    CROW_ROUTE(app, "/api/questions").methods("GET"_method)
    ([&db](const crow::request& /*req*/) {
        auto questions = db.getQuestions();

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& q : questions) {
            crow::json::wvalue item;
            item["id"] = q.id;
            item["title"] = q.title;
            item["description"] = q.description;
            item["category"] = q.category;
            item["difficulty"] = q.difficulty;
            item["createdAt"] = q.createdAt;
            items.push_back(std::move(item));
        }
        body["questions"] = std::move(items);
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/questions").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("title")) {
            return jsonError(400, "Missing 'title'");
        }

        std::string title       = body["title"].s();
        std::string description = body.has("description") ? std::string(body["description"].s()) : "";
        std::string category    = body.has("category")    ? std::string(body["category"].s())    : "";
        std::string difficulty  = body.has("difficulty")  ? std::string(body["difficulty"].s())  : "";

        if (title.empty()) {
            return jsonError(400, "Empty title");
        }

        int id = db.addQuestion(title, description, category, difficulty);
        if (id < 0) {
            return jsonError(500, "Failed to save question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["id"] = id;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/questions/<int>").methods("PUT"_method)
    ([&db](const crow::request& req, int id) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("title")) {
            return jsonError(400, "Missing 'title'");
        }

        std::string title       = body["title"].s();
        std::string description = body.has("description") ? std::string(body["description"].s()) : "";
        std::string category    = body.has("category")    ? std::string(body["category"].s())    : "";
        std::string difficulty  = body.has("difficulty")  ? std::string(body["difficulty"].s())  : "";

        if (title.empty()) {
            return jsonError(400, "Empty title");
        }

        if (!db.updateQuestion(id, title, description, category, difficulty)) {
            return jsonError(500, "Failed to update question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/questions/<int>").methods("DELETE"_method)
    ([&db](const crow::request& /*req*/, int id) {
        if (!db.deleteQuestion(id)) {
            return jsonError(500, "Failed to delete question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    // Admin: test cases
    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("GET"_method)
    ([&db](const crow::request& /*req*/, int questionId) {
        auto testcases = db.getTestCases(questionId);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& t : testcases) {
            crow::json::wvalue item;
            item["id"] = t.id;
            item["question_id"] = t.questionId;
            item["input"] = t.input;
            item["expected_output"] = t.expectedOutput;
            item["is_hidden"] = t.isHidden;
            items.push_back(std::move(item));
        }
        body["testcases"] = std::move(items);
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("POST"_method)
    ([&db](const crow::request& req, int questionId) {
        auto body = crow::json::load(req.body);
        if (!body) {
            return jsonError(400, "Invalid JSON");
        }

        std::string input          = body.has("input")          ? std::string(body["input"].s())          : "";
        std::string expectedOutput = body.has("expected_output") ? std::string(body["expected_output"].s()) : "";
        bool isHidden              = body.has("is_hidden")       ? (body["is_hidden"].b() || body["is_hidden"].t() == crow::json::type::True) : false;

        int id = db.addTestCase(questionId, input, expectedOutput, isHidden);
        if (id < 0) {
            return jsonError(500, "Failed to save test case");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["id"] = id;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/testcases/<int>").methods("DELETE"_method)
    ([&db](const crow::request& /*req*/, int id) {
        if (!db.deleteTestCase(id)) {
            return jsonError(500, "Failed to delete test case");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    std::cout << "Server running on http://0.0.0.0:" << config.port
              << " (" << threads << " threads)" << std::endl;
    app.bindaddr("0.0.0.0").port(config.port).concurrency(threads).run();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
