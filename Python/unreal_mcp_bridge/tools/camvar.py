from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_camvar_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def camvar_create_collection(package_path: str, name: str) -> dict:
        """Creates a new empty Gameplay Camera System UCameraVariableCollection asset —
        the container for typed CameraVariables shared across camera rigs (bind a rig
        node's CameraParameter .Variable to one so an offset/boom/fov transfers between
        rigs). package_path is the content folder (e.g. "/Game/05_Dev/Camera/CameraAssets"),
        name the asset name. Marks it dirty; call asset_save to persist.
        Returns {"ok": true, "path": ...}."""
        if not package_path:
            raise ValueError("package_path is required")
        if not name:
            raise ValueError("name is required")
        return conn.call("camvar.create_collection", {
            "package_path": package_path,
            "name": name,
        })

    @mcp.tool()
    def camvar_add(
        collection_path: str,
        var_type: str,
        name: str,
        default_value: str = "",
    ) -> dict:
        """Adds a typed camera variable to a UCameraVariableCollection (authoring GCS
        variables is otherwise editor-only). var_type is the short type name:
        Boolean, Integer32, Float, Double, Vector2f, Vector2d, Vector3f, Vector3d,
        Vector4f, Vector4d, Rotator3f, Rotator3d, Transform3f, Transform3d
        (a leading 'U' / trailing 'CameraVariable' are tolerated). name becomes the
        variable's DisplayName. default_value is optional Unreal ImportText for the
        default, matching the type — e.g. "(X=0,Y=45,Z=-5)" for Vector3d, "68.0" for
        Double, "true" for Boolean, "(Pitch=0,Yaw=0,Roll=0)" for Rotator3d.
        Call asset_save afterwards to persist. Returns {"ok","name","type","guid","object"}."""
        if not collection_path:
            raise ValueError("collection_path is required")
        if not var_type:
            raise ValueError("var_type is required")
        if not name:
            raise ValueError("name is required")
        params = {
            "collection_path": collection_path,
            "var_type": var_type,
            "name": name,
        }
        if default_value:
            params["default_value"] = default_value
        return conn.call("camvar.add", params)

    @mcp.tool()
    def camvar_list(collection_path: str) -> dict:
        """Lists the variables in a UCameraVariableCollection. Returns
        {"variables": [{index, name, type, guid, default_value}]}."""
        if not collection_path:
            raise ValueError("collection_path is required")
        return conn.call("camvar.list", {"collection_path": collection_path})

    @mcp.tool()
    def camvar_set_default(collection_path: str, name: str, value: str) -> dict:
        """Sets the default value of an existing variable (matched by name/DisplayName)
        via Unreal ImportText (same value format as camvar_add's default_value).
        Call asset_save afterwards to persist. Returns {"ok": true}."""
        if not collection_path:
            raise ValueError("collection_path is required")
        if not name:
            raise ValueError("name is required")
        if not value:
            raise ValueError("value is required")
        return conn.call("camvar.set_default", {
            "collection_path": collection_path,
            "name": name,
            "value": value,
        })
