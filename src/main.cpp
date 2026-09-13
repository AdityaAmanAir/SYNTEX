#include "httplib.h"
#include "pdf_extract.h"
#include "notes_builder.h"
#include "env_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <random>

namespace fs = std::filesystem;
using json = nlohmann::json;

// In-memory cache for recent generated notes to support direct one-click downloads
static std::mutex cacheMutex;
static std::unordered_map<std::string, std::string> notesCache;

static std::string generateUniqueId() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    static std::mt19937_64 rng(std::random_device{}());
    uint64_t randVal = rng();
    return std::to_string(now) + "_" + std::to_string(randVal % 100000);
}

std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[WARN] Could not open " << path << ", using fallback prompt template.\n";
        return "You are an expert tutor turning lecture material into revision notes for {subject} at {level} level.\n"
               "Rules:\n- Markdown headers (##), bullet points.\n- Bold key terms.\n- Key Definitions.\n"
               "- 3 Likely Exam Questions.\n\nSource:\n{raw_text}";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct PipeCloser {
    void operator()(FILE* fp) const { if (fp) pclose(fp); }
};

static std::string runCommandAndGetOutput(const std::string& cmd) {
    std::array<char, 512> buffer;
    std::string result;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static bool convertMarkdownToExport(const std::string& mdPath, const std::string& outPath, const std::string& format) {
    std::string cmd;
    if (format == "pdf") {
        // Try direct pandoc to PDF first (uses pdflatex)
        cmd = "pandoc \"" + mdPath + "\" -o \"" + outPath + "\" --pdf-engine=pdflatex -V geometry:margin=1in 2>&1";
        std::string err = runCommandAndGetOutput(cmd);
        if (fs::exists(outPath) && fs::file_size(outPath) > 0) {
            return true;
        }

        // Fallback: pandoc to docx, then soffice to pdf
        std::string tempDocx = mdPath + ".docx";
        std::string docxCmd = "pandoc \"" + mdPath + "\" -o \"" + tempDocx + "\" 2>&1";
        runCommandAndGetOutput(docxCmd);
        if (fs::exists(tempDocx)) {
            std::string parentDir = fs::path(outPath).parent_path().string();
            std::string sofficeCmd = "soffice --headless --convert-to pdf \"" + tempDocx + "\" --outdir \"" + parentDir + "\" 2>&1";
            runCommandAndGetOutput(sofficeCmd);
            fs::remove(tempDocx);

            fs::path generatedPdf = fs::path(tempDocx).replace_extension(".pdf");
            if (fs::exists(generatedPdf)) {
                if (generatedPdf.string() != outPath) {
                    fs::rename(generatedPdf, outPath);
                }
                return true;
            }
        }
        return false;
    } else if (format == "docx") {
        cmd = "pandoc \"" + mdPath + "\" -o \"" + outPath + "\" 2>&1";
        runCommandAndGetOutput(cmd);
        return fs::exists(outPath) && fs::file_size(outPath) > 0;
    }
    return false;
}

int main() {
    // Load configuration from .env file if present
    loadEnvFile(".env");

    httplib::Server svr;

    // Multi-threaded task queue utilizing all hardware CPU cores
    unsigned int hardwareThreads = std::thread::hardware_concurrency();
    size_t threadCount = (hardwareThreads > 0) ? (hardwareThreads * 2) : 8;
    const char* threadsEnv = std::getenv("SERVER_THREADS");
    if (threadsEnv && *threadsEnv) {
        try { threadCount = std::stoul(threadsEnv); } catch (...) {}
    }
    svr.new_task_queue = [threadCount] {
        return new httplib::ThreadPool(threadCount);
    };

    std::string promptTemplate = loadFile("prompts/notes_prompt.txt");

    // Configurable max upload payload (default 25MB)
    int maxUploadMb = 25;
    const char* uploadEnv = std::getenv("MAX_UPLOAD_MB");
    if (uploadEnv && *uploadEnv) {
        try { maxUploadMb = std::stoi(uploadEnv); } catch (...) {}
    }
    svr.set_payload_max_length(maxUploadMb * 1024 * 1024);

    // Serve frontend static files
    svr.set_mount_point("/", "./web");

    // Health check endpoint
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
        const char* mockEnv = std::getenv("MOCK_LLM");
        bool isMock = (mockEnv && (std::string(mockEnv) == "1" || std::string(mockEnv) == "true")) ||
                      (!apiKey || std::string(apiKey).empty() ||
                       std::string(apiKey).find("your-") != std::string::npos ||
                       std::string(apiKey).find("your_key") != std::string::npos);

        json h = {
            {"status", "healthy"},
            {"service", "ai-student-workspace"},
            {"testing_mode", isMock}
        };
        res.set_content(h.dump(), "application/json");
    });

    // Configuration / status endpoint for frontend badge & settings
    svr.Get("/config", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
        const char* mockEnv = std::getenv("MOCK_LLM");
        bool isMock = (mockEnv && (std::string(mockEnv) == "1" || std::string(mockEnv) == "true")) ||
                      (!apiKey || std::string(apiKey).empty() ||
                       std::string(apiKey).find("your-") != std::string::npos ||
                       std::string(apiKey).find("your_key") != std::string::npos);

        const char* modelEnv = std::getenv("ANTHROPIC_MODEL");
        std::string model = (modelEnv && *modelEnv) ? modelEnv : "claude-3-5-sonnet-20241022";

        json cfg = {
            {"testing_mode", isMock},
            {"model", model},
            {"has_api_key", !isMock}
        };
        res.set_content(cfg.dump(), "application/json");
    });

    // POST /generate : Core endpoint
    svr.Post("/generate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        auto fileIt = req.files.find("lecture");
        if (fileIt == req.files.end()) {
            res.status = 400;
            res.set_content("{\"error\":\"No lecture file uploaded. Please attach a PDF, DOCX, or PPTX.\"}", "application/json");
            return;
        }

        const auto& file = fileIt->second;
        if (file.content.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Uploaded file is empty.\"}", "application/json");
            return;
        }

        std::string subject = "Computer Science";
        if (req.has_param("subject")) {
            subject = req.get_param_value("subject");
        } else {
            auto subIt = req.files.find("subject");
            if (subIt != req.files.end()) {
                subject = subIt->second.content;
            }
        }

        std::string level = "Undergraduate";
        if (req.has_param("level")) {
            level = req.get_param_value("level");
        } else {
            auto lvlIt = req.files.find("level");
            if (lvlIt != req.files.end()) {
                level = lvlIt->second.content;
            }
        }

        // Determine extension from original filename
        std::string origFilename = file.filename;
        std::string ext = fs::path(origFilename).extension().string();
        if (ext.empty()) ext = ".pdf";

        // Safe unique temporary filename
        std::string uniqueId = generateUniqueId();
        std::string tmpUploadPath = "/tmp/upload_" + uniqueId + ext;

        std::ofstream out(tmpUploadPath, std::ios::binary);
        if (!out.is_open()) {
            res.status = 500;
            res.set_content("{\"error\":\"Failed to save temporary upload on server.\"}", "application/json");
            return;
        }
        out.write(file.content.data(), file.content.size());
        out.close();

        try {
            std::cout << "\n[REQUEST] New generation request | File: " << origFilename
                      << " (" << file.content.size() << " bytes) | Subject: " << subject
                      << " | Level: " << level << "\n";

            // Multi-format extract (PDF, PPTX, DOCX, TXT)
            std::string rawText = extractTextFromFile(tmpUploadPath);

            // Clean up upload temp file immediately
            std::error_code ec;
            fs::remove(tmpUploadPath, ec);

            // Build revision notes
            std::string notes = buildNotes(rawText, subject, promptTemplate, level);

            // Cache notes for one-click downloads
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                if (notesCache.size() > 100) {
                    notesCache.erase(notesCache.begin());
                }
                notesCache[uniqueId] = notes;
            }

            json respJson = {
                {"success", true},
                {"id", uniqueId},
                {"subject", subject},
                {"level", level},
                {"notes", notes}
            };
            res.set_content(respJson.dump(), "application/json");

        } catch (const std::exception& e) {
            std::error_code ec;
            fs::remove(tmpUploadPath, ec);
            std::cerr << "[ERROR] Generation failed: " << e.what() << "\n";
            res.status = 500;
            json errJson = {{"error", e.what()}};
            res.set_content(errJson.dump(), "application/json");
        }
    });

    // GET /download?id=...&format=pdf|docx|md
    svr.Get("/download", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string id = req.has_param("id") ? req.get_param_value("id") : "";
        std::string format = req.has_param("format") ? req.get_param_value("format") : "md";
        std::string title = req.has_param("title") ? req.get_param_value("title") : "revision_notes";

        std::string notes;
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            auto it = notesCache.find(id);
            if (it != notesCache.end()) {
                notes = it->second;
            }
        }

        if (notes.empty()) {
            res.status = 404;
            res.set_content("{\"error\":\"Notes session not found or expired.\"}", "application/json");
            return;
        }

        if (format == "md") {
            res.set_header("Content-Disposition", "attachment; filename=\"" + title + ".md\"");
            res.set_content(notes, "text/markdown; charset=utf-8");
            return;
        }

        std::string tmpMd = "/tmp/export_" + id + ".md";
        std::string tmpOut = "/tmp/export_" + id + "." + format;

        {
            std::ofstream mdFile(tmpMd);
            mdFile << notes;
        }

        bool ok = convertMarkdownToExport(tmpMd, tmpOut, format);
        std::error_code ec;
        fs::remove(tmpMd, ec);

        if (!ok || !fs::exists(tmpOut)) {
            fs::remove(tmpOut, ec);
            res.status = 500;
            res.set_content("{\"error\":\"Failed to render document format: " + format + "\"}", "application/json");
            return;
        }

        std::ifstream inFile(tmpOut, std::ios::binary);
        std::stringstream ss;
        ss << inFile.rdbuf();
        inFile.close();
        fs::remove(tmpOut, ec);

        std::string fileData = ss.str();
        std::string contentType = (format == "pdf") ? "application/pdf"
                                                    : "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

        res.set_header("Content-Disposition", "attachment; filename=\"" + title + "." + format + "\"");
        res.set_content(fileData, contentType);
    });

    // POST /export : direct export from markdown in body
    svr.Post("/export", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string notes;
        std::string format = "pdf";
        std::string title = "revision_notes";

        try {
            json body = json::parse(req.body);
            if (body.contains("notes")) notes = body["notes"].get<std::string>();
            if (body.contains("format")) format = body["format"].get<std::string>();
            if (body.contains("title")) title = body["title"].get<std::string>();
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON in export request\"}", "application/json");
            return;
        }

        if (notes.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Notes content cannot be empty\"}", "application/json");
            return;
        }

        if (format == "md") {
            res.set_header("Content-Disposition", "attachment; filename=\"" + title + ".md\"");
            res.set_content(notes, "text/markdown; charset=utf-8");
            return;
        }

        std::string uniqueId = generateUniqueId();
        std::string tmpMd = "/tmp/export_" + uniqueId + ".md";
        std::string tmpOut = "/tmp/export_" + uniqueId + "." + format;

        {
            std::ofstream mdFile(tmpMd);
            mdFile << notes;
        }

        bool ok = convertMarkdownToExport(tmpMd, tmpOut, format);
        std::error_code ec;
        fs::remove(tmpMd, ec);

        if (!ok || !fs::exists(tmpOut)) {
            fs::remove(tmpOut, ec);
            res.status = 500;
            res.set_content("{\"error\":\"Failed to generate " + format + "\"}", "application/json");
            return;
        }

        std::ifstream inFile(tmpOut, std::ios::binary);
        std::stringstream ss;
        ss << inFile.rdbuf();
        inFile.close();
        fs::remove(tmpOut, ec);

        std::string fileData = ss.str();
        std::string contentType = (format == "pdf") ? "application/pdf"
                                                    : "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

        res.set_header("Content-Disposition", "attachment; filename=\"" + title + "." + format + "\"");
        res.set_content(fileData, contentType);
    });

    // Configurable host and port
    std::string host = "0.0.0.0";
    const char* hostEnv = std::getenv("HOST");
    if (hostEnv && *hostEnv) host = hostEnv;

    int port = 80;
    const char* portEnv = std::getenv("PORT");
    if (portEnv && *portEnv) {
        try {
            port = std::stoi(portEnv);
        } catch (...) {
            port = 80;
        }
    }

    const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
    const char* mockEnv = std::getenv("MOCK_LLM");
    bool isMock = (mockEnv && (std::string(mockEnv) == "1" || std::string(mockEnv) == "true")) ||
                  (!apiKey || std::string(apiKey).empty() ||
                   std::string(apiKey).find("your-") != std::string::npos ||
                   std::string(apiKey).find("your_key") != std::string::npos);

    const char* modelEnv = std::getenv("ANTHROPIC_MODEL");
    std::string model = (modelEnv && *modelEnv) ? modelEnv : "claude-3-5-sonnet-20241022";

    std::cout << "=================================================\n";
    std::cout << "  AI-Powered Student Workspace Backend (C++17)   \n";
    std::cout << "  Concurrency: " << threadCount << " worker threads (" << hardwareThreads << " CPU cores detected)\n";
    std::cout << "  Target Port: " << port << " | Host: " << host << "\n";
    std::cout << "  Mode: " << (isMock ? "Local Testing Mode (Zero-cost extractive synthesis)" : "Production Mode (Claude API connected)") << "\n";
    if (!isMock) std::cout << "  Model: " << model << "\n";
    std::cout << "  Export formats: Markdown (.md), PDF (.pdf), Word (.docx)\n";
    std::cout << "  Input formats: PDF, DOCX, PPTX, TXT, MD\n";
    std::cout << "=================================================\n";

    std::cout << "Binding HTTP server to " << host << ":" << port << "...\n";
    if (!svr.listen(host.c_str(), port)) {
        if (port == 80) {
            std::cerr << "\n[NOTICE] Direct binding to Port 80 requires elevated system privileges.\n";
            std::cerr << "         To run directly on port 80 without root, execute:\n";
            std::cerr << "           sudo setcap 'cap_net_bind_service=+ep' ./build/server\n";
            std::cerr << "         Or run with sudo:\n";
            std::cerr << "           sudo ./build/server\n\n";
            std::cerr << "         Switching automatically to fallback development port 8080...\n\n";
            port = 8080;
            std::cout << "Binding HTTP server to " << host << ":" << port << "...\n";
            if (!svr.listen(host.c_str(), port)) {
                std::cerr << "[FATAL] Failed to bind to port " << port << "\n";
                return 1;
            }
        } else {
            std::cerr << "[FATAL] Failed to bind to " << host << ":" << port << "\n";
            return 1;
        }
    }

    return 0;
}
