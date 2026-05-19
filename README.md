# Online Compiler

A web-based code compiler that allows users to write, compile, and execute C++ code directly in the browser. The application uses Docker containers for secure, isolated code execution.

## Features

- **Web-based Code Editor**: Interactive code editor with syntax highlighting using CodeMirror
- **C++ Support**: Compile and execute C++ code with gcc compiler
- **Secure Execution**: Code runs in isolated Docker containers for security
- **Real-time Output**: View compilation errors and program output immediately
- **RESTful API**: Clean API for code compilation and execution

## Tech Stack

- **Backend**: C++ with Crow web framework
- **Frontend**: HTML, JavaScript, CodeMirror for syntax highlighting
- **Containerization**: Docker for isolated code execution
- **Compiler**: GCC (via Docker image `gcc:latest`)

## Requirements

- C++ compiler with C++17 support
- Crow web framework
- Docker installed and running
- Linux/macOS (for Docker volume mounting)

## Installation

1. Clone the repository:
```bash
git clone <repository-url>
cd OnlineCompiler
```

2. Install Crow framework (if not already installed):
```bash
# Using package manager or build from source
# Refer to Crow documentation for installation
```

3. Ensure Docker is running:
```bash
docker --version
```

4. Build the server:
```bash
g++ -std=c++17 ServerCompiler.cpp -o Server -l crow -pthread
```

## Usage

1. Start the server:
```bash
./Server
```

2. Open your browser and navigate to:
```
http://localhost:18080
```

3. Write C++ code in the editor and click "Run" to compile and execute

## API Endpoints

### GET /
Serves the web-based code editor interface.

### POST /run
Compiles and executes the submitted code.

**Request Body:**
```json
{
  "language": "cpp",
  "code": "#include <iostream>\nint main() {\n    std::cout << \"Hello, World!\";\n    return 0;\n}"
}
```

**Response:**
```json
{
  "stdout": "Hello, World!",
  "stderr": ""
}
```

## Project Structure

```
OnlineCompiler/
├── ServerCompiler.cpp    # Main server implementation
├── Server                # Compiled executable
├── htmls/
│   └── editor.html       # Web-based code editor
├── tmp/                  # Temporary directory for code execution (created at runtime)
└── README.md
```

## How It Works

1. User submits code via the web editor
2. Server generates a unique ID for the execution session
3. Code is saved to a temporary directory
4. Docker container mounts the temp directory and compiles the code
5. If compilation succeeds, the program is executed
6. Output and errors are captured and returned to the user
7. Temporary files are cleaned up after execution

## Security

- Code execution is isolated in Docker containers
- Each execution uses a unique temporary directory
- Containers are removed after execution (`--rm` flag)
- No persistent storage of user code

## Current Limitations

- Only C++ language is supported
- No timeout enforcement for long-running programs
- No resource limits (CPU/memory) configured

## Future Enhancements

- Support for additional languages (Python, Java, etc.)
- Add execution timeout and resource limits
- User authentication and session management
- Code history and saved programs
- Enhanced error reporting and debugging features

## License

[Add your license information here]

## Author

[Add your name/contact information here]