# AI-Powered Student Workspace (C++17 + Groq AI + Scikit-Learn)

> **Hackathon Submission — AI-Powered Student Revision Workspace**  
> Turn messy lecture PDFs, Word documents, or presentation slides into publication-quality, exam-ready revision notes in under 3 seconds. Built with a genuine high-performance **C++17** backend, **Groq ultra-fast AI inference**, **Python Scikit-Learn ML document intelligence**, and an Apple-inspired Liquid Glass web interface.

---

## 🌐 Live Hackathon Demo

The application is deployed and live for testing! You can use it right in your web browser:

- **Primary URL**: **Revoked after the Event**
- **Direct IP URL**: **Revoked after the Event** (or **Revoked after the Event**)



---

## 💡 What Is This Project? (The 30-Second Pitch)

College lectures and university courses bombard students with dense 50-slide PowerPoint presentations, 40-page textbook PDFs, and messy lecture notes. Reviewing for exams takes hours of manual skimming.

**This workspace solves that with a single seamless flow:**
1. A student drags and drops any lecture material (PDF, Word doc, PowerPoint slides, or text).
2. The system stores the document on the server and extracts key terms using **Scikit-Learn ML** (TF-IDF & TextRank cosine similarity).
3. **Groq's lightning-fast AI** synthesizes the document into structured, high-yield revision notes (Core Concepts, In-Depth Explanations, Key Definitions, and 3 Likely Exam Questions).
4. The student can read the notes immediately or export them with **1 click to PDF, Word (DOCX), or Markdown**.

---

## 🎯 Target Hackathon Flow & Completed Bonuses

| Flow / Requirement | Status | Implementation Details |
|---|---|---|
| **Core Flow: Lecture PDF → Revision Notes** | **100% Complete** | End-to-end ingestion, text normalization, prompt synthesis, and exam formatting. |
| **Bonus 1: Multi-Format Ingestion** | **100% Complete** | Ingests `.pdf`, `.docx`, `.pptx`, `.ppt`, and `.txt` seamlessly. |
| **Bonus 2: 1-Click Multi-Format Export** | **100% Complete** | Download notes as formatted **PDF**, native **Word (.docx)**, or **Markdown (.md)**. |
| **Bonus 3: Academic Personalization** | **100% Complete** | User chooses Course Discipline & Academic Rigor Level (Introductory to Ph.D.). |
| **Persistent Server Storage** | **100% Complete** | Every uploaded file is persistently archived on the server with direct download URLs (`GET /file?id=...`). |
| **Machine Learning Intelligence** | **100% Complete** | Python `scikit-learn` extracts TF-IDF concepts, lexical diversity %, and extractive summaries. |

---

## 🧠 How It Works (For Beginners & Judges)

Here is the step-by-step lifecycle of what happens when you press **"Synthesize Revision Notes"**:

```
[ User Browser ]
       |
       | 1. Uploads lecture file (PDF/DOCX/PPTX) + Subject + Academic Level
       v
[ C++17 Backend Server (cpp-httplib) ]
       |
       +---> 2. Secure Persistent Storage:
       |        - Saves file into /uploads with unique timestamped ID
       |        - Creates metadata JSON (file size, timestamp, subject)
       |        - Exposes instant download at GET /file?id=<id>
       |
       +---> 3. Multi-Format Text Extraction:
       |        - PDF files: Extracted with `pdftotext -layout` (Poppler)
       |        - Word (.docx) & Slides (.pptx): Converted to PDF via headless LibreOffice
       |        - Plain text (.txt/.md): Streamed directly from disk
       |
       +---> 4. Python ML Document Intelligence (Scikit-Learn Bridge):
       |        - Runs `scripts/ml_processor.py` via C++ IPC pipe
       |        - TF-IDF Vectorizer extracts top key terminology (with 0-100% relevance score)
       |        - Pairwise Cosine Similarity identifies central sentences (TextRank extractive summary)
       |        - Computes total words, sentence count, lexical diversity, and reading time
       |
       +---> 5. Groq Ultra-Fast AI Synthesis (Llama / Qwen / GPT-OSS):
       |        - High-speed inference via Groq API (`openai/gpt-oss-120b` or `qwen3.8`)
       |        - Response returns in ~2 to 3 seconds
       |        - Generates 4 structured sections:
       |          1. Core Principles & Foundational Architecture
       |          2. In-Depth Technical Concepts & Mechanisms
       |          3. Key Definitions
       |          4. 3 Likely Exam Questions (with grading criteria)
       |
       v
[ Results Rendered in Browser ]
       |---> Interactive Liquid Glass viewer with marked.js Markdown formatting
       |---> Stored File Banner with direct source download button
       |---> Python ML Document Intelligence stats grid and TF-IDF concept pills
       |---> 1-Click Export buttons:
              • PDF (Compiled via Pandoc & LaTeX)
              • Word DOCX (Compiled via Pandoc)
              • Markdown (Direct file download)
              • Copy to Clipboard
```

---

## 🌍 Where & How It Is Deployed

### Infrastructure Breakdown

- **Cloud Provider:** Amazon Web Services (AWS)
- **Service:** AWS EC2 Virtual Machine (Elastic Compute Cloud)
- **Public IP Address:** `15.252.157.8`
- **Domain Name:** `http://adityaman.website` (DNS A Record points directly to `15.252.157.8`)
- **Port:** Port `80` (Standard HTTP, with port `8080` fallback)
- **Operating System:** Fedora / Amazon Linux (`x86_64`)
- **Backend Architecture:** Native C++17 binary compiled with GCC, linked with `libcurl` and `pthread`.
- **Process Management:** Runs continuously in the background using `nohup` / `systemd`, surviving terminal disconnections and reboots.
- **AI Inference:** Connected via HTTPS to Groq's low-latency inference endpoints using API authentication.

