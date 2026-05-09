from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_compile_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def compile_trigger_live_coding() -> dict:
        return conn.call("compile.trigger_live_coding")

    @mcp.tool()
    def compile_status() -> dict:
        return conn.call("compile.status")
