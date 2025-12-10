"""
Main entry point for SIS Advisor services.

Usage:
    uv run python main.py injection  # Start injection server
    uv run python main.py chat       # Start chat server
    uv run python main.py test       # Run LLMProxy test
    uv run python main.py            # Show usage

Supported models (from LLMProxy):
    - gpt-4.1-mini
    - gpt-4.1-nano
    - 4o-mini
    - us.anthropic.claude-3-haiku-20240307-v1:0
    - azure-phi3
    - us.meta.llama3-2-3b-instruct-v1:0
    - us.meta.llama3-2-1b-instruct-v1:0
    - us.meta.llama3-1-8b-instruct-v1:0
"""

import json
import sys


def run_test():
    """Run a quick LLMProxy test."""
    from llmproxy import LLMProxy

    print("Starting LLMProxy test...")

    client = LLMProxy()
    print("Client initialized")

    info = client.model_info()
    print("Available models:")
    print(json.dumps(info, indent=4))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("Available commands:")
        print("  injection  - Start the injection server (Unix socket for C proxy)")
        print("  chat       - Start the chat server (HTTP API for browser)")
        print("  test       - Test LLMProxy connection")
        print()
        return

    command = sys.argv[1].lower()

    if command == "injection":
        from sis_advisor.injection_server import run_server

        run_server()
    elif command == "chat":
        from sis_advisor.chat_server import run_server

        run_server()
    elif command == "test":
        run_test()
    else:
        print(f"Unknown command: {command}")
        print("Use 'injection', 'chat', or 'test'")
        sys.exit(1)


if __name__ == "__main__":
    main()
