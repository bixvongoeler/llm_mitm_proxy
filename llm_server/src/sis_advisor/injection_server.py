"""
Injection Server - Unix socket server for C proxy communication.

Receives HTTP responses from C proxy, injects chat widget into HTML pages,
returns modified response. Uses simple length-prefixed binary protocol.

Protocol:
    C -> Python: [4B url_len][url][4B content_len][http_response]
    Python -> C: [4B response_len][modified_response] (len=0 means no change)
"""

import gzip
import io
import os
import re
import socket
import struct
import zlib
from typing import NamedTuple

from .config import get_config
from .widget import get_widget_bundle


class HTTPResponse(NamedTuple):
    """Parsed HTTP response."""

    status_line: bytes
    headers: dict[bytes, bytes]
    body: bytes
    raw_headers: bytes  # Original headers for reconstruction


def parse_http_response(data: bytes) -> HTTPResponse | None:
    """Parse HTTP response into components."""
    # Find header/body boundary
    header_end = data.find(b"\r\n\r\n")
    if header_end == -1:
        return None

    raw_headers = data[: header_end + 4]
    body = data[header_end + 4 :]

    # Split headers
    header_lines = data[:header_end].split(b"\r\n")
    if not header_lines:
        return None

    status_line = header_lines[0]

    # Parse headers into dict (lowercase keys for easy lookup)
    headers: dict[bytes, bytes] = {}
    for line in header_lines[1:]:
        if b":" in line:
            key, value = line.split(b":", 1)
            headers[key.strip().lower()] = value.strip()

    return HTTPResponse(status_line, headers, body, raw_headers)


def decompress_body(body: bytes, encoding: bytes | None) -> bytes:
    """Decompress body based on Content-Encoding header."""
    if not encoding:
        return body

    encoding_lower = encoding.lower()

    try:
        if encoding_lower == b"gzip":
            return gzip.decompress(body)
        elif encoding_lower == b"deflate":
            # deflate can be raw or zlib-wrapped, try both
            try:
                return zlib.decompress(body)
            except zlib.error:
                return zlib.decompress(body, -zlib.MAX_WBITS)
        elif encoding_lower == b"br":
            # Brotli - would need brotli package, skip for now
            print("Warning: Brotli encoding not supported, skipping modification")
            return body
    except Exception as e:
        print(f"Decompression failed: {e}")

    return body


def compress_body(body: bytes, encoding: bytes | None) -> bytes:
    """Re-compress body to match original encoding."""
    if not encoding:
        return body

    encoding_lower = encoding.lower()

    try:
        if encoding_lower == b"gzip":
            buf = io.BytesIO()
            with gzip.GzipFile(fileobj=buf, mode="wb") as f:
                f.write(body)
            return buf.getvalue()
        elif encoding_lower == b"deflate":
            return zlib.compress(body)
    except Exception as e:
        print(f"Compression failed: {e}")

    return body


def inject_chat_widget(html: bytes) -> bytes:
    """Inject chat widget before </body> tag."""
    widget = get_widget_bundle().encode("utf-8")

    # Find </body> tag (case-insensitive)
    body_match = re.search(rb"</body\s*>", html, re.IGNORECASE)
    if not body_match:
        # No </body>, try </html>
        html_match = re.search(rb"</html\s*>", html, re.IGNORECASE)
        if html_match:
            return html[: html_match.start()] + widget + html[html_match.start() :]
        # No closing tag found, append at end
        return html + widget

    return html[: body_match.start()] + widget + html[body_match.start() :]


