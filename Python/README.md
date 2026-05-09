# unreal-mcp-bridge (Python)

Python MCP server that bridges MCP-compatible clients (e.g. Claude) to Unreal Editor
via JSON-RPC 2.0 over TCP.

## Install

```bash
pip install -e .
```

## Configure

Copy `.env.example` or export environment variables:

```bash
export UMCP_HOST=127.0.0.1
export UMCP_PORT=30100
```

See the top-level [README](../README.md) for the full variable reference.

## Run

```bash
unreal-mcp-bridge
```

The server connects to a running UE Editor instance and exposes all registered
MCP tools over stdio for any MCP-compatible client.
