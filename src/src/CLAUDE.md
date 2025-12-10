# C Proxy - HTTPS MITM Implementation

## Source Files

| File           | Purpose                                                                  |
| -------------- | ------------------------------------------------------------------------ |
| `main.c`       | Entry point, libev loop initialization, CA cert loading                  |
| `accepted.c`   | Connection acceptance, HTTP method routing (GET vs CONNECT)              |
| `connecting.c` | HTTPS CONNECT handling, full TLS MITM handshake, cert spoofing           |
| `tunneling.c`  | Bidirectional SSL tunneling, header injection, **LLM integration hooks** |
| `http.c`       | HTTP GET request forwarding (non-CONNECT)                                |
| `connection.c` | Connection lifecycle management (alloc, cleanup, buffer I/O)             |
| `llm_client.c` | Unix socket client for Python server communication                       |
| `llm_client.h` | Protocol definitions and function signatures                             |
| `proxy.h`      | Core data structures                                                     |
| `utils.c/h`    | Socket helpers, host:port parsing                                        |

## Core Data Structures (proxy.h)

**proxy_context_t** - Global proxy state

- `loop`: libev event loop
- `listen_fd`: Server socket
- `ca_cert`, `ca_key`: CA for signing spoofed certs

**connection_t** - Per-connection state

- `client_fd`, `server_fd`: Socket file descriptors
- `client_ssl`, `server_ssl`: SSL connections for MITM
- `target_host`, `target_port`: Destination server
- `spoofed_key`: Generated certificate key

**tunnel_link_t** (tunneling.c) - Bidirectional tunnel state

- `llm_buffer`: Accumulated HTTP response for LLM processing
- `llm_buffer_enabled`: True if URL matches filter (sis.it.tufts.edu)
- `llm_headers_complete`, `llm_content_length`: Response parsing state

## MITM Handshake Flow (connecting.c)

1. Receive CONNECT request, send `200 Connection Established`
2. DNS lookup target server
3. Non-blocking connect to server
4. TLS handshake with server, extract cert CN/SANs
5. Generate spoofed certificate signed by CA
6. TLS handshake with client using spoofed cert
7. Begin bidirectional tunneling

## LLM Integration Points (tunneling.c + llm_client.c)

| Function                    | File         | Purpose                                                   |
| --------------------------- | ------------ | --------------------------------------------------------- |
| `llm_should_process()`      | llm_client.c | URL filtering - returns true for sis.it.tufts.edu         |
| `llm_buffer_append()`       | tunneling.c  | Accumulates response bytes, grows buffer (max 4MB)        |
| `llm_response_complete()`   | tunneling.c  | Detects when full HTTP response received (Content-Length) |
| `llm_process_and_forward()` | tunneling.c  | Orchestrates: send to Python, replace buffer if modified  |
| `llm_process_response()`    | llm_client.c | Unix socket call to Python server                         |

**Data flow:** Server response í buffer accumulation í completeness check í send to Python í forward (modified or original)

## Build & Run

```bash
make proxy                           # Build binary
make run                             # Build and run on port 9999
make rebuild-cc                      # Generate compile_commands.json
./proxy 9999 crt/proxy_ca.crt crt/proxy_ca.key  # Manual run
```

## Dependencies

- **libev** - Event loop (non-blocking I/O)
- **OpenSSL 3.0+** - TLS, certificate generation
- **picohttpparser** (lib/) - HTTP request parsing

## Key Integration Notes

- LLM buffering only for serveríclient direction
- Socket timeout: 2s connect, 30s read/write
- If Python server unavailable, original content forwarded unchanged
- Response completeness: parses `Content-Length` header, counts body bytes
- Header injection (`X-Proxy:CS112`) applied to all responses
