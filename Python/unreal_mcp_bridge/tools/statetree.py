from __future__ import annotations

from fastmcp import FastMCP

from ..connection import UnrealConnection


def register_statetree_tools(mcp: FastMCP, conn: UnrealConnection) -> None:
    @mcp.tool()
    def statetree_add_state(
        tree_path: str,
        state_name: str,
        parent_state_id: str = "",
    ) -> dict:
        """Adds a state to a StateTree asset. If parent_state_id is empty the state is
        added under the tree's Root subtree (created if missing); otherwise it becomes a
        child of the given state. Returns {"state_id": <guid>} to pass to
        statetree_add_task / statetree_add_transition."""
        if not tree_path:
            raise ValueError("tree_path is required")
        if not state_name:
            raise ValueError("state_name is required")
        params: dict = {"tree_path": tree_path, "state_name": state_name}
        if parent_state_id:
            params["parent_state_id"] = parent_state_id
        return conn.call("statetree.add_state", params)

    @mcp.tool()
    def statetree_add_evaluator(tree_path: str, struct_path: str) -> dict:
        """Adds a tree-level Evaluator node of the given struct type (e.g.
        "/Script/IFSR.STEval_SenseTarget"). Evaluators run every tick and expose outputs
        (target/distance/flags) that transitions and tasks can bind to. Returns
        {"node_id": <guid>}."""
        if not tree_path:
            raise ValueError("tree_path is required")
        if not struct_path:
            raise ValueError("struct_path is required")
        return conn.call("statetree.add_evaluator", {
            "tree_path": tree_path,
            "struct_path": struct_path,
        })

    @mcp.tool()
    def statetree_add_task(tree_path: str, state_id: str, struct_path: str) -> dict:
        """Adds a Task node of the given struct type to a state. The task runs while the
        state is active (e.g. a MoveTo or PlayAnimation task). Returns {"node_id": <guid>}
        to bind task inputs from evaluator outputs via statetree_add_binding."""
        if not tree_path:
            raise ValueError("tree_path is required")
        if not state_id:
            raise ValueError("state_id is required")
        if not struct_path:
            raise ValueError("struct_path is required")
        return conn.call("statetree.add_task", {
            "tree_path": tree_path,
            "state_id": state_id,
            "struct_path": struct_path,
        })

    @mcp.tool()
    def statetree_add_transition(
        tree_path: str,
        from_state_id: str,
        to_state_id: str,
        trigger: str = "OnTick",
    ) -> dict:
        """Adds a GotoState transition from one state to another. trigger is one of
        "OnTick" (default, re-evaluated every tick), "OnStateCompleted",
        "OnStateSucceeded", or "OnEvent". The transition is unconditional; add a bound
        condition in the editor (or a future statetree_add_condition) to gate it.
        Returns {"transition_id": <guid>}."""
        if not tree_path:
            raise ValueError("tree_path is required")
        if not from_state_id:
            raise ValueError("from_state_id is required")
        if not to_state_id:
            raise ValueError("to_state_id is required")
        return conn.call("statetree.add_transition", {
            "tree_path": tree_path,
            "from_state_id": from_state_id,
            "to_state_id": to_state_id,
            "trigger": trigger,
        })

    @mcp.tool()
    def statetree_add_binding(
        tree_path: str,
        source_node_id: str,
        source_path: str,
        target_node_id: str,
        target_path: str,
    ) -> dict:
        """Binds an output property of a source node (evaluator/task, by node_id) to an
        input property of a target node. Paths are property names relative to the node's
        instance data, e.g. source_path="TargetActor", target_path="Target". Returns
        {"ok": true}."""
        for name, value in (
            ("tree_path", tree_path),
            ("source_node_id", source_node_id),
            ("source_path", source_path),
            ("target_node_id", target_node_id),
            ("target_path", target_path),
        ):
            if not value:
                raise ValueError(f"{name} is required")
        return conn.call("statetree.add_binding", {
            "tree_path": tree_path,
            "source_node_id": source_node_id,
            "source_path": source_path,
            "target_node_id": target_node_id,
            "target_path": target_path,
        })

    @mcp.tool()
    def statetree_compile(tree_path: str) -> dict:
        """Compiles the StateTree from its editor data (states/tasks/transitions/bindings)
        into runtime data and updates the compiled hash. Call after finishing edits.
        Returns {"compiled": true|false}."""
        if not tree_path:
            raise ValueError("tree_path is required")
        return conn.call("statetree.compile", {"tree_path": tree_path})
