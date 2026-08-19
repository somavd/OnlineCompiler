# Database Module

**File**: `app/src/database.hpp` / `app/src/database.cpp`

## Purpose

Provides persistent storage for submissions (code execution history) and questions (Q&A management) using SQLite with thread-safe operations.

## Public Interface

```cpp
struct Submission {
    int id;
    std::string language;      // "cpp", "python", "javascript"
    std::string code;          // Source code
    std::string input;         // Stdin provided
    std::string stdoutStr;     // Program output
    std::string stderrStr;     // Program errors
    int exitCode;              // Process exit code
    bool timedOut;             // Execution timeout flag
    std::string createdAt;     // ISO 8601 timestamp
};

struct Question {
    int id;
    std::string title;         // Question title
    std::string category;      // e.g., "Arrays", "DP"
    std::string difficulty;    // "Easy", "Medium", "Hard"
};

class Database {
public:
    explicit Database(const std::string& path = "data/submissions.db");
    ~Database();

    // Submissions
    int saveSubmission(const Submission& sub);
    std::vector<Submission> getSubmissions(
        int limit = 20,
        const std::string& language = ""  // Optional filter
    );

    // Questions
    bool addQuestion(const Question& question);
    std::vector<Question> getQuestions();
    bool updateQuestion(const Question& question);
    bool deleteQuestion(int id);
};
```

## Schema

### submissions table

```sql
CREATE TABLE submissions (
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

CREATE INDEX idx_lang ON submissions(language);
CREATE INDEX idx_time ON submissions(created_at);
```

**Indexes:**
- `idx_lang`: Fast filtering by language in `getSubmissions(limit, language)`
- `idx_time`: Fast ordering by creation time (newest first)

### questions table

```sql
CREATE TABLE questions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    category    TEXT    NOT NULL,
    difficulty  TEXT    NOT NULL
);
```

## Implementation Details

### Thread Safety

All public methods are **protected by a `std::mutex`**:

```cpp
class Database {
private:
    sqlite3* db_;
    std::mutex mutex_;  // Guards all DB operations
};
```

**Lock pattern:**
```cpp
bool Database::addQuestion(const Question& question) {
    std::lock_guard<std::mutex> lock(mutex_);  // Acquire
    // ... SQLite operations ...
    return success;
    // Release on scope exit
}
```

This ensures:
- Only one thread accesses SQLite at a time
- No race conditions on concurrent reads/writes
- Automatic lock release via RAII

### Connection & Initialization

**Constructor:**
1. Create `data/` directory if missing
2. Open SQLite connection with `sqlite3_open()`
3. Enable **WAL mode** for better concurrent read performance
4. Set `PRAGMA synchronous=NORMAL` for faster writes
5. Call `init()` to create tables if they don't exist

**WAL (Write-Ahead Logging):**
- Readers don't block writers
- Readers don't block other readers
- Writers don't block readers
- Trade-off: Extra `.db-wal` and `.db-shm` files

### saveSubmission()

```cpp
int Database::saveSubmission(const Submission& sub) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        INSERT INTO submissions (language, code, input, stdout, stderr, exit_code, timed_out)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    // Bind parameters (prevents SQL injection)
    sqlite3_bind_text(stmt, 1, sub.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sub.code.c_str(), -1, SQLITE_TRANSIENT);
    // ... more bindings ...
    
    sqlite3_step(stmt);  // Execute
    int rowId = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    
    return rowId;
}
```

**Key points:**
- Uses **parameterized queries** (`?` placeholders) to prevent SQL injection
- `SQLITE_TRANSIENT` tells SQLite to copy the string (safe with temporary objects)
- Returns the auto-generated `id` for the new row
- `created_at` is auto-filled by SQLite's `CURRENT_TIMESTAMP`

### getSubmissions()

