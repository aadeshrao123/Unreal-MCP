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
		Long:  "Verifies that the Unreal Editor is running and the C++ TCP bridge plugin is loaded and accepting connections. Returns editor version and status. Use this as the first command to verify your setup works.",
		Example: "ue-cli health_check",
	},
	{
		Name:  "execute_python",
		Group: "core",
		Short: "Execute arbitrary Python code inside the running Unreal Editor",
		Long:  "Runs Python code in the editor's Python environment where the 'unreal' module is pre-imported. Set a variable named 'result' to return structured data back. Use for operations not covered by other commands.",
		Example: `ue-cli execute_python --code "result = str(unreal.EditorAssetLibrary.list_assets('/Game'))"`,
		Params: []ParamSpec{
			{Name: "code", Type: "string", Required: true, Help: "Python code to execute (set 'result' variable to return data)"},
		},
	},
}