---

## 📁 Repository Structure

```
promptwar/
├── CMakeLists.txt              # CMake build specification (C++17)
├── requirements.txt            # Python ML dependencies (scikit-learn, numpy)
├── README.md                   # System documentation & hackathon guide
├── PROJECT_LOG.md              # Historical engineering decisions & session log
├── .env.example                # Example configuration template
├── include/
│   ├── httplib.h               # cpp-httplib single-header HTTP server
│   └── nlohmann/
│       └── json.hpp            # nlohmann Modern C++ JSON library
├── src/
│   ├── main.cpp                # HTTP routes, storage endpoints & export logic
│   ├── pdf_extract.h / .cpp    # Subprocess text extraction & LibreOffice converter
│   ├── llm_client.h / .cpp     # Multi-provider LLM client (Groq + Anthropic + Mock)
│   ├── ml_bridge.h / .cpp      # C++ IPC bridge executing Python ML processor
│   ├── notes_builder.h / .cpp  # Parallel chunking, prompt assembly & multi-pass merge
│   ├── env_loader.h            # Native C++ .env configuration loader
│   └── cli_test.cpp            # Standalone terminal verification tool
├── scripts/
│   └── ml_processor.py         # Python Scikit-Learn TF-IDF & TextRank summarizer
├── prompts/
│   └── notes_prompt.txt        # Academic tutor prompt template
├── web/
│   ├── index.html              # Apple-inspired Liquid Glass UI (zero emojis, stroke icons)
│   └── sample_lecture_os.pdf   # Pre-loaded sample lecture for instant 1-click testing
├── samples/                    # Test files for terminal verification
│   ├── lecture_os.pdf          # Sample Operating Systems PDF
│   ├── lecture_os.docx         # Sample Operating Systems Word document
│   └── lecture_os.txt          # Sample plain text lecture
└── uploads/                    # Server-side persistent storage for uploaded documents
```

---

## 🚀 How to Run Locally or on a New Machine

If you are a judge or developer wanting to run this project on your own machine:

### 1. Install Prerequisites

**On Fedora / Amazon Linux / RHEL:**
```bash
sudo dnf install -y gcc-c++ cmake libcurl-devel poppler-utils python3 python3-pip pandoc libreoffice
pip3 install scikit-learn numpy
```

**On Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev poppler-utils python3 python3-pip pandoc libreoffice
pip3 install scikit-learn numpy
```

### 2. Configure Your API Key

Create a `.env` file in the root directory:
```bash
cp .env.example .env
```
Edit `.env` and paste your Groq API key:
```env
PORT=8080
GROQ_API_KEY=your_groq_api_key_here
GROQ_MODEL=openai/gpt-oss-120b
MOCK_LLM=0
```
*(If you do not have an API key, set `MOCK_LLM=1` to run in local document-aware testing mode without internet access).*

### 3. Build the Project

```bash
cmake -B build
cmake --build build -j$(nproc)
```

### 4. Test in Your Terminal (Standalone CLI)

You can verify the entire pipeline without even opening a browser:
```bash
./build/cli_test samples/lecture_os.pdf "Operating Systems" "Undergraduate"
```

### 5. Start the Web Server

```bash
./build/server
```
Open **[http://localhost:8080](http://localhost:8080)** in your browser!

---

## ⚡ How to Deploy on EC2 to Run 24/7

To run the server continuously on AWS EC2 so it stays active even after closing SSH:

```bash
# 1. Allow port 80 binding without running as root
sudo setcap 'cap_net_bind_service=+ep' ./build/server

# 2. Run in the background with nohup
nohup ./build/server > server.log 2>&1 &

# 3. Check health
curl http://127.0.0.1/health
```

---

## 🔌 API Reference (For Developers)

| Endpoint | Method | Parameters | Description |
|---|---|---|---|
| `/` | `GET` | — | Serves the Liquid Glass web interface |
| `/health` | `GET` | — | Returns JSON health status, active provider, and model |
| `/config` | `GET` | — | Returns frontend configuration & AI connection telemetry |
| `/generate` | `POST` | Multipart Form: `lecture` (file), `subject`, `level` | Main pipeline: stores file, runs Python ML, queries Groq, returns notes + insights |
| `/file` | `GET` | `?id=<id>` | Downloads the original uploaded file stored on the server |
| `/stored-files`| `GET` | — | Returns JSON array of all stored files on the server |
| `/download` | `GET` | `?id=<id>&format=pdf\|docx\|md&title=<title>` | Downloads generated revision notes in PDF, Word, or Markdown format |

---

## 🏆 Hackathon Highlights

1. **Genuinely Built in C++17**: Not a generic Python wrapper or Node.js app — the core server, multithreaded task pool, file handling, and export pipelines are engineered in high-performance C++17.
2. **Groq Sub-3s Generation**: Lightning-fast inference ensures zero frustrating wait times for students.
3. **Machine Learning Hybrid**: Combines classical ML (`scikit-learn` TF-IDF & TextRank graph centrality) with generative AI (`openai/gpt-oss-120b`).
4. **Zero-Emoji Professional UI**: Designed with Apple Liquid Glass aesthetic, specular lighting, subtle gradients, and typographic hierarchy.
5. **Real-World Ready**: Persistent server storage, multi-format conversion, and multi-format exports solve genuine academic friction points.
