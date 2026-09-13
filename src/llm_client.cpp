#include "llm_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <regex>
#include <algorithm>

using json = nlohmann::json;

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Intelligent extractive generator for testing mode (generates notes directly from uploaded content without an API key)
static std::string generateIntelligentTestNotes(const std::string& prompt) {
    std::string subject = "General Studies";
    std::string level = "Undergraduate";
    std::string sourceText;

    // Extract subject
    size_t subjPos = prompt.find("studying ");
    if (subjPos != std::string::npos) {
        size_t endPos = prompt.find(" at ", subjPos);
        if (endPos != std::string::npos) {
            subject = prompt.substr(subjPos + 9, endPos - (subjPos + 9));
            size_t lvlEnd = prompt.find(" level", endPos);
            if (lvlEnd != std::string::npos) {
                level = prompt.substr(endPos + 4, lvlEnd - (endPos + 4));
            }
        }
    }

    // Extract source text from prompt
    size_t srcStart = prompt.find("Source material:\n---\n");
    if (srcStart != std::string::npos) {
        srcStart += 21;
        size_t srcEnd = prompt.rfind("\n---");
        if (srcEnd != std::string::npos && srcEnd > srcStart) {
            sourceText = prompt.substr(srcStart, srcEnd - srcStart);
        } else {
            sourceText = prompt.substr(srcStart);
        }
    } else {
        sourceText = prompt;
    }

    // Parse sentences and paragraphs from sourceText
    std::vector<std::string> paragraphs;
    std::stringstream ss(sourceText);
    std::string line;
    std::string currentPara;

    while (std::getline(ss, line)) {
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) {
            if (!currentPara.empty()) {
                paragraphs.push_back(currentPara);
                currentPara.clear();
            }
        } else {
            std::string trimmed = line.substr(start);
            if (!currentPara.empty()) currentPara += " ";
            currentPara += trimmed;
        }
    }
    if (!currentPara.empty()) paragraphs.push_back(currentPara);

    // Collect candidate terms and key sentences
    std::vector<std::pair<std::string, std::string>> definitions;
    std::vector<std::string> bulletPoints;

    std::regex defRegex(R"(([A-Z][A-Za-z0-9\s\-]{2,30})\s*(?::|—|-|\bis\b|\bmeans\b)\s*([^.]{15,140}\.))");
    std::smatch match;

    for (const auto& para : paragraphs) {
        // Look for definitions
        std::string searchStr = para;
        while (std::regex_search(searchStr, match, defRegex)) {
            if (definitions.size() < 6) {
                std::string term = match[1].str();
                std::string def = match[2].str();
                // Clean term
                term.erase(term.find_last_not_of(" \t") + 1);
                definitions.push_back({term, def});
            }
            searchStr = match.suffix().str();
        }

        // Add interesting sentences as bullet points
        if (para.length() > 40 && bulletPoints.size() < 8) {
            size_t firstDot = para.find(". ");
            if (firstDot != std::string::npos && firstDot < 250) {
                bulletPoints.push_back(para.substr(0, firstDot + 1));
            } else if (para.length() < 250) {
                bulletPoints.push_back(para);
            }
        }
    }

    // Default fallbacks if regex yielded few items
    if (definitions.empty()) {
        definitions.push_back({subject + " Core Model", "The primary conceptual framework described in the lecture material."});
        definitions.push_back({"System Constraints", "The operational boundaries and invariant rules governing execution."});
        definitions.push_back({"Invariant Principle", "A condition that remains consistently true throughout process life cycles."});
    }
    if (bulletPoints.empty()) {
        bulletPoints.push_back("Covers foundational mechanisms and core theoretical models presented in the lecture.");
        bulletPoints.push_back("Emphasizes structural organization, operational flow, and trade-off analysis.");
        bulletPoints.push_back("Highlights critical verification boundaries and runtime fault tolerance requirements.");
    }

    // Build structured exam revision notes
    std::stringstream out;
    out << "> 💡 **Notice**: Generated in **Testing Mode** (No Anthropic API key required). Set `ANTHROPIC_API_KEY` in `.env` for production Claude AI generation.\n\n";

    out << "## 1. Core Principles & Foundational Architecture\n\n";
    out << "* **Discipline Context**: Exam review notes for **" << subject << "** (Academic Level: **" << level << "**).\n";
    for (size_t i = 0; i < bulletPoints.size() && i < 3; ++i) {
        out << "* **Key Focus " << (i + 1) << "**: " << bulletPoints[i] << "\n";
    }

    out << "\n## 2. In-Depth Technical Concepts & Mechanisms\n\n";
    for (size_t i = 3; i < bulletPoints.size(); ++i) {
        out << "* " << bulletPoints[i] << "\n";
    }
    if (bulletPoints.size() <= 3) {
        out << "* **Operational Workflow**: Sequential execution designed for determinism and error mitigation.\n";
        out << "* **Performance Trade-offs**: Balances processing latency against memory and computational footprint.\n";
    }

    out << "\n## Key Definitions\n\n";
    for (const auto& d : definitions) {
        out << "* **" << d.first << "**: " << d.second << "\n";
    }

    out << "\n## 3 Likely Exam Questions\n\n";
    out << "1. How do the primary principles of **" << subject << "** resolve boundary conditions outlined in this lecture?\n";
    out << "2. Explain the operational trade-offs of **" << definitions[0].first << "** in practical implementations.\n";
    out << "3. How does the system guarantee data consistency and recover from unexpected execution faults?\n";

    return out.str();
}

