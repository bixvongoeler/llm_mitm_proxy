"""
Chat Server - Flask API for browser communication.

Simplified endpoints:
- GET  /health  - Health check
- GET  /welcome - Welcome message + suggested questions
- POST /init    - Initialize session (upload course content)
- POST /chat    - Send message and get response
- POST /upload  - Upload transcript PDF
"""

import os
import tempfile
from pathlib import Path

from flask import Flask, jsonify, request
from flask_cors import CORS

from .advisor import get_advisor
from .config import get_config


def create_app() -> Flask:
    """Create and configure the Flask application."""
    app = Flask(__name__)

    # Enable CORS for SIS domain and localhost
    CORS(
        app,
        origins=[
            "https://sis.it.tufts.edu",
            "http://localhost:*",
            "http://127.0.0.1:*",
        ],
        supports_credentials=True,
    )

    advisor = get_advisor()

    @app.route("/health", methods=["GET"])
    def health():
        """Health check endpoint."""
        return jsonify({"status": "ok", "service": "sis-advisor"})

    @app.route("/welcome", methods=["GET"])
    def welcome():
        """Get welcome message and suggested questions."""
        return jsonify(advisor.get_welcome_response())

    @app.route("/init", methods=["POST"])
    def init_session():
        """
        Initialize a new session by uploading course content.

        Request body:
            {"session_id": "user_abc123"}

        Response:
            {"success": true}
            or
            {"error": "error message"}
        """
        data = request.get_json()

        if not data:
            return jsonify({"error": "No JSON data provided"}), 400

        session_id = data.get("session_id")
        if not session_id:
            return jsonify({"error": "session_id is required"}), 400

        result = advisor.initialize_session(session_id)

        if "error" in result:
            return jsonify({"error": result["error"]}), 500

        return jsonify({"success": True})

    @app.route("/chat", methods=["POST"])
    def chat():
        """
        Handle chat messages.

        Request body:
            {
                "message": "user message",
                "session_id": "user_abc123",
                "page_content": "raw HTML from current page",  // optional
                "transcript_summary": "summary from localStorage"  // optional
            }

        Response:
            {"response": "assistant response"}
            or
            {"error": "error message"}
        """
        data = request.get_json()

        if not data:
            return jsonify({"error": "No JSON data provided"}), 400

        message = data.get("message", "").strip()
        if not message:
            return jsonify({"error": "Message is required"}), 400

        session_id = data.get("session_id")
        if not session_id:
            return jsonify({"error": "session_id is required"}), 400

        # Get optional context
        page_content = data.get("page_content")
        transcript_summary = data.get("transcript_summary")

        result = advisor.chat(
            message=message,
            session_id=session_id,
            page_content=page_content,
            transcript_summary=transcript_summary,
        )

        if "error" in result:
            return jsonify({"error": result["error"]}), 500

        return jsonify({"response": result["response"]})

    @app.route("/upload", methods=["POST"])
    def upload():
        """
        Upload transcript PDF and get summary.

        Request:
            multipart/form-data with:
            - file: The PDF file
            - session_id: User's session ID

        Response:
            {
                "success": true,
                "message": "Transcript uploaded and analyzed!",
                "summary": "LLM-generated summary for localStorage"
            }
            or
            {"error": "error message"}
        """
        if "file" not in request.files:
            return jsonify({"error": "No file provided"}), 400

        file = request.files["file"]
        if file.filename == "":
            return jsonify({"error": "No file selected"}), 400

        session_id = request.form.get("session_id")
        if not session_id:
            return jsonify({"error": "session_id is required"}), 400

        # Check file size (4MB limit)
        file.seek(0, 2)
        size = file.tell()
        file.seek(0)

        if size > 4 * 1024 * 1024:
            return jsonify({"error": "File too large. Maximum size is 4MB."}), 400

        # Save to temp file and process
        try:
            suffix = Path(file.filename or "upload").suffix or ".pdf"
            with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as tmp:
                file.save(tmp)
                tmp_path = Path(tmp.name)

            result = advisor.upload_transcript(tmp_path, session_id)

            # Clean up temp file
            os.unlink(tmp_path)

            if "error" in result:
                return jsonify({"error": result["error"]}), 500

            return jsonify(
                {
                    "success": True,
                    "message": result.get("message", "Transcript processed"),
                    "summary": result.get("summary", ""),
                }
            )

        except Exception as e:
            return jsonify({"error": f"Upload failed: {str(e)}"}), 500

    return app


def run_server() -> None:
    """Run the chat server."""
    config = get_config()
    app = create_app()

    print(
        f"Starting SIS Advisor on http://{config.chat_server_host}:{config.chat_server_port}"
    )
    print("Endpoints:")
    print("  GET  /health  - Health check")
    print("  GET  /welcome - Welcome message")
    print("  POST /init    - Initialize session")
    print("  POST /chat    - Send chat message")
    print("  POST /upload  - Upload transcript")
    print()

    app.run(
        host=config.chat_server_host,
        port=config.chat_server_port,
        debug=False,
        threaded=True,
    )


if __name__ == "__main__":
    run_server()
