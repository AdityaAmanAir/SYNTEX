#pragma once
#include <string>

// Extracts plain text from a PDF file using pdftotext -layout
std::string extractTextFromPDF(const std::string& pdfPath);

// Converts office documents (DOCX, PPTX, PPT, DOC, ODT, etc.) to PDF via headless LibreOffice
std::string convertOfficeToPDF(const std::string& inputPath, const std::string& outDir = "/tmp");

// Unified text extraction handling PDF, DOCX, PPTX, TXT, MD, etc.
std::string extractTextFromFile(const std::string& filePath);