std::string callClaude(const std::string& prompt) {
    const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
    const char* mockEnv = std::getenv("MOCK_LLM");
    const char* demoEnv = std::getenv("DEMO_MODE");

    bool mockRequested = (mockEnv && (std::string(mockEnv) == "1" || std::string(mockEnv) == "true")) ||
                         (demoEnv && (std::string(demoEnv) == "1" || std::string(demoEnv) == "true"));

    // If no API key is provided, or is empty / default placeholder, run in test mode automatically
    bool keyMissing = (!apiKey || std::string(apiKey).empty() ||
                       std::string(apiKey).find("your-") != std::string::npos ||
                       std::string(apiKey).find("your_key") != std::string::npos);

    if (mockRequested || keyMissing) {
        std::cout << "[TEST MODE] Running intelligent extractive note generator (Testing mode active)\n";
        return generateIntelligentTestNotes(prompt);
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl_easy_init failed");
    }

    const char* modelEnv = std::getenv("ANTHROPIC_MODEL");
    std::string model = (modelEnv && *modelEnv) ? modelEnv : "claude-3-5-sonnet-20241022";

    json body = {
        {"model", model},
        {"max_tokens", 4000},
        {"messages", {{{"role", "user"}, {"content", prompt}}}}
    };

    std::string bodyStr = body.dump();
    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("x-api-key: " + std::string(apiKey)).c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 90L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("Anthropic API request failed: ") + curl_easy_strerror(res));
    }

    json parsed;
    try {
        parsed = json::parse(response);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse Anthropic API response as JSON: " + response);
    }

    if (httpCode != 200) {
        if (parsed.contains("error") && parsed["error"].is_object() && parsed["error"].contains("message")) {
            throw std::runtime_error("Anthropic API error (" + std::to_string(httpCode) + "): " +
                                     parsed["error"]["message"].get<std::string>());
        }
        throw std::runtime_error("Anthropic API HTTP " + std::to_string(httpCode) + ": " + response);
    }

    if (!parsed.contains("content") || !parsed["content"].is_array() || parsed["content"].empty() ||
        !parsed["content"][0].contains("text")) {
        throw std::runtime_error("Anthropic API response missing 'content[0].text' field");
    }

    return parsed["content"][0]["text"].get<std::string>();
}
