#!/bin/bash
# =============================================================================
# Docker Entrypoint - Starts all LLM MITM Proxy services
# =============================================================================
# Services started:
#   1. Python Injection Server (Unix socket at /tmp/llm_proxy.sock)
#   2. Python Chat Server (HTTP on port 5001)
#   3. C Proxy (HTTP proxy on port 9999)
# =============================================================================

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configurable ports with defaults
PROXY_PORT="${PROXY_PORT:-9999}"

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║          LLM MITM Proxy - Starting Services                  ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"

# Check for required environment variables
if [ -z "$LLMPROXY_API_KEY" ]; then
    echo -e "${YELLOW}Warning: LLMPROXY_API_KEY not set. Chat functionality will not work.${NC}"
fi

if [ -z "$LLMPROXY_ENDPOINT" ]; then
    echo -e "${YELLOW}Warning: LLMPROXY_ENDPOINT not set. Chat functionality will not work.${NC}"
fi

# Cleanup function for graceful shutdown
cleanup() {
    echo -e "\n${CYAN}Shutting down services...${NC}"

    # Kill background processes
    if [ -n "$INJECTION_PID" ] && kill -0 "$INJECTION_PID" 2>/dev/null; then
        kill "$INJECTION_PID" 2>/dev/null || true
    fi

    if [ -n "$CHAT_PID" ] && kill -0 "$CHAT_PID" 2>/dev/null; then
        kill "$CHAT_PID" 2>/dev/null || true
    fi

    # Clean up socket file
    rm -f /tmp/llm_proxy.sock

    echo -e "${GREEN}Cleanup complete${NC}"
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM EXIT

# Change to llm_server directory for Python imports
cd /app/llm_server

# Start Injection Server (Unix socket)
echo -e "${CYAN}Starting Injection Server...${NC}"
python -c "from sis_advisor.injection_server import run_server; run_server()" &
INJECTION_PID=$!
sleep 1

if kill -0 "$INJECTION_PID" 2>/dev/null; then
    echo -e "${GREEN}✔ Injection Server started (PID: $INJECTION_PID)${NC}"
else
    echo -e "${RED}✘ Failed to start Injection Server${NC}"
    exit 1
fi

# Start Chat Server (HTTP on port 5001)
echo -e "${CYAN}Starting Chat Server on port 5001...${NC}"
python -c "from sis_advisor.chat_server import run_server; run_server()" &
CHAT_PID=$!
sleep 1

if kill -0 "$CHAT_PID" 2>/dev/null; then
    echo -e "${GREEN}✔ Chat Server started (PID: $CHAT_PID)${NC}"
else
    echo -e "${RED}✘ Failed to start Chat Server${NC}"
    exit 1
fi

# Change back to app directory for proxy
cd /app

# Print usage instructions
echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                   All Services Running                       ║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
printf "${CYAN}║${NC} Proxy:     http://localhost:%-5s (configure browser here) ${CYAN}║${NC}\n" "$PROXY_PORT"
echo -e "${CYAN}║${NC} Chat API:  http://localhost:5001                           ${CYAN}║${NC}"
echo -e "${CYAN}║${NC} Socket:    /tmp/llm_proxy.sock                             ${CYAN}║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC} ${YELLOW}Don't forget to install crt/proxy_ca.crt in your browser!${NC}  ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Start C Proxy (foreground - this keeps the container running)
echo -e "${CYAN}Starting C Proxy on port ${PROXY_PORT}...${NC}"
exec ./proxy "$PROXY_PORT" crt/proxy_ca.crt crt/proxy_ca.key
