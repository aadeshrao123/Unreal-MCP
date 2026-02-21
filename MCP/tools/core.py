"""Core tools — execute_python and health_check."""

import json

from _bridge import mcp, _send
from _tcp_bridge import _tcp_send_raw


@mcp.tool()
def execute_python(code: str) -> str:
    """Execute arbitrary Python code inside the running Unreal Editor.

    The `unreal` module is pre-imported. Set a variable named `result`
    to return structured data back to Claude Code.
    """
    return _send(code)


@mcp.tool()
def health_check() -> str:
    """Check if the UE5 editor bridge is running and responsive."""
    try:
        resp = _tcp_send_raw("health_check")
        if resp.get("status") == "error":
            return f"Error: {resp.get('error', 'Editor not reachable')}"
        return json.dumps(resp.get("result", resp), indent=2)
    except Exception as exc:
        return f"Error: {exc}"
