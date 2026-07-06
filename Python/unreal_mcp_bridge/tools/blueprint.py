from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_blueprint_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def blueprint_create(
        package_path: str,
        asset_name: str,
        parent_class_path: str,
    ) -> dict:
        if not package_path:
            raise ValueError("package_path is required")
        if not asset_name:
            raise ValueError("asset_name is required")
        if not parent_class_path:
            raise ValueError("parent_class_path is required")
        return conn.call("blueprint.create", {
            "package_path": package_path,
            "asset_name": asset_name,
            "parent_class_path": parent_class_path,
        })

    @mcp.tool()
    def blueprint_compile(blueprint_path: str) -> dict:
        if not blueprint_path:
            raise ValueError("blueprint_path is required")
        return conn.call("blueprint.compile", {"blueprint_path": blueprint_path})

    @mcp.tool()
    def blueprint_list_variables(blueprint_path: str) -> dict:
        if not blueprint_path:
            raise ValueError("blueprint_path is required")
        return conn.call("blueprint.list_variables", {"blueprint_path": blueprint_path})

    @mcp.tool()
    def blueprint_get_graph_nodes(blueprint_path: str, graph_name: str = "") -> dict:
        if not blueprint_path:
            raise ValueError("blueprint_path is required")
        params: dict = {"blueprint_path": blueprint_path}
        if graph_name:
            params["graph_name"] = graph_name
        return conn.call("blueprint.get_graph_nodes", params)

    @mcp.tool()
    def blueprint_list_functions(blueprint_path: str, graph_name: str = "") -> dict:
        """Lightweight inventory of a Blueprint's graphs: name, graph_type
        (ubergraph/function/macro), node_count, and for function graphs the
        resolved input/output signature. Use this before blueprint_get_graph_nodes
        to see what's there without pulling every node."""
        if not blueprint_path:
            raise ValueError("blueprint_path is required")
        params: dict = {"blueprint_path": blueprint_path}
        if graph_name:
            params["graph_name"] = graph_name
        return conn.call("blueprint.list_functions", params)
