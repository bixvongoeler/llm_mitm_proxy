#!/bin/bash
# =============================================================================
# LLM MITM Proxy - Run Script
# =============================================================================
# Usage:
#   ./run.sh              # Start the proxy container
#   ./run.sh --setup      # First-time setup (create .env file)
#   ./run.sh --stop       # Stop the running container
#   ./run.sh --logs       # View container logs
#   ./run.sh --status     # Check container status
#   ./run.sh --help       # Show help
# =============================================================================

set -e

# Configuration
CONTAINER_NAME="llm-mitmproxy"
IMAGE_NAME="llm-mitmproxy:latest"
ENV_FILE=".env"
ENV_EXAMPLE=".env.example"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

show_help() {
    echo "LLM MITM Proxy - Run Script"
    echo ""
    echo "Usage: ./run.sh [OPTION]"
    echo ""
    echo "Options:"
    echo "  --setup     First-time setup: create .env file"
    echo "  --stop      Stop the running container"
    echo "  --logs      View container logs (follow mode)"
    echo "  --status    Check if container is running"
    echo "  --help      Show this help message"
    echo ""
    echo "Without options, starts the proxy container."
    echo ""
    echo "Environment variables (set in .env file):"
    echo "  LLMPROXY_API_KEY    - Required for chat functionality"
    echo "  LLMPROXY_ENDPOINT   - Required for chat functionality"
    echo "  PROXY_PORT          - Proxy port (default: 9999)"
    echo ""
}

check_docker() {
    if ! docker info >/dev/null 2>&1; then
        echo -e "${RED}Error: Docker is not running. Please start Docker first.${NC}"
        exit 1
    fi
}

setup() {
    echo -e "${CYAN}=== LLM MITM Proxy Setup ===${NC}"
    echo ""

    if [ ! -f "$ENV_FILE" ]; then
        if [ -f "$ENV_EXAMPLE" ]; then
            cp "$ENV_EXAMPLE" "$ENV_FILE"
            echo -e "${GREEN}Created .env file from template${NC}"
        else
            cat >"$ENV_FILE" <<'EOF'
# LLM MITM Proxy Configuration

LLMPROXY_API_KEY=your-api-key-here
LLMPROXY_ENDPOINT=https://your-llmproxy-endpoint.com/prod

# Optional: Custom proxy port (default: 9999)
# PROXY_PORT=9999
EOF
            echo -e "${GREEN}Created .env file${NC}"
        fi
        echo -e "${YELLOW}Please edit .env and add your LLMPROXY credentials${NC}"
    else
        echo -e "${GREEN}.env file already exists${NC}"
    fi

    echo ""
    echo "Next steps:"
    echo "  1. Edit .env and add your LLMPROXY_API_KEY and LLMPROXY_ENDPOINT"
    echo "  2. Install crt/proxy_ca.crt in your browser's certificate store"
    echo "  3. Run: ./run.sh"
    echo ""
}

stop_container() {
    if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
        echo -e "${CYAN}Stopping $CONTAINER_NAME...${NC}"
        docker stop "$CONTAINER_NAME" >/dev/null
        echo -e "${GREEN}Container stopped${NC}"
    else
        echo -e "${YELLOW}Container is not running${NC}"
    fi
}

view_logs() {
    if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
        docker logs -f "$CONTAINER_NAME"
    else
        echo -e "${YELLOW}Container is not running${NC}"
    fi
}

check_status() {
    if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
        echo -e "${GREEN}Container is running${NC}"
        docker ps -f name="$CONTAINER_NAME" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
    else
        echo -e "${YELLOW}Container is not running${NC}"
    fi
}

run_container() {
    check_docker

    if [ ! -f "$ENV_FILE" ]; then
        echo -e "${RED}Error: .env file not found${NC}"
        echo "Run './run.sh --setup' first"
        exit 1
    fi

    # Read PROXY_PORT from .env (default 9999)
    PROXY_PORT=$(grep -E "^PROXY_PORT=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 | tr -d ' "'"'" || echo "9999")
    [ -z "$PROXY_PORT" ] && PROXY_PORT=9999

    # Stop existing container if running
    if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
        echo -e "${YELLOW}Stopping existing container...${NC}"
        docker stop "$CONTAINER_NAME" >/dev/null
    fi
    docker rm "$CONTAINER_NAME" >/dev/null 2>&1 || true

    echo -e "${CYAN}Starting LLM MITM Proxy...${NC}"
    echo -e "  Proxy port: ${GREEN}$PROXY_PORT${NC}"
    echo -e "  Chat API:   ${GREEN}5001${NC}"
    echo ""

    docker run -d \
        --name "$CONTAINER_NAME" \
        -p "$PROXY_PORT:$PROXY_PORT" \
        -p 5001:5001 \
        --env-file "$ENV_FILE" \
        "$IMAGE_NAME"

    sleep 2

    if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
        echo -e "${GREEN}Container started successfully!${NC}"
        echo ""
        echo "Configure your browser to use HTTP proxy: localhost:$PROXY_PORT"
        echo "Chat API available at: http://localhost:5001"
        echo ""
        echo "Use './run.sh --logs' to view logs"
        echo "Use './run.sh --stop' to stop the proxy"
    else
        echo -e "${RED}Container failed to start. Check logs:${NC}"
        docker logs "$CONTAINER_NAME"
        exit 1
    fi
}

case "${1:-}" in
--help | -h)
    show_help
    ;;
--setup)
    check_docker
    setup
    ;;
--stop)
    stop_container
    ;;
--logs)
    view_logs
    ;;
--status)
    check_status
    ;;
"")
    run_container
    ;;
*)
    echo -e "${RED}Unknown option: $1${NC}"
    echo "Use './run.sh --help' for usage"
    exit 1
    ;;
esac
