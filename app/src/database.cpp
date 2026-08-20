#include "database.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

static const char* CREATE_TABLE_SQL = R"(
CREATE TABLE IF NOT EXISTS submissions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    language    TEXT    NOT NULL,
    code        TEXT    NOT NULL,
    input       TEXT    DEFAULT '',
    stdout      TEXT    DEFAULT '',
    stderr      TEXT    DEFAULT '',
    exit_code   INTEGER DEFAULT 0,
    timed_out   INTEGER DEFAULT 0,
    created_at  TEXT    DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS questions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    description TEXT    DEFAULT '',
    category    TEXT    NOT NULL,
    difficulty  TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS test_cases (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id     INTEGER NOT NULL,
    input           TEXT    NOT NULL,
    expected_output TEXT    NOT NULL,
    is_hidden       INTEGER DEFAULT 0,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_lang ON submissions(language);
CREATE INDEX IF NOT EXISTS idx_time ON submissions(created_at);
CREATE INDEX IF NOT EXISTS idx_question_id ON test_cases(question_id);
)";

Database::Database(const std::string& path) {
    // Ensure directory exists
    fs::path dbPath(path);
    if (dbPath.has_parent_path()) {
        fs::create_directories(dbPath.parent_path());
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err);
    }

    // Enable WAL mode for better concurrent read performance
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    // Enable foreign key constraints for cascade delete
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    init();
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::init() {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, CREATE_TABLE_SQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create table: " + err);
    }
}

int Database::saveSubmission(const Submission& sub) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO submissions (language, code, input, stdout, stderr, exit_code, timed_out)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, sub.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sub.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, sub.input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, sub.stdoutStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sub.stderrStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, sub.exitCode);
    sqlite3_bind_int(stmt, 7, sub.timedOut ? 1 : 0);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

std::vector<Submission> Database::getSubmissions(int limit, const std::string& language) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Submission> results;

    // Clamp limit
    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;

    std::string sql;
    if (language.empty()) {
        sql = "SELECT id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
              "FROM submissions ORDER BY created_at DESC LIMIT ?";
    } else {
        sql = "SELECT id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
              "FROM submissions WHERE language = ? ORDER BY created_at DESC LIMIT ?";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    if (language.empty()) {
        sqlite3_bind_int(stmt, 1, limit);
    } else {
        sqlite3_bind_text(stmt, 1, language.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
    }

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Submission s;
        s.id        = sqlite3_column_int(stmt, 0);
        s.language  = getText(stmt, 1);
        s.code      = getText(stmt, 2);
        s.input     = getText(stmt, 3);
        s.stdoutStr = getText(stmt, 4);
        s.stderrStr = getText(stmt, 5);
        s.exitCode  = sqlite3_column_int(stmt, 6);
        s.timedOut  = sqlite3_column_int(stmt, 7) != 0;
        s.createdAt = getText(stmt, 8);
        results.push_back(std::move(s));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::addQuestion(const Question& question) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO questions (title, description, category, difficulty)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, question.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, question.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, question.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, question.difficulty.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    if (!success) {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}

std::vector<Question> Database::getQuestions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Question> results;

    const char* sql = R"(
        SELECT id, title, description, category, difficulty
        FROM questions
        ORDER BY id ASC
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Question q;
        q.id = sqlite3_column_int(stmt, 0);
        q.title = getText(stmt, 1);
        q.description = getText(stmt, 2);
        q.category = getText(stmt, 3);
        q.difficulty = getText(stmt, 4);
        results.push_back(std::move(q));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::updateQuestion(const Question& question) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE questions
        SET title = ?, description = ?, category = ?, difficulty = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, question.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, question.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, question.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, question.difficulty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, question.id);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    if (rc != SQLITE_DONE) {
        std::cerr << "DB update error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::deleteQuestion(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        DELETE FROM questions
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    if (!success) {
        std::cerr << "DB delete error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}

int Database::addTestCase(const TestCase& testCase) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO test_cases (question_id, input, expected_output, is_hidden)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, testCase.questionId);
    sqlite3_bind_text(stmt, 2, testCase.input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, testCase.expectedOutput.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, testCase.isHidden ? 1 : 0);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

std::vector<TestCase> Database::getTestCases(int questionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TestCase> results;

    const char* sql = R"(
        SELECT id, question_id, input, expected_output, is_hidden
        FROM test_cases
        WHERE question_id = ?
        ORDER BY id ASC
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, questionId);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TestCase tc;
        tc.id = sqlite3_column_int(stmt, 0);
        tc.questionId = sqlite3_column_int(stmt, 1);
        tc.input = getText(stmt, 2);
        tc.expectedOutput = getText(stmt, 3);
        tc.isHidden = sqlite3_column_int(stmt, 4) != 0;
        results.push_back(std::move(tc));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::updateTestCase(const TestCase& testCase) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE test_cases
        SET input = ?, expected_output = ?, is_hidden = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, testCase.input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, testCase.expectedOutput.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, testCase.isHidden ? 1 : 0);
    sqlite3_bind_int(stmt, 4, testCase.id);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    if (rc != SQLITE_DONE) {
        std::cerr << "DB update error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::deleteTestCase(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        DELETE FROM test_cases
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    if (!success) {
        std::cerr << "DB delete error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}
