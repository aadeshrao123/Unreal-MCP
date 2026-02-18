"""
Shared MCP server instance and UE5 bridge helper.

All tool modules import `mcp` and `_send` from here.

Communication goes through the C++ TCP bridge on port 55557.
The `execute_python` command runs Python code inside the editor,
replicating the old HTTP init_unreal.py behaviour.
"""

import json

from fastmcp import FastMCP
from _tcp_bridge import _tcp_send_raw

# ---------------------------------------------------------------------------
# MCP Server instance (shared across all tool modules)
# ---------------------------------------------------------------------------
mcp = FastMCP(
    "Unreal Engine",
    instructions=(
        "Control Unreal Engine 5 editor — create materials, blueprints, "
        "spawn actors, modify assets, and execute arbitrary Python."
    ),
)


# ---------------------------------------------------------------------------
# Bridge helper
# ---------------------------------------------------------------------------
def _send(code: str) -> str:
    """Send Python code to the UE5 editor via the C++ TCP bridge.

    The C++ bridge base64-encodes the code, runs it through
    IPythonScriptPlugin::ExecPythonCommandEx, captures the ``result``
    variable (or stdout), and returns it as JSON — identical semantics
    to the old HTTP init_unreal.py server.
    """
    try:
        resp = _tcp_send_raw("execute_python", {"code": code})

        # Unwrap TCP bridge envelope:
        #   {"status":"success","result":{"success":true,"result":<value>}}
        if resp.get("status") == "error":
            return f"Error: {resp.get('error', 'Unknown')}"

        inner = resp.get("result", resp)
        if isinstance(inner, dict):
            if inner.get("success") is False:
                return f"Error: {inner.get('error', 'Unknown')}"
            result = inner.get("result", "OK")
        else:
            result = inner

        return (
            json.dumps(result, default=str, indent=2)
            if not isinstance(result, str)
            else result
        )
    except Exception as exc:
        return f"Error: {exc}"
