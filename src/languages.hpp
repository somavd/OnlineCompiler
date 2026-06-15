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
            "gcc:latest",
            "program.cpp",
            "g++ program.cpp -o program",
            "./program"
        }},
        {"python", {
            "Python",
            "python:3-slim",
            "program.py",
            "",
            "python program.py"
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

