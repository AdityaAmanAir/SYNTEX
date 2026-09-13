# AI-Powered Student Workspace (C++17)

> **Lecture PDF & Slides → Exam-Ready Revision Notes**  
> Built genuinely in high-performance C++17 with an interactive web UI, multi-format ingestion, chunked multi-pass LLM synthesis, and 1-click publication export (PDF, Word, Markdown).

---

## 🏗️ Architecture

```
[ Browser: web/index.html ]  <--- Drag & Drop (PDF, DOCX, PPTX, TXT)
            |
            |  POST /generate (Multipart Form Data)
            v
[ C++ Backend: cpp-httplib Server ]
            |
            +---> Multi-format Ingestion:
            |       - .pdf       --> pdftotext -layout (poppler-utils)
            |       - .docx/.pptx--> LibreOffice headless -> PDF -> pdftotext
            |       - .txt/.md   --> Direct disk stream
            |
            +---> Chunking & Token Window Management:
            |       - chunkText() splits on paragraph/sentence boundaries
            |
            +---> AI Inference Engine (libcurl + Anthropic API / Claude Sonnet):
            |       - Prompt engineering with structured exam rules
            |       - Independent chunk analysis
            |       - Multi-chunk coherence & de-duplication pass
            |
            +---> Thread-Safe LRU Session Cache
            |
            v
[ Results & Export System ]
            |---> Rendered Markdown in browser (marked.js)
            |---> GET /download?format=pdf   --> Pandoc + pdflatex
            |---> GET /download?format=docx  --> Pandoc
            |---> GET /download?format=md    --> Raw Markdown
```

---

## 📁 Repository Structure

```
promptwar/
├── CMakeLists.txt              # CMake build configuration (C++17)
├── README.md                   # System documentation & deployment guide
├── PROJECT_LOG.md              # Cross-agent project log & decision history
├── include/
│   ├── httplib.h               # cpp-httplib HTTP server
│   └── nlohmann/
│       └── json.hpp            # nlohmann Modern C++ JSON library
├── src/
│   ├── main.cpp                # HTTP server routes, caching & export endpoints
│   ├── pdf_extract.h / .cpp    # Subprocess text extraction & LibreOffice converter
│   ├── llm_client.h / .cpp     # Anthropic Claude API client via libcurl
│   ├── notes_builder.h / .cpp  # Smart chunking, prompt templating & multi-pass merge
│   └── cli_test.cpp            # Standalone terminal verification binary
├── prompts/
│   └── notes_prompt.txt        # Core rule-based tutor prompt template
├── web/
│   └── index.html              # Sleek dark-mode drag-and-drop web interface
└── samples/                    # Ready-to-test lecture files
    ├── lecture_os.txt
    ├── lecture_os.docx
    └── lecture_os.pdf
```

---

## 🚀 Quick Start (Local Setup)

### 1. Prerequisites

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y g++ cmake poppler-utils libcurl4-openssl-dev pandoc libreoffice texlive-latex-base
```

### 2. Configure Environment

Set your Anthropic API key:
```bash
export ANTHROPIC_API_KEY="sk-ant-..."
# Optional: Set a specific model (defaults to claude-3-5-sonnet-20241022)
export ANTHROPIC_MODEL="claude-3-5-sonnet-20241022"
```

> **Tip for Offline Demo / Testing:**
> If you don't have an active API key or want to demo without network calls, set:
> ```bash
> export MOCK_LLM=1
> ```

### 3. Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

This compiles two binaries:
- `build/server`: The full HTTP web backend
- `build/cli_test`: Standalone terminal verification tool

### 4. Run Standalone CLI Test

Verify extraction and note generation directly in your terminal:
```bash
./build/cli_test samples/lecture_os.pdf "Computer Science" "Advanced"
```

### 5. Run Web Server

```bash
# Runs on port 8080 by default
./build/server

