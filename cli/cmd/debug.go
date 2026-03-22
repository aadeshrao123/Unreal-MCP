package cmd

func init() {
	ensureGroup("debug", "Debug")
	registerCommands(debugCommands)
}

var debugCommands = []CommandSpec{
	{
		Name:    "set_mcp_debug",
		Group:   "debug",
		Short:   "Enable or disable MCP debug mode (token tracking)",
		Long:    "Toggles MCP debug mode which enables token usage tracking on every MCP command. When enabled, the bridge measures the approximate token size of each request and response, accumulates totals per command, and tracks call counts. Use get_mcp_token_stats to read the accumulated statistics. Useful for identifying which commands produce oversized responses that waste LLM context budget.",
		Example: `ue-cli set_mcp_debug --enabled
ue-cli set_mcp_debug --enabled=false`,
		Params: []ParamSpec{
			{Name: "enabled", Type: "bool", Default: true, Help: "Enable debug mode"},
		},
	},
	{
		Name:    "get_mcp_token_stats",
		Group:   "debug",
		Short:   "Get MCP token usage statistics",
		Long:    "Returns accumulated token usage statistics from the MCP bridge. Shows per-command call counts, total input/output tokens, and averages. Only collects data when debug mode is enabled via set_mcp_debug. Use this to audit which MCP commands consume the most tokens and optimize your workflow accordingly.",
		Example: "ue-cli get_mcp_token_stats",
	},
}
