package cmd

import (
	"fmt"
	"os"

	"github.com/epicgames/ue-cli/internal/project"
	"github.com/spf13/cobra"
)

var initForce bool

var initCmd = &cobra.Command{
	Use:   "init",
	Short: "Install the UnrealMCPBridge plugin into the current UE project",
	Long: `Detects the .uproject in the current directory (or parent directories),
extracts the embedded C++ plugin source, and patches the .uproject to enable it.

After running init, open the project in Unreal Editor — it will compile the
plugin automatically on first launch.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		cwd, err := os.Getwd()
		if err != nil {
			return fmt.Errorf("failed to get working directory: %w", err)
		}

		// Step 1: Find .uproject
		projDir, uprojectFile, err := project.FindProjectRoot(cwd)
		if err != nil {
			return fmt.Errorf("no .uproject found: %w\n\nRun this command from inside a UE project directory", err)
		}
		fmt.Printf("Found project: %s/%s\n", projDir, uprojectFile)

		// Step 2: Check if plugin already exists
		if project.PluginExists(projDir) && !initForce {
			if project.PluginSourceExists(projDir) {
				fmt.Println("UnrealMCPBridge plugin already installed.")
				fmt.Println("Use --force to reinstall/update.")
				return nil
			}
			fmt.Println("Plugin directory exists but source is missing. Reinstalling...")
		}

		// Step 3: Extract embedded plugin files
		fmt.Println("Installing UnrealMCPBridge plugin...")
		fileCount, err := project.ExtractPlugin(projDir)
		if err != nil {
			return fmt.Errorf("failed to extract plugin: %w", err)
		}
		fmt.Printf("  Extracted %d files to Plugins/UnrealMCP/\n", fileCount)

		// Step 4: Patch .uproject
		uproject, err := project.ReadUProject(projDir, uprojectFile)
		if err != nil {
			return fmt.Errorf("failed to read .uproject: %w", err)
		}

		requiredPlugins := []string{
			"UnrealMCP",
			"PythonScriptPlugin",
			"EditorScriptingUtilities",
			"EnhancedInput",
		}
		added := uproject.EnsurePlugins(requiredPlugins)

		if len(added) > 0 {
			if err := uproject.Save(projDir, uprojectFile); err != nil {
				return fmt.Errorf("failed to update .uproject: %w", err)
			}
			fmt.Printf("  Added to %s: %v\n", uprojectFile, added)
		} else {
			fmt.Printf("  %s already has all required plugins\n", uprojectFile)
		}

		// Step 5: Success
		fmt.Println()
		fmt.Println("Done! Next steps:")
		fmt.Println("  1. Open your project in Unreal Editor")
		fmt.Println("  2. The plugin will compile automatically on first launch")
		fmt.Println("  3. Run: ue-cli health_check")
		return nil
	},
}

func init() {
	initCmd.Flags().BoolVar(&initForce, "force", false, "Force reinstall even if plugin exists")
	rootCmd.AddCommand(initCmd)
}
