#pragma once
#include <string>
#include <fstream>
#include <cstdlib>
#include <iostream>

inline void loadEnvFile(const std::string& path = ".env") {
    std::string actualPath = path;
    std::ifstream file(actualPath);
    if (!file.is_open()) {
        actualPath = "../" + path;
        file.open(actualPath);
    }
    if (!file.is_open()) {
        std::cout << "[CONFIG] No " << path << " file found. Using environment defaults.\n";
        return;
    }

    std::cout << "[CONFIG] Loading environment configuration from " << actualPath << "...\n";
    std::string line;
    while (std::getline(file, line)) {
        // Strip carriage returns and leading spaces
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        if (line[first] == '#') continue; // Skip comments

        size_t eq = line.find('=', first);
        if (eq == std::string::npos) continue;

        std::string key = line.substr(first, eq - first);
        // Trim key
        size_t lastKey = key.find_last_not_of(" \t\r\n");
        if (lastKey != std::string::npos) key = key.substr(0, lastKey + 1);

        std::string val = line.substr(eq + 1);
        // Trim value
        size_t firstVal = val.find_first_not_of(" \t\r\n");
        if (firstVal != std::string::npos) {
            val = val.substr(firstVal);
            size_t lastVal = val.find_last_not_of(" \t\r\n");
            if (lastVal != std::string::npos) val = val.substr(0, lastVal + 1);
            // Strip quotes if wrapped
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
        } else {
            val = "";
        }

        if (!key.empty()) {
            setenv(key.c_str(), val.c_str(), 1);
        }
    }
}
