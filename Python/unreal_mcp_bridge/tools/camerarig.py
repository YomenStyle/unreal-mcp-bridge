from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_camerarig_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def camerarig_get_info(rig_path: str) -> dict:
        """Lists a Gameplay Camera System UCameraRigAsset's camera nodes. Returns
        {"root_node_class": ..., "nodes": [{"class": ..., "name": ...}]}. Use the class
        names to target camerarig_set_node_property (e.g. "BoomArmCameraNode")."""
        if not rig_path:
            raise ValueError("rig_path is required")
        return conn.call("camerarig.get_info", {"rig_path": rig_path})

    @mcp.tool()
    def camerarig_set_node_property(
        rig_path: str,
        node_class: str,
        property_name: str,
        value: str,
        node_index: int = 0,
    ) -> dict:
        """Sets a property on a camera node (found by short class name) via reflection
        ImportText. node_index selects which node when several share a class (default 0).
        value uses Unreal's text format. Camera parameters wrap their value in a Value
        field, e.g. set a Boom Arm offset with
        node_class="BoomArmCameraNode", property_name="BoomOffset",
        value="(Value=(X=-200.0,Y=60.0,Z=60.0))". Returns {"ok": true, "node": ...}."""
        for name, val in (
            ("rig_path", rig_path), ("node_class", node_class),
            ("property_name", property_name), ("value", value),
        ):
            if not val:
                raise ValueError(f"{name} is required")
        return conn.call("camerarig.set_node_property", {
            "rig_path": rig_path,
            "node_class": node_class,
            "property_name": property_name,
            "value": value,
            "node_index": node_index,
        })

    @mcp.tool()
    def camerarig_build(rig_path: str) -> dict:
        """Rebuilds the camera rig (validates data + rebuilds allocation info) and marks it
        dirty. Call after set_node_property changes. Returns {"ok": true}."""
        if not rig_path:
            raise ValueError("rig_path is required")
        return conn.call("camerarig.build", {"rig_path": rig_path})
