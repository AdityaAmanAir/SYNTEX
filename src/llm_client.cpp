#include "llm_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static std::string generateMockNotes(const std::string& prompt) {
    std::string subject = "Computer Science";
    size_t subjPos = prompt.find("studying ");
    if (subjPos != std::string::npos) {
        size_t endPos = prompt.find(" at ", subjPos);
        if (endPos == std::string::npos) endPos = prompt.find(".", subjPos);
        if (endPos != std::string::npos) {
            subject = prompt.substr(subjPos + 9, endPos - (subjPos + 9));
        }
    }

    return "## Overview & Core Concepts\n\n"
           "* **Core Foundation**: This lecture covers fundamental principles of **" + subject + "**.\n"
           "* **System Design**: Emphasizes modular architecture, clean separation of concerns, and robust error handling.\n"
           "* **Performance Optimization**: Balancing computational complexity with practical resource utilization.\n\n"
           "## Detailed Analysis\n\n"
           "* **Key Mechanisms**: Execution pipelines are designed for low latency and high throughput.\n"
           "* **Validation & Constraints**: All inputs are checked prior to dispatch to prevent runtime hazards.\n"
           "* **Integration Points**: Seamless communication across interfaces using standard protocols.\n\n"
           "## Key Definitions\n\n"
           "* **Throughput**: Rate at which a system processes successful work units over time.\n"
           "* **Latency**: Time interval between an initial request and the complete corresponding response.\n"
           "* **Idempotence**: Property where multiple identical requests produce the exact same outcome as a single request.\n\n"
           "## 3 Likely Exam Questions\n\n"
           "1. How does the architecture maintain data consistency under high concurrent load?\n"
           "2. Compare and contrast the trade-offs between batch processing and stream processing in this system.\n"
           "3. What failure recovery strategies are employed when an intermediate pipeline step errors out?\n";
}

std::string callClaude(const std::string& prompt) {
    const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
    const char* mockEnv = std::getenv("MOCK_LLM");
    const char* demoEnv = std::getenv("DEMO_MODE");

    bool isMock = (mockEnv && (std::string(mockEnv) == "1" || std::string(mockEnv) == "true")) ||
                  (demoEnv && (std::string(demoEnv) == "1" || std::string(demoEnv) == "true"));

    if (!apiKey || std::string(apiKey).empty()) {
        if (isMock) {
            std::cout << "[INFO] ANTHROPIC_API_KEY not set, using Mock LLM mode (MOCK_LLM=1)\n";
            return generateMockNotes(prompt);
        }
        throw std::runtime_error("ANTHROPIC_API_KEY environment variable not set. Please set your API key or export MOCK_LLM=1 for offline testing.");
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 90L); // 90s timeout for LLM responses

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