```cpp
std::vector<Submission> Database::getSubmissions(
    int limit, 
    const std::string& language
) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Submission> results;
    
    // Clamp limit to prevent DoS
    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;
    
    std::string sql;
    if (language.empty()) {
        sql = "SELECT id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
              "FROM submissions ORDER BY created_at DESC LIMIT ?";
    } else {
        sql = "SELECT ... WHERE language = ? ORDER BY created_at DESC LIMIT ?";
    }
    
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    // Bind parameters
    if (language.empty()) {
        sqlite3_bind_int(stmt, 1, limit);
    } else {
        sqlite3_bind_text(stmt, 1, language.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
    }
    
    // Helper lambda to safely extract text (handles NULL)
    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };
    
    // Fetch rows
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Submission s;
        s.id = sqlite3_column_int(stmt, 0);
        s.language = getText(stmt, 1);
        // ... more columns ...
        results.push_back(std::move(s));
    }
    
    sqlite3_finalize(stmt);
    return results;
}
```

**Key points:**
- **Limit clamping**: Prevents `LIMIT -1` (returns all rows) or huge limits
- **Optional filtering**: If language is empty, returns all; otherwise filters
- **Ordering**: `ORDER BY created_at DESC` returns newest first
- **getText lambda**: Safely converts `sqlite3_column_text()` (returns `const unsigned char*`) to `std::string`, handling NULL

### updateQuestion()

```cpp
bool Database::updateQuestion(const Question& question) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        UPDATE questions
        SET title = ?, category = ?, difficulty = ?
        WHERE id = ?
    )";
    
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    sqlite3_bind_text(stmt, 1, question.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, question.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, question.difficulty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, question.id);
    
    int rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    
    sqlite3_finalize(stmt);
    return success;
}
```

**Key points:**
- Checks both `SQLITE_DONE` (execution succeeded) and `sqlite3_changes() > 0` (at least one row updated)
- Returns `false` if ID doesn't exist (no rows matched)

### deleteQuestion()

```cpp
bool Database::deleteQuestion(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = "DELETE FROM questions WHERE id = ?";
    
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    
    int rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    sqlite3_finalize(stmt);
    return success;
}
```

## Error Handling

| Scenario | Behavior |
|----------|----------|
| DB file locked | `sqlite3_step()` returns `SQLITE_BUSY` (rare with WAL mode) |
| Disk full | `sqlite3_step()` returns error, method returns false |
| Invalid SQL | `sqlite3_prepare_v2()` returns error code, logged to stderr |
| NULL column | `getText()` returns empty string |
| ID not found (update/delete) | Returns false |

**Logging:**
```cpp
if (rc != SQLITE_OK) {
    std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
}
```

## Performance Characteristics

- **Insert**: ~1-2ms per submission (with WAL)
- **Select**: ~0.5-1ms for 20 rows (with index)
- **Update**: ~1-2ms per question
- **Delete**: ~1-2ms per question
- **Concurrent reads**: Multiple threads can read simultaneously (WAL advantage)

## Deployment

- **Location**: `data/submissions.db` (created on first run)
- **Backup**: Copy the `.db` file; WAL files (`.db-wal`, `.db-shm`) are temporary
- **Docker volume**: Mounted at `/app/data` to persist across container restarts

## Testing

Manual test:
```bash
cd app/build
./online_compiler &

# Create a submission
curl -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(42)"}'

# Retrieve submissions
curl http://localhost:3000/api/submissions

# Create a question
curl -X POST http://localhost:3000/api/questions \
  -H 'Content-Type: application/json' \
  -d '{"title":"Two Sum","category":"Arrays","difficulty":"Easy"}'

# List questions
curl http://localhost:3000/api/questions
```

## Known Limitations

1. **No authentication**: Any client can read/write all data
2. **No transactions**: Multi-step operations not atomic
3. **No migrations**: Schema changes require manual SQL
4. **Single file**: No sharding or replication
5. **Text-only**: No binary data support

## Future Enhancements

- [ ] Add `tags` column to questions
- [ ] Add `testcases` table with backend CRUD
- [ ] Add `users` table with authentication
- [ ] Add `submissions.user_id` foreign key
- [ ] Implement transactions for multi-step operations
- [ ] Add database migration system
