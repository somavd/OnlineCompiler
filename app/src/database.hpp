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
    std::vector<Submission> getSubmissions(
        int limit = 20,
        const std::string& language = ""
    );

    // Question management
    bool addQuestion(const Question& question);
    std::vector<Question> getQuestions();
    bool updateQuestion(const Question& question);
    bool deleteQuestion(int id);

    // Test case management
    int addTestCase(const TestCase& testCase);
    std::vector<TestCase> getTestCases(int questionId);
    bool updateTestCase(const TestCase& testCase);
    bool deleteTestCase(int id);

private:
    void init();
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};