#pragma once
#include <string>

// Sends a prompt to the active LLM provider (Groq, Anthropic, or Local Testing Generator)
std::string callLLM(const std::string& prompt);

// Backward-compatible alias
inline std::string callClaude(const std::string& prompt) {
    return callLLM(prompt);
}

// Provider & diagnostic queries
std::string getActiveLLMProvider();
std::string getActiveLLMModel();
bool isLocalTestingMode();
