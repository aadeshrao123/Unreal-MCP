package cmd

func init() {
	ensureGroup("core", "Core")
	registerCommands(coreCommands)
}

var coreCommands = []CommandSpec{
	{
		Name:  "health_check",
		Group: "core",
		Short: "Check if the UE5 editor bridge is running and responsive",
	},
	{
		Name:  "execute_python",
		Group: "core",
		Short: "Execute arbitrary Python code inside the running Unreal Editor",
		Params: []ParamSpec{
			{Name: "code", Type: "string", Required: true, Help: "Python code to execute (set 'result' variable to return data)"},
		},
	},
}
