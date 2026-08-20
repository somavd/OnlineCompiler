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
#include <cstring>
#include <unistd.h>

namespace fs = std::filesystem;

static const size_t MAX_BODY_BYTES = 1024 * 1024;  // 1MB

// JSON response helper — sets Content-Type and CORS properly
static crow::response jsonResponse(int code, crow::json::wvalue& body) {
    crow::response res(code, body.dump());
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
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
static volatile sig_atomic_t g_shutdownRequested = 0;

static void signalHandler(int sig) {
    // Only async-signal-safe calls here
    g_shutdownRequested = 1;
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

    // Global OPTIONS preflight handler for all /api/* endpoints
    CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
    ([](const std::string& /*path*/) {
        crow::response res(200);
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // Serve frontend — index.html at root
    CROW_ROUTE(app, "/")([]() {
        auto [found, content] = readFileContents("public/index.html");
        if (!found) {
            return crow::response(404, "Frontend not found");
        }
        auto res = crow::response(200, content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve static files: /public/<path>
    CROW_ROUTE(app, "/public/<path>")([](const std::string& filePath) {
        std::string fullPath = "public/" + filePath;

        // Prevent path traversal via canonical path check
        auto canonical = fs::weakly_canonical(fullPath);
        auto publicRoot = fs::weakly_canonical("public");
        if (canonical.string().rfind(publicRoot.string(), 0) != 0) {
            return crow::response(403, "Forbidden");
        }

        auto [found, content] = readFileContents(fullPath);
        if (!found) {
            return crow::response(404, "Not found");
        }
        std::string ext = fs::path(fullPath).extension().string();
        auto res = crow::response(200, content);
        res.set_header("Content-Type", getMimeType(ext));
        res.set_header("Cache-Control", "public, max-age=3600");
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
        if (req.body.size() > MAX_BODY_BYTES) {
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

    // GET /api/questions — retrieve all questions
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
            items.push_back(std::move(item));
        }
        body["questions"] = std::move(items);
        return jsonResponse(200, body);
    });

    // POST /api/questions — add a new question
    CROW_ROUTE(app, "/api/questions").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto jsonBody = crow::json::load(req.body);
        if (!jsonBody) {
            return jsonError(400, "Invalid JSON body");
        }

        if (!jsonBody.has("title") || std::string(jsonBody["title"].s()).empty()) {
            return jsonError(400, "Title is required");
        }

        std::string title = jsonBody["title"].s();
        std::string description = jsonBody.has("description") ? std::string(jsonBody["description"].s()) : "";
        std::string category = jsonBody.has("category") ? std::string(jsonBody["category"].s()) : "";
        std::string difficulty = jsonBody.has("difficulty") ? std::string(jsonBody["difficulty"].s()) : "";

        Question question;
        question.title = title;
        question.description = description;
        question.category = category;
        question.difficulty = difficulty;

        if (!db.addQuestion(question)) {
            return jsonError(500, "Failed to add question");
        }

        crow::json::wvalue body;
        body["success"] = true;
        return jsonResponse(200, body);
    });

    // PUT /api/questions/<int> — update an existing question
    CROW_ROUTE(app, "/api/questions/<int>").methods("PUT"_method)
    ([&db](const crow::request& req, int id) {
        auto jsonBody = crow::json::load(req.body);
        if (!jsonBody) {
            return jsonError(400, "Invalid JSON body");
        }
        if (!jsonBody.has("title") || std::string(jsonBody["title"].s()).empty()) {
            return jsonError(400, "Title is required");
        }

        Question question;
        question.id = id;
        question.title = jsonBody["title"].s();
        question.description = jsonBody.has("description") ? std::string(jsonBody["description"].s()) : "";
        question.category = jsonBody.has("category") ? std::string(jsonBody["category"].s()) : "";
        question.difficulty = jsonBody.has("difficulty") ? std::string(jsonBody["difficulty"].s()) : "";

        if (!db.updateQuestion(question)) {
            return jsonError(404, "Question not found or not updated");
        }

        crow::json::wvalue body;
        body["success"] = true;
        return jsonResponse(200, body);
    });

    // DELETE /api/questions/<int> — delete a question
    CROW_ROUTE(app, "/api/questions/<int>").methods("DELETE"_method)
    ([&db](int id) {
        bool deleted = db.deleteQuestion(id);

        crow::json::wvalue body;
        body["success"] = deleted;
        body["id"] = id;
        return jsonResponse(200, body);
    });

    // GET /api/questions/<int>/testcases — get all test cases for a question
    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("GET"_method)
    ([&db](int id) {
        auto testCases = db.getTestCases(id);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& tc : testCases) {
            crow::json::wvalue item;
            item["id"] = tc.id;
            item["question_id"] = tc.questionId;
            item["input"] = tc.input;
            item["expected_output"] = tc.expectedOutput;
            item["is_hidden"] = tc.isHidden;
            items.push_back(std::move(item));
        }
        body["testcases"] = std::move(items);
        return jsonResponse(200, body);
    });

    // POST /api/questions/<int>/testcases — add a test case to a question
    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("POST"_method)
    ([&db](const crow::request& req, int questionId) {
        auto jsonBody = crow::json::load(req.body);
        if (!jsonBody) {
            return jsonError(400, "Invalid JSON body");
        }

        if (!jsonBody.has("input") || std::string(jsonBody["input"].s()).empty()) {
            return jsonError(400, "Input is required");
        }
        if (!jsonBody.has("expected_output") || std::string(jsonBody["expected_output"].s()).empty()) {
            return jsonError(400, "Expected output is required");
        }

        TestCase testCase;
        testCase.questionId = questionId;
        testCase.input = jsonBody["input"].s();
        testCase.expectedOutput = jsonBody["expected_output"].s();
        testCase.isHidden = jsonBody.has("is_hidden") ? jsonBody["is_hidden"].b() : false;

        int rowId = db.addTestCase(testCase);
        if (rowId < 0) {
            return jsonError(500, "Failed to add test case");
        }

        crow::json::wvalue body;
        body["success"] = true;
        body["id"] = rowId;
        return jsonResponse(200, body);
    });

    // DELETE /api/testcases/<int> — delete a test case
    CROW_ROUTE(app, "/api/testcases/<int>").methods("DELETE"_method)
    ([&db](int id) { 
        bool deleted = db.deleteTestCase(id);

        crow::json::wvalue body;
        body["success"] = deleted;
        body["id"] = id;
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

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    std::cout << "Server running on http://0.0.0.0:" << config.port
              << " (" << threads << " threads)" << std::endl;

    app.bindaddr("0.0.0.0").port(config.port).concurrency(threads).run();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
