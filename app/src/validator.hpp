#pragma once

#include <string>
#include "crow.h"
#include "languages.hpp"

struct ValidationResult {
    bool ok = false;
    std::string error;
    std::string language;
    std::string code;
    std::string input;
};

inline ValidationResult validateRunRequest(const crow::request& req) {
    ValidationResult result;

    auto body = crow::json::load(req.body);
    if (!body) {
        result.error = "Invalid JSON body";
        return result;
    }

    if (!body.has("language") || body["language"].t() != crow::json::type::String) {
        result.error = "Missing or invalid 'language' field";
        return result;
    }

    if (!body.has("code") || body["code"].t() != crow::json::type::String) {
        result.error = "Missing or invalid 'code' field";
        return result;
    }

    result.language = body["language"].s();
    result.code = body["code"].s();

    if (result.code.empty()) {
        result.error = "Empty 'code' field";
        return result;
    }

    const auto& langs = getLanguages();
    if (langs.find(result.language) == langs.end()) {
        result.error = "Unsupported language '" + result.language + "'";
        return result;
    }

    if (body.has("input") && body["input"].t() == crow::json::type::String) {
        result.input = body["input"].s();
    }

    result.ok = true;
    return result;
}