def rebuild_response(
    status_line: bytes,
    original_headers: dict[bytes, bytes],
    new_body: bytes,
    was_compressed: bool,
    encoding: bytes | None,
) -> bytes:
    """Rebuild HTTP response with new body and updated Content-Length."""
    # Re-compress if original was compressed
    if was_compressed and encoding:
        final_body = compress_body(new_body, encoding)
    else:
        final_body = new_body
        # Remove Content-Encoding if we're not compressing
        original_headers.pop(b"content-encoding", None)

    # Update Content-Length
    original_headers[b"content-length"] = str(len(final_body)).encode()

    # Remove chunked transfer encoding since we have full body
    if original_headers.get(b"transfer-encoding", b"").lower() == b"chunked":
        del original_headers[b"transfer-encoding"]

    # Rebuild headers
    header_lines = [status_line]
    for key, value in original_headers.items():
        # Use original case for common headers
        key_str = key.decode("latin-1")
        # Title-case the header name
        key_titled = "-".join(word.capitalize() for word in key_str.split("-"))
        header_lines.append(f"{key_titled}: {value.decode('latin-1')}".encode("latin-1"))

    headers_bytes = b"\r\n".join(header_lines) + b"\r\n\r\n"
    return headers_bytes + final_body


def process_response(url: str, http_response: bytes) -> bytes:
    """
    Process HTTP response, inject chat widget if HTML.

    Returns modified response, or empty bytes if no modification needed.
    """
    # Parse response
    parsed = parse_http_response(http_response)
    if not parsed:
        print("  Failed to parse HTTP response")
        return b""

    # Check Content-Type for HTML
    content_type = parsed.headers.get(b"content-type", b"")
    if b"text/html" not in content_type.lower():
        print(f"  Not HTML (Content-Type: {content_type.decode('latin-1', errors='replace')})")
        return b""

    # Get body, handling compression
    encoding = parsed.headers.get(b"content-encoding")
    was_compressed = encoding is not None

    body = decompress_body(parsed.body, encoding)
    if body == parsed.body and was_compressed:
        # Decompression failed or unsupported
        print("  Could not decompress, skipping")
        return b""

    # Inject widget
    modified_body = inject_chat_widget(body)
    if modified_body == body:
        print("  No injection point found")
        return b""

    print(f"  Injected chat widget ({len(body)} -> {len(modified_body)} bytes)")

    # Rebuild response
    return rebuild_response(
        parsed.status_line,
        parsed.headers.copy(),
        modified_body,
        was_compressed,
        encoding,
    )


def recv_exact(conn: socket.socket, n: int) -> bytes:
    """Receive exactly n bytes from socket."""
    data = b""
    while len(data) < n:
        chunk = conn.recv(min(65536, n - len(data)))
        if not chunk:
            raise ConnectionError("Connection closed while receiving")
        data += chunk
    return data


def handle_connection(conn: socket.socket) -> None:
    """Handle a single connection from the C proxy."""
    try:
        # Read url_len (4 bytes, big-endian)
        url_len = struct.unpack("!I", recv_exact(conn, 4))[0]

        # Read url
        url = recv_exact(conn, url_len).decode("utf-8", errors="replace")

        # Read content_len (4 bytes, big-endian)
        content_len = struct.unpack("!I", recv_exact(conn, 4))[0]

        # Read content
        content = recv_exact(conn, content_len)

        print(f"Request: {url} ({content_len} bytes)")

        # Process
        modified = process_response(url, content)

        # Send response
        response_len = struct.pack("!I", len(modified))
        conn.sendall(response_len)
        if modified:
            conn.sendall(modified)

        print(f"Response: {len(modified)} bytes {'(modified)' if modified else '(no change)'}")

    except Exception as e:
        print(f"Error handling connection: {e}")
        # Send empty response on error
        try:
            conn.sendall(struct.pack("!I", 0))
        except Exception:
            pass


def run_server() -> None:
    """Run the injection server."""
    config = get_config()
    socket_path = config.injection_socket_path

    # Remove existing socket file
    try:
        os.unlink(socket_path)
    except OSError:
        if os.path.exists(socket_path):
            raise

    # Create Unix domain socket
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.bind(socket_path)
    sock.listen(5)

    # Make socket accessible
    os.chmod(socket_path, 0o777)

    print(f"Injection server listening on {socket_path}")
    print("Press Ctrl+C to stop\n")

    try:
        while True:
            conn, _ = sock.accept()
            try:
                handle_connection(conn)
            finally:
                conn.close()
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        sock.close()
        try:
            os.unlink(socket_path)
        except OSError:
            pass


if __name__ == "__main__":
    run_server()
