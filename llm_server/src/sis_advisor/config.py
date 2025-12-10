"""Minimal configuration for SIS Advisor."""

import os
from dataclasses import dataclass, field


def _get_default_host() -> str:
    """Get default host based on environment.

    Uses CHAT_HOST env var if set, otherwise defaults to 0.0.0.0
    (works in Docker and allows local testing).
    """
    return os.environ.get("CHAT_HOST", "0.0.0.0")


@dataclass
class Config:
    """Application configuration."""

    # Chat server settings
    chat_server_host: str = field(default_factory=_get_default_host)
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
