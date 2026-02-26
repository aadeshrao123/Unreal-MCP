"""
TCP bridge for the C++ UnrealMCPBridge.

The C++ bridge handles Blueprint graph manipulation, editor commands, and
operations requiring direct access to K2Node APIs that aren't available
through the Python scripting plugin.

Port is auto-discovered from the port file written by the C++ bridge
at Saved/UnrealMCP/port.txt. This allows multiple Unreal Editor instances
to run simultaneously on different ports without configuration.

All tool modules that need the C++ bridge import `_tcp_send` from here.
"""

import json
import os
import socket
import logging
import struct
import time
import threading
from typing import Any, Dict, Optional

logger = logging.getLogger("UnrealMCP.TCPBridge")

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
TCP_HOST = "127.0.0.1"
DEFAULT_PORT = 55557
PORT_FILE = os.path.join("Saved", "UnrealMCP", "port.txt")
CONNECT_TIMEOUT = 10  # seconds
DEFAULT_RECV_TIMEOUT = 30  # seconds
LARGE_OP_RECV_TIMEOUT = 300  # seconds
BUFFER_SIZE = 65536  # 64 KB recv chunk size
MAX_RETRIES = 3
BASE_RETRY_DELAY = 0.5  # seconds

LARGE_OPERATION_COMMANDS = {
    "get_available_materials",
    "read_blueprint_content",
    "analyze_blueprint_graph",
    "build_material_graph",
    "performance_start_trace",
    "performance_analyze_insight",
}


def _resolve_port() -> int:
    """Resolve the TCP port for the C++ bridge.

    Priority:
        1. UNREAL_MCP_PORT environment variable (manual override)
        2. Saved/UnrealMCP/port.txt (auto-written by running editor)
        3. Default 55557
    """
    env_port = os.environ.get("UNREAL_MCP_PORT")
    if env_port:
        try:
            port = int(env_port)
            if 1 <= port <= 65535:
                logger.info("Using port %d from UNREAL_MCP_PORT env var", port)
                return port
        except ValueError:
            logger.warning("Invalid UNREAL_MCP_PORT value: %s", env_port)

    try:
        if os.path.exists(PORT_FILE):
            with open(PORT_FILE, "r", encoding="utf-8") as f:
                port = int(f.read().strip())
                if 1 <= port <= 65535:
                    logger.info("Using port %d from %s", port, PORT_FILE)
                    return port
    except (ValueError, OSError) as exc:
        logger.warning("Failed to read port file: %s", exc)

    logger.info("Using default port %d", DEFAULT_PORT)
    return DEFAULT_PORT


