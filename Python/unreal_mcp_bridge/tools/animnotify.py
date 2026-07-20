from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_animnotify_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def animnotify_list(animation_path: str, notify_class: str = "") -> dict:
        """Lists the notify events on an animation asset (sequence or montage).

        notify_class optionally filters by class name or path and matches subclasses, e.g.
        "AnimNotifyState_MotionWarping" also returns project subclasses of it.

        Returns {"notifies": [{"index", "class", "start_time", "duration", "track"}]}. The index is
        what statetree-style edits need — pass it to animnotify_set_window."""
        if not animation_path:
            raise ValueError("animation_path is required")
        params: dict = {"animation_path": animation_path}
        if notify_class:
            params["notify_class"] = notify_class
        return conn.call("animnotify.list", params)

    @mcp.tool()
    def animnotify_set_window(
        animation_path: str,
        index: int,
        start_time: float | None = None,
        duration: float | None = None,
    ) -> dict:
        """Retimes an existing notify event in place. index comes from animnotify_list. Pass start_time
        and/or duration (duration applies to notify states only).

        Edits the event in place rather than removing and re-adding it — a notify's Instanced sub-objects
        (a MotionWarping notify's RootMotionModifier, for instance) are rebuilt from class defaults on
        re-creation, which silently discards whatever the animator configured on them.

        Returns {"ok", "class", "start_time", "duration"} reflecting the stored values."""
        if not animation_path:
            raise ValueError("animation_path is required")
        if start_time is None and duration is None:
            raise ValueError("provide start_time and/or duration")
        params: dict = {"animation_path": animation_path, "index": index}
        if start_time is not None:
            params["start_time"] = start_time
        if duration is not None:
            params["duration"] = duration
        return conn.call("animnotify.set_window", params)
