"""Debug tools — token estimation, response size analysis."""

from _bridge import mcp
from _tcp_bridge import _call


@mcp.tool()
def set_mcp_debug(enabled: bool = True) -> str:
    """Enable or disable token debug estimation on MCP responses.

    When enabled, every command response shows a token debug header:
        [TOKEN DEBUG] command_name — ~1,289 tokens (5,156 chars)

    This appears at the top of every response so you can immediately
    see which commands consume the most tokens.

    Disabling clears accumulated stats.

    Args:
        enabled: True to enable, False to disable and clear stats.
    """
    return _call("set_mcp_debug", {"enabled": enabled})


@mcp.tool()
def get_mcp_token_stats() -> str:
    """Get accumulated per-command token usage statistics.

    Returns commands sorted by max_estimated_tokens (worst offenders first).
    Each entry shows: call_count, avg/max/total estimated tokens, response chars.

    Enable debug mode first with set_mcp_debug(enabled=True).
    """
    return _call("get_mcp_token_stats")
