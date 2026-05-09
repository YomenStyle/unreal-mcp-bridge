from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_asset_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def asset_list(package_path: str, recursive: bool = False) -> dict:
        if not package_path:
            raise ValueError("package_path is required")
        return conn.call("asset.list", {
            "package_path": package_path,
            "recursive": recursive,
        })

    @mcp.tool()
    def asset_get_metadata(object_path: str) -> dict:
        if not object_path:
            raise ValueError("object_path is required")
        return conn.call("asset.get_metadata", {"object_path": object_path})

    @mcp.tool()
    def asset_save(object_path: str) -> dict:
        if not object_path:
            raise ValueError("object_path is required")
        return conn.call("asset.save", {"object_path": object_path})
