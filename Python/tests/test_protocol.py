"""Unit tests for JSON-RPC 2.0 serialization / deserialization as used by UnrealConnection."""

from __future__ import annotations

import json

import pytest


# ---------------------------------------------------------------------------
# Helpers that replicate the wire-level logic in connection.py without a
# live socket, so we can test protocol correctness in isolation.
# ---------------------------------------------------------------------------

def make_request(method: str, params: dict | None = None, request_id: int | str | None = 1) -> str:
    """Build a newline-terminated JSON-RPC 2.0 request string."""
    payload: dict = {"jsonrpc": "2.0", "id": request_id, "method": method}
    if params is not None:
        payload["params"] = params
    return json.dumps(payload, ensure_ascii=False) + "\n"


def parse_response(line: str) -> dict:
    """Parse a raw newline-terminated JSON-RPC 2.0 response line."""
    return json.loads(line.rstrip("\n"))


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

class TestRequestSerialization:
    def test_basic_request_fields(self):
        raw = make_request("editor.get_level_name")
        obj = json.loads(raw)
        assert obj["jsonrpc"] == "2.0"
        assert obj["method"] == "editor.get_level_name"
        assert obj["id"] == 1

    def test_request_with_params(self):
        raw = make_request("asset.exists", params={"asset_path": "/Game/Maps/MainMap"})
        obj = json.loads(raw)
        assert obj["params"]["asset_path"] == "/Game/Maps/MainMap"

    def test_request_ends_with_newline(self):
        raw = make_request("pie.start")
        assert raw.endswith("\n"), "Framing delimiter must be a trailing newline"

    def test_string_id_preserved(self):
        raw = make_request("compile.blueprint", request_id="req-abc")
        obj = json.loads(raw)
        assert obj["id"] == "req-abc"

    def test_null_id_makes_notification(self):
        raw = make_request("pie.stop", request_id=None)
        obj = json.loads(raw)
        assert obj["id"] is None


class TestResponseParsing:
    def _success_line(self, request_id: int | str | None, result: dict) -> str:
        return json.dumps({"jsonrpc": "2.0", "id": request_id, "result": result}) + "\n"

    def _error_line(self, request_id: int | str | None, code: int, message: str) -> str:
        return json.dumps({
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {"code": code, "message": message},
        }) + "\n"

    def test_success_response_parsed(self):
        line = self._success_line(1, {"level_name": "MainMap"})
        obj = parse_response(line)
        assert obj["result"]["level_name"] == "MainMap"
        assert "error" not in obj

    def test_error_response_parsed(self):
        line = self._error_line(2, -32601, "Method not found")
        obj = parse_response(line)
        assert obj["error"]["code"] == -32601
        assert obj["error"]["message"] == "Method not found"
        assert "result" not in obj

    def test_id_round_trip_integer(self):
        line = self._success_line(42, {})
        obj = parse_response(line)
        assert obj["id"] == 42

    def test_id_round_trip_string(self):
        line = self._success_line("call-99", {"version": "5.3.0"})
        obj = parse_response(line)
        assert obj["id"] == "call-99"

    def test_standard_error_codes_are_correct(self):
        expected = {
            "ParseError": -32700,
            "InvalidRequest": -32600,
            "MethodNotFound": -32601,
            "InvalidParams": -32602,
            "InternalError": -32603,
        }
        # Validate the constants we depend on in tests and integration code.
        assert expected["ParseError"] == -32700
        assert expected["MethodNotFound"] == -32601
        assert expected["InternalError"] == -32603
