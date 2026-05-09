from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_editor_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def editor_get_status() -> dict:
        return conn.call("editor.get_status")

    @mcp.tool()
    def editor_list_actors(
        filter_class: str | None = None,
        max_count: int = 1000,
    ) -> dict:
        params: dict = {"max_count": max_count}
        if filter_class:
            params["filter_class"] = filter_class
        return conn.call("editor.list_actors", params)

    @mcp.tool()
    def editor_spawn_actor(
        class_path: str,
        location: list[float] | None = None,
        rotation: list[float] | None = None,
    ) -> dict:
        if not class_path:
            raise ValueError("class_path is required")
        params: dict = {"class_path": class_path}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        return conn.call("editor.spawn_actor", params)
