"""Minimal configuration for SIS Advisor."""

from dataclasses import dataclass


@dataclass
class Config:
    """Application configuration."""

    # Chat server settings
    chat_server_host: str = "127.0.0.1"
    chat_server_port: int = 5001

    # Injection server settings
    injection_socket_path: str = "/tmp/llm_proxy.sock"


# Singleton config instance
_config: Config | None = None


def get_config() -> Config:
    """Get the application configuration."""
    global _config
    if _config is None:
        _config = Config()
    return _config
