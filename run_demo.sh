#!/bin/bash
#
# run_demo.sh - Start all SIS Advisor demo services
#
# This script starts:
#   1. C HTTPS proxy (port 9999)
#   2. Python injection server (Unix socket)
#   3. Python chat server (port 5001)
#
# Usage: ./run_demo.sh [--build]
#
# Options:
#   --build    Rebuild the C proxy before starting
#

set -e

# Colors
BLACK='\033[0;30m'
BOLDBLACK='\033[1;30m'
RED='\033[0;31m'
BOLDRED='\033[1;31m'
GREEN='\033[0;32m'
BOLDGREEN='\033[1;32m'
YELLOW='\033[1;33m'
BOLDYELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLDBLUE='\033[1;34m'
PURPLE='\033[0;35m'
BOLDPURPLE='\033[1;35m'
CYAN='\033[0;36m'
BOLDCYAN='\033[1;36m'
WHITE='\033[0;37m'
BOLDWHITE='\033[1;37m'
NC='\033[0m' # No Color

CHECKMARK=$BOLDGREEN"✔$NC"
FAILMARK=$BOLDRED"✘$NC"

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
LLM_SERVER_DIR="$PROJECT_ROOT/llm_server"

# PID file for cleanup
PID_FILE="$PROJECT_ROOT/.runtime_pids"

# Cleanup function
cleanup() {
    echo -e "\n${BOLDCYAN}Shutting down services...${NC}"

    if [ -f "$PID_FILE" ]; then
        while read -r pid; do
            if kill -0 "$pid" 2>/dev/null; then
                echo "Stopping process $pid"
                kill "$pid" 2>/dev/null || true
            fi
        done <"$PID_FILE"
        rm -f "$PID_FILE"
    fi

    # Clean up socket file
    rm -f /tmp/llm_proxy.sock

    echo -e "${GREEN}Cleanup complete${NC}"
    exit 0
}

# Set up trap for cleanup
trap cleanup SIGINT SIGTERM EXIT

