from __future__ import annotations

from fastmcp import FastMCP

from .config import get_settings
from .connection import UnrealConnection
from .tools import (
    register_editor_tools,
    register_blueprint_tools,
    register_asset_tools,
    register_compile_tools,
    register_pie_tools,
)


def main() -> None:
    settings = get_settings()
    conn = UnrealConnection(settings)
    conn.connect()

    mcp = FastMCP("unreal-mcp-bridge")

    register_editor_tools(mcp, conn)
    register_blueprint_tools(mcp, conn)
    register_asset_tools(mcp, conn)
    register_compile_tools(mcp, conn)
    register_pie_tools(mcp, conn)

    try:
        mcp.run(transport="stdio")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