# ---------------------------------------------------------------------------
# TCP Connection
# ---------------------------------------------------------------------------
class _TCPConnection:
    """Thread-safe TCP connection to the C++ MCP bridge.

    Uses length-prefix framing: each response is preceded by a 4-byte
    big-endian length header, followed by that many bytes of JSON payload.

    The port is resolved lazily on first connection and refreshed
    on connection failure (handles editor restarts on a different port).
    """

    def __init__(self):
        self._sock: Optional[socket.socket] = None
        self._lock = threading.RLock()
        self._port: Optional[int] = None

    def _get_port(self, force_refresh: bool = False) -> int:
        if self._port is None or force_refresh:
            self._port = _resolve_port()
        return self._port

    # -- low-level helpers --------------------------------------------------

    def _make_socket(self) -> socket.socket:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(CONNECT_TIMEOUT)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)
        return sock

    def _close_unsafe(self):
        if self._sock:
            try:
                self._sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None

    def _ensure_connected(self) -> bool:
        """Connect only if not already connected. Reuses existing socket."""
        if self._sock is not None:
            return True
        try:
            self._sock = self._make_socket()
            self._sock.connect((TCP_HOST, self._get_port()))
            logger.info(
                "Connected to C++ bridge at %s:%d",
                TCP_HOST, self._get_port(),
            )
            return True
        except Exception:
            self._close_unsafe()
            return False

    def _recv_exact(self, n: int) -> bytes:
        """Read exactly n bytes from the socket."""
        data = bytearray()
        while len(data) < n:
            remaining = n - len(data)
            chunk = self._sock.recv(min(remaining, BUFFER_SIZE))
            if not chunk:
                raise ConnectionError(
                    f"Connection closed after {len(data)}/{n} bytes"
                )
            data.extend(chunk)
        return bytes(data)

    def _recv_response(self, command: str) -> bytes:
        """Receive a length-prefixed response from the C++ bridge."""
        if command in LARGE_OPERATION_COMMANDS:
            timeout = LARGE_OP_RECV_TIMEOUT
        else:
            timeout = DEFAULT_RECV_TIMEOUT
        self._sock.settimeout(timeout)

        # Read 4-byte big-endian length header
        header = self._recv_exact(4)
        payload_len = struct.unpack(">I", header)[0]

        if payload_len == 0:
            raise ConnectionError("Server sent empty response (length=0)")
        if payload_len > 100 * 1024 * 1024:
            raise ConnectionError(
                f"Response too large: {payload_len} bytes (>100MB)"
            )

        logger.info(
            "Expecting %d bytes for %s (timeout=%ds)",
            payload_len, command, timeout,
        )

        # Read exactly payload_len bytes of JSON
        return self._recv_exact(payload_len)

    # -- public API ---------------------------------------------------------

    def send_command(
        self,
        command: str,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Send a command to the C++ bridge and return the parsed response."""
        last_error = ""
        for attempt in range(MAX_RETRIES + 1):
            try:
                return self._send_once(command, params)
            except (ConnectionError, TimeoutError, socket.error, OSError) as exc:
                last_error = str(exc)
                logger.warning(
                    "TCP command %s failed (attempt %d): %s",
                    command, attempt + 1, exc,
                )
                with self._lock:
                    self._close_unsafe()
                    self._port = None
                if attempt < MAX_RETRIES:
                    delay = min(BASE_RETRY_DELAY * (2 ** attempt), 5.0)
                    time.sleep(delay)
            except Exception as exc:
                logger.error("Unexpected error sending TCP command: %s", exc)
                with self._lock:
                    self._close_unsafe()
                return {"status": "error", "error": str(exc)}

        return {
            "status": "error",
            "error": f"Failed after {MAX_RETRIES + 1} attempts: {last_error}",
        }

    def _send_once(
        self,
        command: str,
        params: Optional[Dict[str, Any]],
    ) -> Dict[str, Any]:
        with self._lock:
            if not self._ensure_connected():
                raise ConnectionError(
                    f"Cannot connect to C++ bridge at "
                    f"{TCP_HOST}:{self._get_port()}"
                )
            try:
                payload = json.dumps({
                    "type": command,
                    "params": params or {},
                })
                logger.info(
                    "Sending command: %s (%d bytes)",
                    command, len(payload),
                )
                self._sock.settimeout(10)
                self._sock.sendall(payload.encode("utf-8"))
                logger.info("Send complete, waiting for response...")

                raw = self._recv_response(command)
                logger.info("Received response: %d bytes", len(raw))
                text = raw.decode("utf-8")
                try:
                    response = json.loads(text)
                except json.JSONDecodeError as e:
                    start = max(0, e.pos - 100)
                    end = min(len(text), e.pos + 50)
                    context = (
                        f"...{text[start:e.pos]}"
                        f">>>HERE>>>"
                        f"{text[e.pos:end]}..."
                    )
                    raise ValueError(
                        f"JSON parse error at char {e.pos}: {e.msg} | "
                        f"Context: {context}"
                    ) from e

                if response.get("success") is False and "status" not in response:
                    response["status"] = "error"

                return response
            except Exception as exc:
                logger.error("_send_once failed for %s: %s", command, exc)
                self._close_unsafe()
                raise


# ---------------------------------------------------------------------------
# Module-level singleton
# ---------------------------------------------------------------------------
_conn: Optional[_TCPConnection] = None
_conn_lock = threading.Lock()


def _get_conn() -> _TCPConnection:
    global _conn
    with _conn_lock:
        if _conn is None:
            _conn = _TCPConnection()
        return _conn


def _tcp_send(
    command: str,
    params: Optional[Dict[str, Any]] = None,
) -> str:
    """Send a command to the C++ MCP bridge and return a JSON string result.

    This is the TCP equivalent of ``_send()`` in ``_bridge.py``.
    Returns a JSON-formatted string for consistency with the HTTP bridge.
    """
    try:
        resp = _get_conn().send_command(command, params)
        return json.dumps(resp, default=str, indent=2)
    except Exception as exc:
        return json.dumps({"status": "error", "error": str(exc)})


def _tcp_send_raw(
    command: str,
    params: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Send a command and return the raw dict (not JSON-serialized)."""
    try:
        return _get_conn().send_command(command, params)
    except Exception as exc:
        return {"status": "error", "error": str(exc)}


def _call(command: str, params: dict | None = None) -> str:
    """Send a command and return the JSON-formatted response string."""
    return json.dumps(
        _tcp_send_raw(command, params or {}),
        default=str,
        indent=2,
    )