# echo "║                  SisGPT - Tufts Academic Advising Assistant                  ║"
# Print header
print_header() {
    echo -e "${BOLDCYAN}"
    msg_color=$BOLDWHITE
    header_msg="NO MESSAGE PROVIDED"
    if [ -n "$1" ]; then
        header_msg="$1"
    fi
    if [ -n "$2" ]; then
        msg_color="$2"
    fi
    msg_len=${#header_msg}
    num_space=$((79 - msg_len))
    num_space=$((num_space / 2))
    echo "╔══════════════════════════════════════════════════════════════════════════════╗"
    echo -e -n "║$msg_color"
    # Print left whitespace, then message, then right whitespace
    printf "%*s%s%*s" $num_space "" "$header_msg" $num_space ""
    echo -e -n "${BOLDCYAN}"
    echo "║"
    echo "╚══════════════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Check dependencies
check_dependencies() {
    echo -e "${BOLDCYAN}Checking Dependencies...${NC}"

    # Check for required commands
    local missing=()

    if ! command -v uv &>/dev/null; then
        missing+=("uv (Python package manager)")
    fi

    if ! command -v make &>/dev/null; then
        missing+=("make (build tool)")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}Missing dependencies:${NC}"
        for dep in "${missing[@]}"; do
            echo "  - $dep"
        done
        exit 1
    fi

    echo -e "$CHECKMARK${GREEN} - All dependencies found${NC}"
}

# Build C proxy
build_proxy() {
    echo -e "\n${BOLDCYAN}Building C proxy...${NC}"
    cd "$PROJECT_ROOT"
    make clean proxy
    echo -e "$CHECKMARK${GREEN} - Build complete${NC}"
}

# Install Python dependencies
install_python_deps() {
    echo -e "\n${BOLDCYAN}Installing Python dependencies...${NC}"
    cd "$LLM_SERVER_DIR"
    uv sync
    echo -e "$CHECKMARK${GREEN} - Python dependencies installed${NC}"
}

# Check for .env file
check_env() {
    if [ ! -f "$LLM_SERVER_DIR/.env" ]; then
        echo -e "${RED}Warning: No .env file found in $LLM_SERVER_DIR${NC}"
        echo "The chat server requires LLMProxy credentials."
        echo "Create a .env file with:"
        echo "  LLMPROXY_ENDPOINT=<your-endpoint>"
        echo "  LLMPROXY_API_KEY=<your-api-key>"
        echo ""
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Start C proxy
start_proxy() {
    echo -e "\n${BOLDCYAN}Starting C proxy on port 9999...${NC}"
    cd "$PROJECT_ROOT"

    if [ ! -f "./proxy" ]; then
        echo -e "$FAILMARK${RED} - Proxy binary not found. Building...${NC}"
        build_proxy
    fi

    ./proxy 9999 crt/proxy_ca.crt crt/proxy_ca.key &
    local pid=$!
    echo "$pid" >>"$PID_FILE"

    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        echo -e "$CHECKMARK${GREEN} - C proxy started (PID: $pid)${NC}"
    else
        echo -e "$FAILMARK${RED} - Failed to start C proxy${NC}"
        exit 1
    fi
}

# Start injection server
start_injection_server() {
    echo -e "\n${BOLDCYAN}Starting injection server...${NC}"
    cd "$LLM_SERVER_DIR"

    uv run python -c "from sis_advisor.injection_server import run_server; run_server()" &
    local pid=$!
    echo "$pid" >>"$PID_FILE"

    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        echo -e "$CHECKMARK${GREEN} - Injection server started (PID: $pid)${NC}"
    else
        echo -e "$FAILMARK${RED} - Failed to start injection server${NC}"
        exit 1
    fi
}

# Start chat server
start_chat_server() {
    echo -e "\n${BOLDCYAN}Starting chat server on port 5001...${NC}"
    cd "$LLM_SERVER_DIR"

    uv run python -c "from sis_advisor.chat_server import run_server; run_server()" &
    local pid=$!
    echo "$pid" >>"$PID_FILE"

    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        echo -e "$CHECKMARK${GREEN} - Chat server started (PID: $pid)${NC}"
    else
        echo -e "$FAILMARK${RED} - Failed to start chat server${NC}"
        exit 1
    fi
}

# Print usage instructions
print_instructions() {
    print_header "All services started successfully!" $BOLDGREEN
    echo -e "${BOLDPURPLE}════════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLDCYAN}Services running:${NC}"
    echo "  - C Proxy:          http://localhost:9999"
    echo "  - Injection Server: /tmp/llm_proxy.sock"
    echo "  - Chat Server:      http://localhost:5001"
    echo ""
    echo -e "${BOLDCYAN}To use the demo:${NC}"
    echo "  1. Install the CA certificate in your browser:"
    echo "     $PROJECT_ROOT/crt/proxy_ca.crt"
    echo ""
    echo "  2. Configure your browser to use HTTP proxy:"
    echo "     Host: localhost"
    echo "     Port: 9999"
    echo ""
    echo "  3. Navigate to: https://sis.it.tufts.edu"
    echo -e "${BOLDPURPLE}════════════════════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${BOLDYELLOW}Press Ctrl+C to stop all services${NC}"
}

# Main
main() {
    print_header "SisGPT - Tufts Academic Advising Assistant"

    # Parse arguments
    if [ "$1" == "--build" ]; then
        check_dependencies "--build"
        build_proxy
    else
        check_dependencies
    fi

    # Clear old PID file
    rm -f "$PID_FILE"

    # Check for .env
    check_env

    # Install Python deps
    install_python_deps

    # Start services
    start_injection_server
    start_chat_server
    start_proxy

    # Print instructions
    print_instructions

    # Wait for signals
    wait
}

main "$@"
