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
            "gcc:14",
            "program.cpp",
            "g++ -o program program.cpp",
            "./program"
        }},
        {"python", {
            "Python",
            "python:3-alpine",
            "program.py",
            "",
            "python program.py"
        }},
        {"javascript", {
            "JavaScript",
            "node:20-alpine",
            "program.js",
            "",
            "node program.js"
        }},
    };
    return langs;
}