# Or customize port:
PORT=8080 ./build/server
```

Open your browser to: **[http://localhost:8080](http://localhost:8080)**

---

## ✨ Features & Bonuses Implemented

### 1. Core Rule-Based AI Flow
- Loads prompt template from `prompts/notes_prompt.txt` at runtime.
- Generates structured Markdown: `## Topics`, bulleted explanations, **bolded key terms**, a dedicated `## Key Definitions` list, and `## 3 Likely Exam Questions`.

### 2. Multi-Chunk Coherence Merge
- Lectures exceeding token thresholds are split on natural paragraph/sentence boundaries (`notes_builder.cpp`).
- Each chunk is processed through the LLM, followed by an automated **second-pass coherence & de-duplication merge** to produce a unified document.

### 3. Bonus: Multi-Format Input (PDF, DOCX, PPTX, TXT)
- Supports `.pdf`, `.docx`, `.doc`, `.pptx`, `.ppt`, `.odt`, `.rtf`, `.txt`, and `.md`.
- Office presentations and documents are normalized to PDF via headless LibreOffice (`soffice --headless --convert-to pdf`), enabling universal lecture ingestion with zero format fragmentation.

### 4. Bonus: 1-Click Export to PDF, Word & Markdown
- **PDF Export**: Compiled with Pandoc and LaTeX engine (`pdflatex`) with 1-inch margins and publication typography.
- **Word Export**: Compiles directly to native `.docx`.
- **Markdown Export**: Direct download for Obsidian, Notion, or text editors.
- In-memory thread-safe LRU cache allows instantaneous downloads right from the web UI.

### 5. Bonus: Academic Personalization
- Supports user-selected Subject domains (Computer Science, Mathematics, Biology, Economics, Physics, Chemistry, Medicine, etc.).
- Supports Academic Levels (Introductory, Undergraduate, Advanced Graduate, Cram Exam Prep) injected into the prompt.

---

## 🌐 Production Deployment (AWS EC2)

### Target Host: `15.252.157.8`

### 1. AWS Security Group
Ensure inbound HTTP traffic is enabled:
- **Type:** HTTP
- **Port:** `80`
- **Source:** `0.0.0.0/0` (and `::/0` if IPv6 is enabled)
- **SSH (Port 22):** Restricted to your IP

### 2. Bind Port 80 Without Root
Grant the server binary capability to bind privileged port 80 without running the process as root:
```bash
sudo setcap 'cap_net_bind_service=+ep' ./build/server
```

### 3. Systemd Service Setup
Create `/etc/systemd/system/studentworkspace.service`:
```ini
[Unit]
Description=AI-Powered Student Workspace (C++17)
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/home/ubuntu/student-workspace
Environment=PORT=80
Environment=ANTHROPIC_API_KEY=your-anthropic-api-key-here
ExecStart=/home/ubuntu/student-workspace/build/server
Restart=always
RestartSec=5
AmbientCapabilities=CAP_NET_BIND_SERVICE
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

Enable and start the service:
```bash
sudo systemctl daemon-reload
sudo systemctl enable studentworkspace
sudo systemctl start studentworkspace
sudo systemctl status studentworkspace
```

Your service will be live globally at: **`http://15.252.157.8`**

---

## 🔌 API Reference

| Endpoint | Method | Params / Body | Description |
|---|---|---|---|
| `/` | `GET` | — | Serves the interactive web interface |
| `/health` | `GET` | — | Returns `{"status":"healthy"}` health check |
| `/generate` | `POST` | Multipart `lecture` (file), `subject`, `level` | Ingests document, runs extraction & LLM pipeline, returns JSON `{id, notes, subject, level}` |
| `/download` | `GET` | `?id=<id>&format=pdf\|docx\|md&title=<title>` | Downloads the generated notes formatted as PDF, DOCX, or MD |
| `/export` | `POST` | JSON `{notes, format, title}` | Directly converts provided Markdown into PDF, DOCX, or MD |
