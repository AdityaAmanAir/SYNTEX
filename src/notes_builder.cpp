#include "notes_builder.h"
#include "llm_client.h"
#include <sstream>
#include <stdexcept>
#include <iostream>

std::vector<std::string> chunkText(const std::string& text, size_t maxChars) {
    std::vector<std::string> chunks;
    if (text.empty()) return chunks;

    size_t start = 0;
    while (start < text.size()) {
        if (text.size() - start <= maxChars) {
            chunks.push_back(text.substr(start));
            break;
        }

        size_t end = start + maxChars;
        // Search backwards for a natural paragraph break
        size_t splitPoint = text.rfind("\n\n", end);
        if (splitPoint != std::string::npos && splitPoint > start + (maxChars / 2)) {
            end = splitPoint + 2;
        } else {
            // Search backwards for a newline
            splitPoint = text.rfind("\n", end);
            if (splitPoint != std::string::npos && splitPoint > start + (maxChars / 2)) {
                end = splitPoint + 1;
            } else {
                // Search backwards for space
                splitPoint = text.rfind(" ", end);
                if (splitPoint != std::string::npos && splitPoint > start + (maxChars / 2)) {
                    end = splitPoint + 1;
                }
            }
        }

        chunks.push_back(text.substr(start, end - start));
        start = end;
    }
    return chunks;
}

static std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return s;
}

std::string buildNotes(const std::string& rawText,
                       const std::string& subject,
                       const std::string& promptTemplate,
                       const std::string& level) {
    if (rawText.find_first_not_of(" \t\n\r") == std::string::npos) {
        throw std::runtime_error("Document contains no readable text or is empty.");
    }

    auto chunks = chunkText(rawText);
    if (chunks.empty()) {
        throw std::runtime_error("Document chunking produced no text chunks.");
    }

    std::cout << "[INFO] Processing document for subject: " << subject
              << " | Level: " << (level.empty() ? "Undergraduate" : level)
              << " | Chunks: " << chunks.size() << "\n";

    std::string combined;
    for (size_t i = 0; i < chunks.size(); ++i) {
        std::cout << "[INFO] Generating notes for chunk " << (i + 1) << "/" << chunks.size() << "...\n";
        std::string prompt = replaceAll(promptTemplate, "{subject}", subject);
        prompt = replaceAll(prompt, "{level}", level.empty() ? "Undergraduate" : level);
        prompt = replaceAll(prompt, "{raw_text}", chunks[i]);
        combined += callClaude(prompt) + "\n\n";
    }

    // Multi-chunk coherence pass
    if (chunks.size() > 1) {
        std::cout << "[INFO] Merging " << chunks.size() << " chunks into coherent final notes...\n";
        std::string mergePrompt =
            "Merge these separate note sections into one coherent, de-duplicated "
            "set of revision notes for a student studying " + subject + " (" +
            (level.empty() ? "Undergraduate" : level) +
            " level). Keep the same Markdown structure (## headers for topics, bullet points, "
            "**bold key terms**, ## Key Definitions, and ## 3 Likely Exam Questions):\n\n" +
            combined;
        return callClaude(mergePrompt);
    }

    return combined;
}
