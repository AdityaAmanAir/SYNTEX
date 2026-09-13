#include "pdf_extract.h"
#include "notes_builder.h"
#include "llm_client.h"
#include "env_loader.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    loadEnvFile(".env");

    if (argc < 2) {
        std::cout << "Usage: ./cli_test <document-file> [subject] [level]\n";
        std::cout << "Example: ./cli_test sample.pdf \"Computer Science\" \"Advanced\"\n";
        return 1;
    }

    std::string filePath = argv[1];
    std::string subject = (argc >= 3) ? argv[2] : "Computer Science";
    std::string level = (argc >= 4) ? argv[3] : "Undergraduate";

    std::cout << "======================================\n";
    std::cout << "Running Standalone Pipeline Test\n";
    std::cout << "Input Document: " << filePath << "\n";
    std::cout << "Subject: " << subject << " | Level: " << level << "\n";
    std::cout << "======================================\n";

    try {
        std::cout << "\n[Step 1] Extracting text...\n";
        std::string rawText = extractTextFromFile(filePath);
        std::cout << "Extracted " << rawText.size() << " characters of text.\n";

        std::cout << "\n[Step 2] Loading prompt template...\n";
        std::ifstream promptFile("prompts/notes_prompt.txt");
        std::stringstream ss;
        if (promptFile.is_open()) {
            ss << promptFile.rdbuf();
        } else {
            ss << "You are an expert tutor turning lecture material into revision notes for {subject} at {level} level.\n"
               << "Rules:\n- Markdown headers (##), bullet points.\n- Bold key terms.\n- Key Definitions.\n"
               << "- 3 Likely Exam Questions.\n\nSource:\n{raw_text}";
        }
        std::string promptTemplate = ss.str();

        std::cout << "\n[Step 3] Building revision notes...\n";
        std::string notes = buildNotes(rawText, subject, promptTemplate, level);

        std::cout << "\n================ RESULT ================\n";
        std::cout << notes << "\n";
        std::cout << "========================================\n";
        std::cout << "Pipeline Test Passed Successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "\n[FAILURE] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
