# =============================================================================
# LLM MITM Proxy - Complete Containerized Application (Alpine-based)
# =============================================================================
# This Dockerfile builds a single container with:
#   - C HTTPS MITM Proxy (port 9999)
#   - Python Injection Server (Unix socket)
#   - Python Chat Server (port 5001)
#
# Build: docker build -t llm-mitmproxy .
# Run:   docker run -p 9999:9999 -p 5001:5001 --env-file .env llm-mitmproxy
# =============================================================================

# =============================================================================
# STAGE 1: C Build Stage - compiles the C proxy binary
# =============================================================================
FROM alpine:3.21.3 AS c-build

# Install build dependencies for C proxy
RUN apk update && apk add --no-cache \
    build-base \
    git \
    linux-headers \
    libev-dev \
    libnsl-dev \
    openssl-dev

WORKDIR /llm_mitmproxy

# Copy C proxy source files
COPY src/ ./src/
COPY lib/ ./lib/
COPY crt/ ./crt/
COPY Makefile ./Makefile

# Build the proxy binary
RUN make proxy

# =============================================================================
# STAGE 2: Python Build Stage - installs Python dependencies
# =============================================================================
FROM python:3.13-alpine AS python-build

# Install build dependencies for Python packages
# Using Alpine's packaged uv (community repo) instead of pip install
RUN apk add --no-cache \
    build-base \
    libffi-dev \
    openssl-dev \
    musl-dev \
    linux-headers \
    uv --repository=https://dl-cdn.alpinelinux.org/alpine/v3.21/community

WORKDIR /llm_server

# Copy Python project files (including workspace member llmproxy)
COPY llm_server/pyproject.toml ./pyproject.toml
COPY llm_server/src/ ./src/
COPY llm_server/llmproxy/ ./llmproxy/
COPY llm_server/context/ ./context/
COPY llm_server/main.py ./main.py

# Create virtual environment and install dependencies using uv
RUN uv sync --no-dev

# =============================================================================
# STAGE 3: Runtime Stage - minimal image with both services
# =============================================================================
FROM python:3.13-alpine AS runtime

# Install runtime dependencies
RUN apk add --no-cache \
    libev \
    libnsl \
    openssl \
    curl \
    bash \
    tini

# Create non-root user for security
RUN addgroup -S mitmp && adduser -S mitmp -G mitmp

WORKDIR /app

# Copy C proxy binary and certificates from c-build stage
COPY --from=c-build /llm_mitmproxy/proxy ./proxy
COPY --from=c-build /llm_mitmproxy/crt/ ./crt/

# Copy Python virtual environment from python-build stage
COPY --from=python-build /llm_server/.venv /app/llm_server/.venv

# Copy Python application code
COPY --from=python-build /llm_server/src/ ./llm_server/src/
COPY --from=python-build /llm_server/llmproxy/ ./llm_server/llmproxy/
COPY --from=python-build /llm_server/context/ ./llm_server/context/
COPY --from=python-build /llm_server/main.py ./llm_server/main.py
COPY --from=python-build /llm_server/pyproject.toml ./llm_server/pyproject.toml

# Copy entrypoint script into /app
COPY docker-entrypoint.sh ./docker-entrypoint.sh

# Set Python environment to use uv's virtual environment
ENV PATH="/app/llm_server/.venv/bin:$PATH"
ENV PYTHONPATH="/app/llm_server/src:/app/llm_server/llmproxy/src"
ENV PYTHONUNBUFFERED=1

# Create directory for Unix socket with proper permissions
RUN mkdir -p /tmp && chmod 1777 /tmp

# Change ownership and set permissions
RUN chmod +x ./docker-entrypoint.sh && \
    chown -R mitmp:mitmp /app

# Expose ports
# 9999 - HTTP Proxy (configure browser to use this)
# 5001 - Chat API (widget makes XHR requests here)
EXPOSE 9999 5001

# Use tini as init system to handle signals properly
ENTRYPOINT ["/sbin/tini", "--"]

# Run as non-root user
USER mitmp

# Start all services
CMD ["./docker-entrypoint.sh"]
