#pragma once
#include <string>

// Sends a prompt to the Anthropic Messages API (Claude) and returns the generated text response
std::string callClaude(const std::string& prompt);
