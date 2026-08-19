#pragma once

#include <string>
#include <unordered_map>

struct LangConfig {
    std::string name;
    std::string image;
    std::string fileName;
    std::string compileCmd;   // empty if interpreted
    std::string runCmd;
};

inline const std::unordered_map<std::string, LangConfig>& getLanguages() {
    static const std::unordered_map<std::string, LangConfig> langs = {
        {"cpp", {
            "C++",
            "gcc:14-bookworm",
            "program.cpp",
            "g++ -O2 -std=c++17 program.cpp -o program",
            "./program"
        }},
        {"python", {
            "Python",
            "python:3.12-slim",
            "program.py",
            "",
            "python3 program.py"
        }},
        {"javascript", {
            "JavaScript",
            "node:20-slim",
            "program.js",
            "",
            "node program.js"
        }},
    };
    return langs;
}
