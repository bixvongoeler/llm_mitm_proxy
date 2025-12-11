# LLM MITM Proxy
*HTTPS proxy that injects an AI chat widget into the Tufts SIS web page*

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
