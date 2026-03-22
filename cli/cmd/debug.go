package cmd

func init() {
	ensureGroup("debug", "Debug")
	registerCommands(debugCommands)
}

var debugCommands = []CommandSpec{
	{
		Name:  "set_mcp_debug",
		Group: "debug",
		Short: "Enable or disable MCP debug mode (token tracking)",
		Params: []ParamSpec{
			{Name: "enabled", Type: "bool", Default: true, Help: "Enable debug mode"},
		},
	},
	{
		Name:  "get_mcp_token_stats",
		Group: "debug",
		Short: "Get MCP token usage statistics",
	},
}
