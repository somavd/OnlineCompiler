#pragma once

#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

struct Question {
    int id = 0;
    std::string title;
    std::string description;
    std::string category;
    std::string difficulty;
    std::string createdAt;
};

struct TestCase {
    int id = 0;
    int questionId = 0;
    std::string input;
    std::string expectedOutput;
    bool isHidden = false;
};

struct Submission {
    int id = 0;
    std::string language;
    std::string code;
    std::string input;
    std::string stdoutStr;
    std::string stderrStr;
    int exitCode = 0;
    bool timedOut = false;
    std::string createdAt;
};

class Database {
public:
    explicit Database(const std::string& path = "data/submissions.db");
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Save a submission, returns the new row ID
    int saveSubmission(const Submission& sub);

    // Retrieve recent submissions
    std::vector<Submission> getSubmissions(int limit = 20, const std::string& language = "");

    // Question management
    int addQuestion(const std::string& title, const std::string& description,
                    const std::string& category, const std::string& difficulty);
    bool updateQuestion(int id, const std::string& title, const std::string& description,
                        const std::string& category, const std::string& difficulty);
    bool deleteQuestion(int id);
    std::vector<Question> getQuestions();

    // Test case management
    int addTestCase(int questionId, const std::string& input,
                    const std::string& expectedOutput, bool isHidden);
    bool deleteTestCase(int id);
    std::vector<TestCase> getTestCases(int questionId);

private:
    void init();
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};
