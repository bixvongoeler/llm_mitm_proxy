# LLM MITM Proxy

An HTTPS man-in-the-middle proxy that intercepts encrypted web traffic and injects an LLM-powered academic advisor widget into university course registration pages. The system consists of three main components:

1. An event based multiclient C proxy server using `libev` handling CONNECT tunneling and mitm TLS certificate generation (using a trusted CA).
2. A Python injection server communicating via Unix socket that parses page HTML and injects a JavaScript widget
3. An LLM backend using RAG-augmented GPT queries. Context managed using llm generated structed json summaries of student transcripts, scraped course catalog data, and academic policy info.

The widget provides context-aware academic advising and course recommendations using parsed page content, degree requirements, academic history, and detailed course information. Containerized with Docker for portable deployment.

Image available on [Docker Repo](https://hub.docker.com/r/bixvongoeler/llm_mitmproxy_containerized)

https://github.com/user-attachments/assets/bbad990e-d925-482d-bae7-1d844135818e

## Run

```bash
docker pull bixvongoeler/llm_mitmproxy_containerized
docker run -d -p 9999:9999 -p 5001:5001 \
    -e LLMPROXY_API_KEY=your-key \
    -e LLMPROXY_ENDPOINT=your-endpoint \
    bixvongoeler/llm_mitmproxy_containerized
```

## Browser Setup and Usage

1. Set HTTP/HTTPS proxy to `localhost:9999` in FireFox
2. Install `crt/proxy_ca.crt` as trusted CA (can download from https://github.com/bixvongoeler/llm_mitmproxy_containerized)
3. Navigate to SIS Course Search Page (https://sis.it.tufts.edu/psp/paprd/EMPLOYEE/EMPL/h/?tab=TFP_CLASS_SEARCH#class_search)
4. You may need to perform a force refresh (cmd+shift+r)
5. Click the chat widget in the bottom corner
6. Make a course search (LLM has info about CS courses only)
7. Upload a transcript (example anonymized transcripts can be found in the `example transcripts`) for personal context
8. Chat with the SIS Academic Advisor!

## Environment Variables

| Variable            | Required |
| ------------------- | -------- |
| `LLMPROXY_API_KEY`  | Yes      |
| `LLMPROXY_ENDPOINT` | Yes      |

## LLM Context Background

https://github.com/user-attachments/assets/dce6e496-3e60-4ba5-9bbc-1e4214be4ab0

## Repo Structure
```bash
llm_mitmproxy/
├── Makefile                     # Build configuration
├── src/                         # C proxy implementation
│   ├── main.c                   # Entry point, event loop initialization
│   ├── proxy.h                  # Core data structures (connection_t, proxy_context_t)
│   ├── connection.c             # Connection lifecycle management
│   ├── accepted.c               # Incoming client connection handling
│   ├── connecting.c             # HTTPS CONNECT handling with TLS MITM
│   ├── tunneling.c              # Bidirectional tunneling & LLM buffering
│   ├── http.c                   # HTTP GET request handling
│   ├── llm_client.c             # Unix socket client for Python server
│   ├── llm_client.h             # LLM client interface
│   └── utils.c/h                # Logging, socket utilities
│
├── llm_server/                  # Python backend services
│   ├── main.py                  # Entry point (injection/chat run commands)
│   ├── src/sis_advisor/
│   │   ├── advisor.py           # Core SIS advisor logic with LLM interactions
│   │   ├── injection_server.py  # Unix socket server for C proxy communication
│   │   ├── chat_server.py       # Flask HTTP API for browser/widget
│   │   ├── config.py            # Configuration management
│   │   └── widget/
│   │       ├── __init__.py
│   │       └── bundle.py        # HTML/CSS/JS widget generator
│   └── context/
│       ├── course_summaries/    # Summarized Course catalog data
│       └── cs_major_reqs.md     # CS program requirements
│
├── crt/                         # Certificate files
│   ├── proxy_ca.crt             # CA certificate (installed in browser)
│   └── proxy_ca.key             # CA private key (for MITM)
│
├── example_transcripts/         # Anonymized student transcripts for testing (AI Generated)
│   └── transcript_GRADE.pdf     # Individual transcripts for multiple students (based on real tufts transcripts)
│
├── Dockerfile                   # Define Docker Image
└── docker-entrypoint.sh         # Runs servers inside docker container
```
