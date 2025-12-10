from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Optional, Union

import requests
from dotenv import load_dotenv
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

# -----------------------
# Config & HTTP utilities
# -----------------------


@dataclass(frozen=True)
class ClientConfig:
    endpoint: str
    api_key: str
    timeout: float = 118.0  # seconds, applied to both connect & read

    @staticmethod
    def from_env() -> "ClientConfig":
        # Explicitly load .env from current working directory
        cwd_env = Path.cwd() / ".env"
        load_dotenv(dotenv_path=cwd_env, override=True)

        endpoint = os.getenv("LLMPROXY_ENDPOINT")
        api_key = os.getenv("LLMPROXY_API_KEY")

        if not endpoint or not api_key:
            raise ValueError(
                "LLMProxy configuration error:\n"
                "Missing LLMPROXY_ENDPOINT or LLMPROXY_API_KEY.\n\n"
                "Make sure your .env file is in the SAME DIRECTORY where you run python.\n"
                "\nExample .env:\n"
                "    LLMPROXY_ENDPOINT=https://your-endpoint\n"
                "    LLMPROXY_API_KEY=your-api-key\n"
            )

        return ClientConfig(endpoint=endpoint, api_key=api_key)


def _build_session() -> requests.Session:
    """Session with retries and connection pooling."""
    s = requests.Session()
    retries = Retry(
        total=3,
        connect=3,
        read=3,
        backoff_factor=0.5,
        status_forcelist=(429, 500, 502, 503, 504),
        allowed_methods=frozenset(["POST"]),
        raise_on_status=False,
    )
    adapter = HTTPAdapter(max_retries=retries, pool_connections=10, pool_maxsize=10)
    s.mount("http://", adapter)
    s.mount("https://", adapter)
    return s


# -----------------------
# Core client
# -----------------------


