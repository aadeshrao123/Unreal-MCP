"""
UnrealMCP — MCP Server for Unreal Engine 5

Pip-installable package. After `pip install unrealmcp`, run the server with:
    unrealmcp              # stdio transport (default)

Or use directly in MCP configs:
    "command": "unrealmcp"
"""

__version__ = "1.0.0"


def main():
    """Entry point for the `unrealmcp` console command."""
    from unrealmcp._bridge import mcp  # noqa: F401

    # Importing tools registers them via @mcp.tool() decorators
    import unrealmcp.tools  # noqa: F401

    mcp.run()
