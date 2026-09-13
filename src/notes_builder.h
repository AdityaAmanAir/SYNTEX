#pragma once
#include <string>
#include <vector>

std::vector<std::string> chunkText(const std::string& text, size_t maxChars = 6000);

std::string buildNotes(const std::string& rawText,
                       const std::string& subject,
                       const std::string& promptTemplate,
                       const std::string& level = "Undergraduate");
