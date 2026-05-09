from __future__ import annotations

import itertools
import json
import socket
import threading
from typing import Any

from .config import Settings


class MCPRemoteError(Exception):
    """Raised when the UE plugin returns a JSON-RPC error object."""

    def __init__(self, code: int, message: str, data: Any = None) -> None:
        super().__init__(f"[{code}] {message}")
        self.code = code
        self.message = message
        self.data = data


class UnrealConnection:
    """Persistent TCP connection to the UE MCP Bridge plugin."""

    def __init__(self, settings: Settings) -> None:
        self._settings = settings
        self._sock: socket.socket | None = None
        self._lock = threading.Lock()
        self._id_counter = itertools.count(1)
        # Buffer for partial lines received across multiple recv calls.
        self._recv_buffer = b""

    def connect(self) -> None:
        """Open a TCP connection to the UE plugin listener. Raises on failure."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(self._settings.connect_timeout)
        try:
            sock.connect((self._settings.host, self._settings.port))
        except OSError as exc:
            sock.close()
            raise ConnectionError(
                f"Cannot connect to UE plugin at "
                f"{self._settings.host}:{self._settings.port}: {exc}"
            ) from exc
        # Switch to blocking mode with request timeout for subsequent IO.
        sock.settimeout(self._settings.request_timeout)
        self._sock = sock

    def _ensure_connected(self) -> None:
        """Reconnect once if the socket appears to have been closed."""
        if self._sock is None:
            self.connect()

    def call(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        timeout: float | None = None,
    ) -> dict[str, Any]:
        """Send a JSON-RPC request and return the result dict.

        Raises MCPRemoteError when the plugin returns an error response.
        Raises ConnectionError / TimeoutError on IO failures.
        """
        with self._lock:
            self._ensure_connected()
            assert self._sock is not None

            request_id = next(self._id_counter)
            payload: dict[str, Any] = {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
            }
            if params is not None:
                payload["params"] = params

            line = json.dumps(payload, ensure_ascii=False) + "\n"
            encoded = line.encode("utf-8")

            # Send the request.
            try:
                self._sock.sendall(encoded)
            except OSError as exc:
                self._sock = None
                raise ConnectionError(f"Send failed: {exc}") from exc

            # Override timeout for this call if requested.
            if timeout is not None:
                self._sock.settimeout(timeout)

            # Receive the response line.
            response_line = self._recv_line()

            if timeout is not None:
                # Restore default timeout.
                self._sock.settimeout(self._settings.request_timeout)

        # Parse and validate the response.
        try:
            response = json.loads(response_line)
        except json.JSONDecodeError as exc:
            raise ConnectionError(f"Invalid JSON from server: {exc}") from exc

        if response.get("id") != request_id:
            raise ConnectionError(
                f"Response id mismatch: expected {request_id}, got {response.get('id')}"
            )

        if "error" in response:
            err = response["error"]
            raise MCPRemoteError(
                code=err.get("code", 0),
                message=err.get("message", "Unknown error"),
                data=err.get("data"),
            )

        return response.get("result") or {}

    def _recv_line(self) -> str:
        """Read bytes until a newline delimiter, respecting max_line_bytes."""
        assert self._sock is not None
        max_bytes = self._settings.max_line_bytes

        while b"\n" not in self._recv_buffer:
            if len(self._recv_buffer) >= max_bytes:
                self._sock = None
                raise ConnectionError(
                    f"Response line exceeded {max_bytes} bytes without a newline delimiter"
                )
            try:
                chunk = self._sock.recv(4096)
            except socket.timeout as exc:
                self._sock = None
                raise TimeoutError("Timed out waiting for response from UE plugin") from exc
            except OSError as exc:
                self._sock = None
                raise ConnectionError(f"Receive failed: {exc}") from exc

            if not chunk:
                self._sock = None
                raise ConnectionError("Connection closed by UE plugin while waiting for response")
            self._recv_buffer += chunk

        newline_pos = self._recv_buffer.index(b"\n")
        line_bytes = self._recv_buffer[:newline_pos]
        self._recv_buffer = self._recv_buffer[newline_pos + 1:]
        return line_bytes.decode("utf-8")

    def close(self) -> None:
        """Close the connection."""
        with self._lock:
            if self._sock is not None:
                try:
                    self._sock.close()
                except OSError:
                    pass
                self._sock = None