class LLMProxy:
    """
    @brief Client for interacting with the LLMProxy service.

    @details Provides methods to generate LLM responses, retrieve RAG context,
             upload files for RAG, and query available models. Handles authentication,
             retries, and error handling automatically.

    @note Requires LLMPROXY_ENDPOINT and LLMPROXY_API_KEY environment variables
          or a .env file in the current working directory.
    """

    def __init__(self) -> None:
        """
        @brief Initialize the LLMProxy client.

        @details Loads configuration from environment variables and creates
                 an HTTP session with retry logic.

        @throws ValueError If required environment variables are missing.
        """
        self.config = ClientConfig.from_env()
        self.session = _build_session()

    def _headers(
        self, request_type: str, extra: Optional[Dict[str, str]] = None
    ) -> Dict[str, str]:
        """
        @brief Build HTTP headers for API requests.

        @param request_type The type of request (e.g., "call", "retrieve", "add").
        @param extra        Optional additional headers to include.

        @return Dictionary of HTTP headers.
        """
        base = {
            "x-api-key": self.config.api_key,
            "request_type": request_type,
        }
        if extra:
            base.update(extra)
        return base

    def _post_json(
        self,
        request_type: str,
        payload: Dict[str, Any],
    ) -> Dict:
        """
        @brief Send a JSON POST request to the LLMProxy server.

        @details Sends a POST request with JSON payload, handling errors and
                 parsing the response. None values are automatically removed
                 from the payload.

        @param request_type The type of request for the header.
        @param payload      Dictionary of request parameters.

        @return Dictionary containing the server response or error details.
                On error, contains 'error' and 'status_code' keys.
        """
        # Remove None values to avoid sending nulls unnecessarily
        clean_payload = {k: v for k, v in payload.items() if v is not None}

        try:
            resp = self.session.post(
                self.config.endpoint,
                headers=self._headers(request_type),
                json=clean_payload,
                timeout=self.config.timeout,
            )
        except requests.exceptions.RequestException as e:
            return {"error": f"Network error: {e}", "status_code": None}

        if 200 <= resp.status_code < 300:
            try:
                return resp.json()
            except ValueError:
                # JSON decode failed; return text for visibility
                return {"error": "Invalid JSON in response", "status_code": resp.status_code}
        else:
            # Try to surface server-provided error details
            detail: str
            try:
                detail = resp.json().get("error", resp.text)
            except ValueError:
                detail = resp.text
            return {
                "error": f"HTTP {resp.status_code}: {detail}",
                "status_code": resp.status_code,
            }

    # -------- Public methods --------

    def retrieve(
        self,
        query: str,
        session_id: str,
        rag_threshold: float,
        rag_k: int,
    ) -> Dict:
        """
        @brief Retrieve relevant context for RAG from uploaded documents.

        @details Fetches relevant context based on a given query and session_id,
                 allowing for RAG context reuse across different sessions. Filters
                 results using similarity threshold and quantity limits.

        @param query         The query string to search for relevant context.
        @param session_id    Session identifier for context retrieval. Should not
                             contain hyphens (-).
        @param rag_threshold Minimum similarity threshold (0.0 to 1.0) for a chunk
                             to be included. Default recommendation: 0.5.
        @param rag_k         Number of retrieved chunks to return (0 to 10).

        @return List of dictionaries, each containing:
                - doc_id: Document identifier
                - summary: LLM-generated summary of the document
                - chunks: List of text chunks matching the query

        @note This function is useful for verifying chunks before passing them
              to generate(), or for global files that apply to all users.
        """
        payload = {
            "query": query,
            "session_id": session_id,
            "rag_threshold": rag_threshold,
            "rag_k": rag_k,
        }
        return self._post_json("retrieve", payload)

    def model_info(self) -> Dict:
        """
        @brief Fetch information about available LLM models.

        @details Retrieves the full list of models currently accessible with
                 your API key. Default models include GPT4o-mini, Claude Haiku,
                 and Microsoft Phi3. Additional models may be available depending
                 on your access level.

        @return Dictionary containing available model information.
        """
        return self._post_json("model_info", {})

    def generate(
        self,
        model: str,
        system: str,
        query: str,
        temperature: Optional[float] = None,
        lastk: Optional[int] = None,
        session_id: Optional[str] = "GenericSession",
        rag_threshold: Optional[float] = 0.5,
        rag_usage: Optional[bool] = False,
        rag_k: Optional[int] = 5,
    ) -> Dict:
        """
        @brief Generate a response from an LLM.

        @details Sends a query to the specified LLM model with optional context
                 management and RAG (Retrieval Augmented Generation) support.
                 The proxy maintains conversation history per session_id.

        @param model        Model identifier (e.g., "4o-mini", "azure-phi3",
                            "us.anthropic.claude-3-haiku-20240307-v1:0").
                            Use model_info() to see available models.
        @param system       System prompt providing instructions for how the LLM
                            should respond (e.g., tone, format, constraints).
        @param query        The user query for the LLM to respond to.
        @param temperature  Controls randomness in output (0.0 to 2.0).
                            Lower values = more deterministic. Default: 0.7.
        @param lastk        Number of previous request-response pairs to include
                            as context (>= 0). Default: 0 (no context).
        @param session_id   Session identifier for context tracking. Should not
                            contain hyphens (-). Default: "GenericSession".
        @param rag_threshold Minimum similarity threshold for RAG chunks (0.0 to 1.0).
                            Default: 0.5.
        @param rag_usage    Enable RAG to augment queries with uploaded documents.
                            Default: False.
        @param rag_k        Number of chunks to retrieve for RAG (0 to 10).
                            Default: 5.

        @return Dictionary containing:
                - result: The LLM's response text
                - rag_context: Context used for RAG (if any)
                On error, contains 'error' and 'status_code' keys.

        @note Requests taking longer than 59 seconds will timeout.
              Ensure query/system strings are properly JSON-escaped.
        """
        payload = {
            "model": model,
            "system": system,
            "query": query,
            "temperature": temperature,
            "lastk": lastk,
            "session_id": session_id,
            "rag_threshold": rag_threshold,
            "rag_usage": rag_usage,
            "rag_k": rag_k,
        }
        res = self._post_json("call", payload)
        if "error" in res:
            return res
        # Defensive extraction
        return res
        # result_text = res.get("result")
        # rag_context = res.get("rag_context")
        # return {"response": result_text, "rag_context": rag_context, "raw": res}

    def upload_file(
        self,
        file_path: Union[str, Path],
        session_id: str,
        mime_type: Optional[str] = None,
        description: Optional[str] = None,
        strategy: Optional[str] = "smart",
    ) -> Dict:
        """
        @brief Upload a file for RAG (Retrieval Augmented Generation).

        @details Uploads a file (typically PDF) to be chunked and indexed for
                 retrieval during generate() calls. The file becomes available
                 for RAG queries within the specified session.

        @param file_path    Path to the file to upload. Supports str or Path objects.
        @param session_id   Session identifier where the file will be available
                            for RAG. Should not contain hyphens (-).
        @param mime_type    MIME type of the file. Auto-detected if not provided
                            (defaults to "application/pdf" for .pdf files).
        @param description  Optional description of the file content.
        @param strategy     Chunking strategy for splitting documents into
                            retrievable chunks. Default: "smart".

        @return Dictionary containing server response or error details.
                On error, contains 'error' and 'status_code' keys.

        @note File size limit is 4MB. For larger files, split into smaller
              parts before uploading.
        """
        path = Path(file_path)
        if not path.exists():
            return {"error": f"File not found: {path}", "status_code": None}

        if mime_type is None:
            # Minimal sniffing; caller can override
            mime_type = (
                "application/pdf"
                if path.suffix.lower() == ".pdf"
                else "application/octet-stream"
            )

        params = {
            "description": description,
            "session_id": session_id,
            "strategy": strategy,
        }
        # Remove None values
        params = {k: v for k, v in params.items() if v is not None}

        files = {
            # Include a filename so the server can store it meaningfully
            "params": (None, json.dumps(params), "application/json"),
            "file": (None, path.open("rb"), mime_type),
        }

        try:
            resp = self.session.post(
                self.config.endpoint,
                headers=self._headers("add"),
                files=files,
                timeout=self.config.timeout,
            )
        except requests.exceptions.RequestException as e:
            return {"error": f"Network error: {e}", "status_code": None}

        if 200 <= resp.status_code < 300:
            try:
                return resp.json()
            except ValueError:
                # If server returns plain text success
                return {"message": resp.text}
        else:
            try:
                detail = resp.json().get("error", resp.text)
            except ValueError:
                detail = resp.text
            return {
                "error": f"HTTP {resp.status_code}: {detail}",
                "status_code": resp.status_code,
            }

    def upload_text(
        self,
        text: str,
        session_id: str,
        description: Optional[str] = None,
        strategy: Optional[str] = "smart",
    ) -> Dict:
        """
        @brief Upload raw text content for RAG (Retrieval Augmented Generation).

        @details Uploads text content to be chunked and indexed for retrieval
                 during generate() calls. Useful for adding dynamic content
                 without creating a file.

        @param text         The text content to upload.
        @param session_id   Session identifier where the text will be available
                            for RAG. Should not contain hyphens (-).
        @param description  Optional description of the text content.
        @param strategy     Chunking strategy for splitting the text into
                            retrievable chunks. Default: "smart".

        @return Dictionary containing server response or error details.
                On error, contains 'error' and 'status_code' keys.
        """
        params = {
            "description": description,
            "session_id": session_id,
            "strategy": strategy,
        }
        params = {k: v for k, v in params.items() if v is not None}

        files = {
            "params": (None, json.dumps(params), "application/json"),
            "text": (None, text, "application/text"),
        }

        try:
            resp = self.session.post(
                self.config.endpoint,
                headers=self._headers("add"),
                files=files,
                timeout=self.config.timeout,
            )
        except requests.exceptions.RequestException as e:
            return {"error": f"Network error: {e}", "status_code": None}

        if 200 <= resp.status_code < 300:
            try:
                return resp.json()
            except ValueError:
                return {"message": resp.text}
        else:
            try:
                detail = resp.json().get("error", resp.text)
            except ValueError:
                detail = resp.text
            return {
                "error": f"HTTP {resp.status_code}: {detail}",
                "status_code": resp.status_code,
            }
