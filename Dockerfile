# =============================================================================
# LLM MITM Proxy - Containerized HTTPS Interception with LLM Chat
# =============================================================================
#
# A man-in-the-middle HTTPS proxy that injects an AI chat widget into web pages.
# Built for the Tufts CS 112 course project.
#
# SERVICES:
#   - C HTTPS Proxy      (port 9999) - Configure browser to use this as HTTP proxy
#   - Python Chat API    (port 5001) - Handles LLM requests from injected widget
#   - Injection Server   (internal)  - Injects chat widget into HTML responses
#
# QUICK START:
#   docker run -d -p 9999:9999 -p 5001:5001 \
#     -e LLMPROXY_API_KEY=your-key \
#     -e LLMPROXY_ENDPOINT=your-endpoint \
#     llm-mitmproxy
#
# ENVIRONMENT VARIABLES:
#   LLMPROXY_API_KEY    - (Required) API key for LLMProxy service
#   LLMPROXY_ENDPOINT   - (Required) LLMProxy API endpoint URL
#   PROXY_PORT          - (Optional) Proxy port, default: 9999
#
# BROWSER SETUP:
#   1. Configure browser HTTP proxy to localhost:9999
#   2. Install crt/proxy_ca.crt as trusted CA certificate
#
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1: Build C proxy binary
# -----------------------------------------------------------------------------
FROM alpine:3.21.3 AS c-build

RUN apk update && apk add --no-cache \
    build-base \
    git \
    linux-headers \
    libev-dev \
    libnsl-dev \
    openssl-dev

WORKDIR /llm_mitmproxy
COPY src/ ./src/
COPY lib/ ./lib/
COPY crt/ ./crt/
COPY Makefile ./Makefile

RUN make proxy

# -----------------------------------------------------------------------------
# Stage 2: Build Python environment
# -----------------------------------------------------------------------------
FROM python:3.13-alpine AS python-build

RUN apk add --no-cache \
    build-base \
    libffi-dev \
    openssl-dev \
    musl-dev \
    linux-headers \
    uv --repository=https://dl-cdn.alpinelinux.org/alpine/v3.21/community

WORKDIR /llm_server
COPY llm_server/pyproject.toml ./pyproject.toml
COPY llm_server/src/ ./src/
COPY llm_server/llmproxy/ ./llmproxy/
COPY llm_server/context/ ./context/
COPY llm_server/main.py ./main.py

RUN uv sync --no-dev

# -----------------------------------------------------------------------------
# Stage 3: Runtime image
# -----------------------------------------------------------------------------
FROM python:3.13-alpine AS runtime

LABEL org.opencontainers.image.title="LLM MITM Proxy"
LABEL org.opencontainers.image.description="HTTPS proxy with AI chat widget injection"
LABEL org.opencontainers.image.source="https://github.com/your-username/llm-mitmproxy"

# Runtime dependencies
RUN apk add --no-cache libev libnsl openssl curl bash tini

# Security: non-root user
RUN addgroup -S mitmp && adduser -S mitmp -G mitmp

WORKDIR /app

# Copy artifacts from build stages
COPY --from=c-build /llm_mitmproxy/proxy ./proxy
COPY --from=c-build /llm_mitmproxy/crt/ ./crt/
COPY --from=python-build /llm_server/.venv /app/llm_server/.venv
COPY --from=python-build /llm_server/src/ ./llm_server/src/
COPY --from=python-build /llm_server/llmproxy/ ./llm_server/llmproxy/
COPY --from=python-build /llm_server/context/ ./llm_server/context/
COPY --from=python-build /llm_server/main.py ./llm_server/main.py
COPY --from=python-build /llm_server/pyproject.toml ./llm_server/pyproject.toml
COPY docker-entrypoint.sh ./docker-entrypoint.sh

# Python environment
ENV PATH="/app/llm_server/.venv/bin:$PATH"
ENV PYTHONPATH="/app/llm_server/src:/app/llm_server/llmproxy/src"
ENV PYTHONUNBUFFERED=1

# Setup permissions
RUN mkdir -p /tmp && chmod 1777 /tmp && \
    chmod +x ./docker-entrypoint.sh && \
    chown -R mitmp:mitmp /app

# Expose ports
EXPOSE 9999 5001

# Use tini for proper signal handling
ENTRYPOINT ["/sbin/tini", "--"]

# Run as non-root
USER mitmp

CMD ["./docker-entrypoint.sh"]
