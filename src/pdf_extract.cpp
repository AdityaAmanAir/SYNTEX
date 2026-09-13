#include "pdf_extract.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

struct PipeCloser {
    void operator()(FILE* fp) const { if (fp) pclose(fp); }
};

std::string extractTextFromPDF(const std::string& pdfPath) {
    std::string cmd = "pdftotext -layout \"" + pdfPath + "\" -";
    std::array<char, 4096> buffer;
    std::string result;

    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("pdftotext failed to start");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Check if result has any non-whitespace content
    bool hasContent = false;
    for (char c : result) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            hasContent = true;
            break;
        }
    }

    if (!hasContent) {
        throw std::runtime_error(
            "No extractable text found in PDF. The document may be empty, scanned/image-only, or corrupted."
        );
    }

    return result;
}

std::string convertOfficeToPDF(const std::string& inputPath, const std::string& outDir) {
    fs::path inPath(inputPath);
    std::string stem = inPath.stem().string();
    fs::path expectedPdf = fs::path(outDir) / (stem + ".pdf");

    // Remove any stale file with same name
    std::error_code ec;
    fs::remove(expectedPdf, ec);

    std::string cmd = "soffice --headless --convert-to pdf \"" + inputPath + "\" --outdir \"" + outDir + "\" 2>&1";
    std::array<char, 1024> buffer;
    std::string output;

    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("soffice failed to start for document conversion");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    if (!fs::exists(expectedPdf)) {
        throw std::runtime_error("Document conversion to PDF failed: " + output);
    }

    return expectedPdf.string();
}

std::string extractTextFromFile(const std::string& filePath) {
    fs::path p(filePath);
    std::string ext = toLower(p.extension().string());

    if (ext == ".pdf") {
        return extractTextFromPDF(filePath);
    }

    if (ext == ".txt" || ext == ".md" || ext == ".markdown") {
        std::ifstream f(filePath);
        if (!f.is_open()) {
            throw std::runtime_error("Failed to open text file: " + filePath);
        }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        if (content.find_first_not_of(" \t\n\r") == std::string::npos) {
            throw std::runtime_error("The uploaded file is empty.");
        }
        return content;
    }

    if (ext == ".docx" || ext == ".pptx" || ext == ".ppt" || ext == ".doc" || ext == ".odt" || ext == ".rtf") {
        std::string parentDir = p.parent_path().string();
        if (parentDir.empty()) parentDir = "/tmp";
        std::string convertedPdf = convertOfficeToPDF(filePath, parentDir);

        try {
            std::string text = extractTextFromPDF(convertedPdf);
            std::error_code ec;
            fs::remove(convertedPdf, ec);
            return text;
        } catch (...) {
            std::error_code ec;
            fs::remove(convertedPdf, ec);
            throw;
        }
    }

    // Default fallback: attempt PDF extraction directly
    return extractTextFromPDF(filePath);
}
