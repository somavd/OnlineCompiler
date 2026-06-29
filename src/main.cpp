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
#include <csignal>

namespace fs = std::filesystem;

// JSON response helper — sets Content-Type and CORS properly
static crow::response jsonResponse(int code, crow::json::wvalue& body) {
    crow::response res(code, body.dump());
    res.set_header("Content-Type", "application/json");
    // Allow VS Code Live Server or external origins to read the JSON response
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
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

static void signalHandler(int sig) {
    std::cout << "\nShutting down (signal " << sig << ")..." << std::endl;
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

    // Global OPTIONS Preflight Handler for CORS requests
    CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
    ([](const std::string& /*path*/) {
        crow::response res(200);
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
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

        // Prevent path traversal
        if (filePath.find("..") != std::string::npos) {
            return crow::response(403, "Forbidden");
        }

        if (!fs::exists(fullPath)) {
            return crow::response(404, "Not found");
        }

        auto [found, content] = readFileContents(fullPath);
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

    std::cout << "Server running on http://0.0.0" << std::endl;
    
    // 💡 FIXED: Explicitly bind to 0.0.0.0 to accept network requests from Windows host
    app.bindaddr("0.0.0.0").port(3000).multithreaded().run();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
