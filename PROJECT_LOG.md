# PROJECT LOG — AI-Powered Student Workspace

> **If you are an AI assistant picking up this project** (Claude, ChatGPT, Copilot, Claude Code, or a fresh session of any of them): read this entire file before writing or changing anything. It exists so you don't repeat a decision that was already made, retry something that already failed, or overwrite work you don't know about.
>
> Before you finish your session, append a new dated entry under **Session Log**. Never delete or rewrite old entries — only add to them.

---

## 1. Project Snapshot
- **Goal:** Web app — user uploads a lecture PDF (or DOCX/PPTX), gets back AI-generated revision notes.
- **Core flow:** Lecture PDF → revision notes (single flow, not a multi-tool suite).
- **Stack:** C++17 backend (cpp-httplib, libcurl, nlohmann/json), plain HTML/JS frontend, Anthropic API (with offline fallback/mock support).
- **Deployment target:** AWS EC2, public IP `15.252.157.8`, served on port 80.
- **Full build spec:** see `README.md` and spec in this repo for architecture, code, and setup commands.

## 2. Locked-in Decisions (don't re-litigate these without a good reason)
| Decision | Reasoning |
|---|---|
| C++ backend, minimal HTML/JS frontend | Project should genuinely be C++; UI is just a thin shell to demo in a browser |
| `pdftotext` (poppler-utils) instead of Poppler's C++ API | Poppler's C++ bindings are fiddly to set up fast; shelling out is reliable under time pressure |
| `soffice --headless` to convert docx/pptx → PDF | Avoids writing separate parsers per file format |
| Anthropic API, model `claude-3-5-sonnet-20241022` (configurable via `ANTHROPIC_MODEL`) | Top quality/cost balance for academic note extraction |
| Unique per-request temp filenames (`upload_<timestamp>_<rand>`) | Public server = concurrent users; fixed paths corrupt results under simultaneous uploads |
| Custom `PipeCloser` struct for `std::unique_ptr<FILE, PipeCloser>` | Eliminates compiler attribute warnings cleanly in C++17 |
| In-memory thread-safe LRU cache for generated notes | Powers instantaneous 1-click downloads for PDF, DOCX, and Markdown formats |
| Direct Pandoc + LaTeX PDF engine with soffice fallback | Yields publication-quality formatted PDF and Word output |
| systemd service on EC2, not a manual `nohup`/screen session | Survives SSH disconnects and reboots |
| Port 80 via `setcap`, not running the whole server as root | Same result, smaller attack surface |

## 3. Current Status
_Last updated: 2026-09-13 12:15 UTC by Antigravity AI_

