#pragma once
#include <string>
#include <nlohmann/json.hpp>

// Invokes the Python Scikit-Learn ML document processor and returns structured JSON
nlohmann::json runPythonML(const std::string& rawText, const std::string& subject, const std::string& level);
