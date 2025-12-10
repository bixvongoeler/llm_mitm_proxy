#!/bin/bash
# Docker Entrypoint - Starts all LLM MITM Proxy services

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

PROXY_PORT="${PROXY_PORT:-9999}"

echo -e "${CYAN}LLM MITM Proxy - Starting Services${NC}"

if [ -z "$LLMPROXY_API_KEY" ]; then
    echo -e "${YELLOW}Warning: LLMPROXY_API_KEY not set${NC}"
fi

if [ -z "$LLMPROXY_ENDPOINT" ]; then
    echo -e "${YELLOW}Warning: LLMPROXY_ENDPOINT not set${NC}"
fi

cleanup() {
    echo -e "\n${CYAN}Shutting down...${NC}"
    [ -n "$INJECTION_PID" ] && kill "$INJECTION_PID" 2>/dev/null || true
    [ -n "$CHAT_PID" ] && kill "$CHAT_PID" 2>/dev/null || true
    rm -f /tmp/llm_proxy.sock
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

cd /app/llm_server

# Start Injection Server
echo -e "${CYAN}Starting Injection Server...${NC}"
python -c "from sis_advisor.injection_server import run_server; run_server()" &
INJECTION_PID=$!
sleep 1

if kill -0 "$INJECTION_PID" 2>/dev/null; then
    echo -e "${GREEN}Injection Server started${NC}"
else
    echo -e "${RED}Failed to start Injection Server${NC}"
    exit 1
fi

# Start Chat Server
echo -e "${CYAN}Starting Chat Server...${NC}"
python -c "from sis_advisor.chat_server import run_server; run_server()" &
CHAT_PID=$!
sleep 1

if kill -0 "$CHAT_PID" 2>/dev/null; then
    echo -e "${GREEN}Chat Server started${NC}"
else
    echo -e "${RED}Failed to start Chat Server${NC}"
    exit 1
fi

cd /app

echo ""
echo -e "${GREEN}All services running:${NC}"
echo -e "  Proxy:    ${CYAN}http://localhost:${PROXY_PORT}${NC}"
echo -e "  Chat API: ${CYAN}http://localhost:5001${NC}"
echo ""
echo -e "${YELLOW}Install crt/proxy_ca.crt in your browser${NC}"
echo ""

# Start C Proxy (foreground)
echo -e "${CYAN}Starting C Proxy on port ${PROXY_PORT}...${NC}"
exec ./proxy "$PROXY_PORT" crt/proxy_ca.crt crt/proxy_ca.key
