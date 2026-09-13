#include "ml_bridge.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <array>
#include <chrono>
#include <random>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct PipeCloser {
    void operator()(FILE* f) const {
        if (f) pclose(f);
    }
};

static std::string generateBridgeId() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    static std::mt19937_64 rng(std::random_device{}());
    return std::to_string(now) + "_" + std::to_string(rng() % 10000);
}

static std::string escapeArg(const std::string& str) {
    std::string escaped = "\"";
    for (char c : str) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            escaped += '\\';
        }
        escaped += c;
    }
    escaped += "\"";
    return escaped;
}

json runPythonML(const std::string& rawText, const std::string& subject, const std::string& level) {
    // 1. Locate scripts/ml_processor.py
    std::string scriptPath = "scripts/ml_processor.py";
    if (!fs::exists(scriptPath) && fs::exists("../scripts/ml_processor.py")) {
        scriptPath = "../scripts/ml_processor.py";
    }

    if (!fs::exists(scriptPath)) {
        std::cerr << "[WARN] ML script not found at " << scriptPath << ". Using basic statistics.\n";
        return {
            {"status", "fallback"},
            {"engine", "C++ internal fallback"},
            {"stats", {
                {"total_words", rawText.size() / 6},
                {"subject", subject},
                {"academic_level", level}
            }},
            {"top_keywords", json::array()},
            {"extractive_summary", json::array()},
            {"key_concepts", json::array()}
        };
    }

    // 2. Write text to safe temporary file
    std::string tmpId = generateBridgeId();
    std::string tmpFile = "/tmp/ml_input_" + tmpId + ".txt";
    {
        std::ofstream out(tmpFile, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Could not write temporary ML input file.\n";
            return {{"status", "error"}, {"message", "Temp file error"}};
        }
        out.write(rawText.data(), rawText.size());
    }

    // 3. Construct execution command
    std::string cmd = "python3 " + scriptPath +
                      " --text-file " + tmpFile +
                      " --subject " + escapeArg(subject) +
                      " --level " + escapeArg(level) + " 2>&1";

    std::string output;
    {
        std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
        if (!pipe) {
            std::error_code ec;
            fs::remove(tmpFile, ec);
            std::cerr << "[ERROR] Failed to execute Python ML process.\n";
            return {{"status", "error"}, {"message", "Process execution failed"}};
        }

        std::array<char, 2048> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            output += buffer.data();
        }
    }

    // Safely remove temporary file after Python process has completed reading
    std::error_code ec;
    fs::remove(tmpFile, ec);

    // 4. Parse JSON output from Python
    try {
        // Find JSON start '{' in case any warning lines were printed before stdout
        size_t jsonStart = output.find('{');
        if (jsonStart != std::string::npos) {
            output = output.substr(jsonStart);
        }
        json mlResult = json::parse(output);
        std::cout << "[ML BRIDGE] Python ML analysis completed successfully ("
                  << mlResult.value("engine", "scikit-learn") << ").\n";
        return mlResult;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse Python ML output JSON: " << e.what() << "\n";
        std::cerr << "[DEBUG] Raw output was: " << output.substr(0, 300) << "...\n";
        return {
            {"status", "parse_error"},
            {"engine", "fallback"},
            {"stats", {
                {"total_words", rawText.size() / 6},
                {"subject", subject},
                {"academic_level", level}
            }},
            {"top_keywords", json::array()},
            {"extractive_summary", json::array()},
            {"key_concepts", json::array()}
        };
    }
}