- [x] `pdf_extract.cpp` working (PDF → raw text via `pdftotext -layout`)
- [x] `llm_client.cpp` working (libcurl + Anthropic API with model configuration & offline demo fallback)
- [x] `notes_builder.cpp` chunking + merge pass working for long lectures
- [x] `httplib` server + `web/index.html` working locally on port 8080 (configurable via `PORT`)
- [x] Export bonus (pandoc → PDF, DOCX, and Markdown with `/download` and `/export` routes)
- [x] Multi-format bonus (DOCX, PPTX, PPT, TXT, MD normalized via headless LibreOffice conversion)
- [x] Personalization bonus (Subject & Academic Level dropdowns injected directly into prompt template)
- [x] Standalone CLI test utility (`build/cli_test`) for fast offline or terminal verification
- [ ] Deployed on EC2 instance `15.252.157.8`, security group open on port 80
- [ ] Verified reachable from an outside machine at `http://15.252.157.8`
- [ ] Elastic IP allocated (so the IP doesn't change on instance restart)

## 4. Known Issues / Dead Ends
_(so nobody burns an hour retrying something already ruled out)_

- **Direct system-wide apt without sudo:** Server runs in unprivileged container where `sudo` asks for password. Statically linked `pandoc` binary downloaded to `~/.local/bin/pandoc`, and `nlohmann/json.hpp` header placed in `include/nlohmann/json.hpp`. Works with zero external system package issues.
- **`decltype(&pclose)` compiler warning:** GCC 13 throws `-Wignored-attributes` when using function pointer in `std::unique_ptr`. Resolved by defining `struct PipeCloser { void operator()(FILE* f) const { if (f) pclose(f); } };`.
- **Multipart form text fields in cpp-httplib:** In `multipart/form-data`, non-file form fields like `subject` and `level` are placed in `req.files` by `httplib`. Handled by checking `req.has_file()` and `req.has_param()` for total interoperability.

## 5. Next Steps (priority order)
1. Verify EC2 SSH access and deploy binary to EC2 (`15.252.157.8`).
2. Set up `cap_net_bind_service` for port 80 execution and systemd service `studentworkspace.service`.
3. Provide live demo verification and pre-recorded/cached outputs.

## 6. Environment / Secrets
- `ANTHROPIC_API_KEY` — set via systemd `Environment=` line on EC2, never committed to git
- `ANTHROPIC_MODEL` — optional (defaults to `claude-3-5-sonnet-20241022` or `claude-sonnet-5`)
- `PORT` — default `8080` for local dev; `80` on production EC2
- `MOCK_LLM` — set `MOCK_LLM=1` to run offline without API calls during demo / rehearsal
- EC2 public IP: `15.252.157.8`
- Repo location on EC2: `/home/ubuntu/student-workspace`

## 7. Session Log
_(append only — newest entry at the top)_

### 2026-09-13 14:45 UTC — Antigravity AI
- **Changed:**
  - Added persistent server storage for uploaded files in `uploads/` with unique identifier hashing, JSON metadata tracking (`_meta.json`), and dedicated retrieval endpoints:
    - `GET /file?id=<id>` (downloads original uploaded file with correct MIME type and original filename headers).
    - `GET /stored-files` (returns JSON array of all stored files and metadata).
  - Integrated Python Scikit-Learn Machine Learning Pipeline (`scripts/ml_processor.py` & `src/ml_bridge.cpp`):
    - TF-IDF N-gram key concept extraction with normalized relevance scoring (0-100%).
    - TextRank-inspired extractive summarization using sentence-level pairwise cosine similarity centrality.
    - Lexical statistics (word count, sentence count, lexical diversity %, study time estimate, academic complexity scoring).
    - Built-in graceful pure-Python fallback if scikit-learn is not present.
  - Upgraded Liquid Glass UI (`web/index.html`):
    - Added Stored File Banner with 1-click download of the stored source document.
    - Added Python ML Document Intelligence Panel with metrics grid, TF-IDF concept pills, and cosine centrality summary.
  - Added `requirements.txt` for Python dependencies (`scikit-learn`, `numpy`).
  - Standardized `req.files` lookup in `src/main.cpp` for Fedora/EC2 cpp-httplib build compatibility.
- **Why:** Satisfy user requirement to persistently store uploaded lecture files on the server, process them with a well-known Python ML library (scikit-learn TF-IDF + cosine centrality), pass structured JSON back to the C++ server, and display the intelligence in the UI.
- **Result:** 100% build pass, verified `/generate`, `/file`, `/stored-files`, `/download` endpoints, and standalone CLI test.

### 2026-09-13 13:22 UTC — Antigravity AI
- **Changed:**
  - Redesigned [web/index.html](file:///home/aditya-aman/promptwar/web/index.html) with an Apple-inspired Liquid Glass aesthetic (`backdrop-filter: blur(28px) saturate(180%)`, specular light edges, atmospheric radial mesh gradients).
  - Added a monumental hero section with metallic gradient typography and a 4-card Bento architectural spec strip (`01 / Concurrency`, `02 / Ingestion`, `03 / Synthesis`, `04 / Publishing`).
  - Added a sticky frosted glass navigation bar with brand monogram and live status beacon.
  - Added an extensive 4-column academic engineering footer (Core Subsystems, Ingestion Specs, Synthesis Models, Deployment & EC2 Infrastructure).
  - Removed all `translateY` hover-floating animations (replaced with smooth specular border lighting and color shifts).
  - Maintained 100% emoji-free design (clean stroke SVG vectors and typographic glyphs only).
- **Why:** Fulfill user request for hero section, footer, liquid glass trending design, and smooth non-floating animations with zero emojis.
- **Result:** Beautiful, production-grade interface ready on port 80/8080.

### 2026-09-13 12:36 UTC — Antigravity AI
- **Changed:**
  - Configured default port to `80` in [.env](file:///home/aditya-aman/promptwar/.env) and [src/main.cpp](file:///home/aditya-aman/promptwar/src/main.cpp), with automatic fallback to port `8080` if elevated permissions (`CAP_NET_BIND_SERVICE`) are not yet granted.
  - Implemented multi-core CPU concurrency:
    - HTTP server configured with `ThreadPool` sized to `hardware_concurrency * 2` (configurable via `SERVER_THREADS` in `.env`).
    - Lecture chunk processing in [src/notes_builder.cpp](file:///home/aditya-aman/promptwar/src/notes_builder.cpp) parallelized using `std::async(std::launch::async, ...)` across worker threads.
  - Redesigned frontend in [web/index.html](file:///home/aditya-aman/promptwar/web/index.html) into an authentic Swiss International Typographic Style:
    - Clean sans-serif hierarchy, strict asymmetric grid rhythm, high-contrast monochrome aesthetic with Swiss Red accents.
    - Zero emojis anywhere in UI, backend logs, or generated notes.
    - Human-like academic editorial microcopy and typography.
- **Why:** Satisfy user request for port 80 hosting, multi-threaded CPU concurrency, and professional Swiss design with no emojis.
- **Result:** Fully compiled, verified with parallel chunking, port 80 handling, and Swiss design UI.

### 2026-09-13 12:22 UTC — Antigravity AI
- **Changed:**
  - Added native C++ `.env` configuration loader ([src/env_loader.h](file:///home/aditya-aman/promptwar/src/env_loader.h)) parsing `PORT`, `HOST`, `ANTHROPIC_API_KEY`, `ANTHROPIC_MODEL`, `MOCK_LLM`, and upload limits.
  - Upgraded [src/llm_client.cpp](file:///home/aditya-aman/promptwar/src/llm_client.cpp) with an intelligent document-aware testing mode: automatically synthesizes structured notes, key definitions, and exam questions directly from uploaded material without requiring an API key.
  - Added `GET /config` endpoint and interactive frontend mode badge in [web/index.html](file:///home/aditya-aman/promptwar/web/index.html) to indicate Testing Mode vs Live Claude.
  - Added 1-click sample lecture loading in web UI.
  - Created [.env](file:///home/aditya-aman/promptwar/.env) and [.env.example](file:///home/aditya-aman/promptwar/.env.example).
- **Why:** Allow immediate zero-cost local testing without an API key while keeping all critical parameters centrally managed via `.env`.
- **Result:** Successfully builds, loads `.env` on boot, runs testing mode when key is absent, and seamlessly connects to Claude when key is populated.

### 2026-09-13 12:15 UTC — Antigravity AI
- **Changed:**
  - Implemented full C++17 student workspace backend (`pdf_extract`, `llm_client`, `notes_builder`, `main`).
  - Added standalone `cli_test` tool to test the entire pipeline end-to-end on any document.
  - Added multi-format conversion bonus (DOCX, PPTX via `soffice --headless`).
  - Added multi-format export bonus (PDF via LaTeX pandoc, DOCX via pandoc, Markdown).
  - Built modern interactive responsive UI in `web/index.html` with drag-and-drop, marked.js rendering, and 1-click exports.
  - Created sample lecture assets in `samples/` for immediate testing.
- **Why:** Complete implementation of build spec and bonus deliverables for hackathon submission.
- **Result:** Successfully compiled with 0 warnings, verified end-to-end with sample lecture documents.
