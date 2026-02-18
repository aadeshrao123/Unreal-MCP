"""
UnrealMCP — MCP Server for Unreal Engine 5

Entry point. Imports the shared server instance and all tool modules,
then runs on stdio transport for Claude Code.

Usage:
    python mcp_server.py          # stdio (default, for Claude Code)
    pip install fastmcp requests  # dependencies
"""

from _bridge import mcp  # noqa: F401 — server instance

# Importing tools registers them via @mcp.tool() decorators
import tools  # noqa: F401

if __name__ == "__main__":
    mcp.run()
