from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_pie_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def pie_start(mode: str = "PIE") -> dict:
        return conn.call("pie.start", {"mode": mode})

    @mcp.tool()
    def pie_stop() -> dict:
        return conn.call("pie.stop")
