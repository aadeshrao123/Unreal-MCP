package cmd

import (
	"fmt"
	"os"
	"time"

	"github.com/epicgames/ue-cli/internal/bridge"
	"github.com/spf13/cobra"
)

// Version is set at build time via ldflags.
var Version = "dev"

var (
	flagPort    int
	flagTimeout int
	flagJSON    string
	client      *bridge.Client
)

var rootCmd = &cobra.Command{
	Use:   "ue-cli",
	Short: "CLI for controlling Unreal Engine 5 Editor",
	Long: `ue-cli talks directly to the UnrealMCPBridge C++ plugin running
inside the Unreal Editor via TCP. No Python, no MCP, no dependencies.

Usage:
  ue-cli health_check
  ue-cli find_assets --class_type material --path /Game
  ue-cli spawn_actor --name MyCube --type StaticMeshActor
  ue-cli build_material_graph --json '{"material_path":"/Game/M_Test","nodes":[...]}'`,
	Version: Version,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		// Skip client init for commands that don't need it
		if cmd.Name() == "init" || cmd.Name() == "doctor" || cmd.Name() == "version" {
			return
		}

		projectDir := findProjectDir()
		timeout := time.Duration(0)
		if flagTimeout > 0 {
			timeout = time.Duration(flagTimeout) * time.Second
		}
		client = bridge.NewClient(projectDir, flagPort, timeout)
	},
}

func init() {
	rootCmd.PersistentFlags().IntVar(&flagPort, "port", 0, "TCP port override (default: auto-discover)")
	rootCmd.PersistentFlags().IntVar(&flagTimeout, "timeout", 0, "Timeout in seconds (default: 30, large ops: 300)")
	rootCmd.PersistentFlags().StringVar(&flagJSON, "json", "", "Full params as JSON string (use - for stdin)")
}

// Execute runs the root command.
func Execute() {
	if err := rootCmd.Execute(); err != nil {
		os.Exit(2)
	}
}

// findProjectDir walks up from CWD looking for a .uproject file.
func findProjectDir() string {
	dir, err := os.Getwd()
	if err != nil {
		return ""
	}

	for {
		entries, err := os.ReadDir(dir)
		if err != nil {
			break
		}
		for _, entry := range entries {
			if !entry.IsDir() && len(entry.Name()) > 9 && entry.Name()[len(entry.Name())-9:] == ".uproject" {
				return dir
			}
		}
		parent := dir[:max(0, len(dir)-1)]
		for parent != "" && parent[len(parent)-1] != '/' && parent[len(parent)-1] != '\\' {
			parent = parent[:len(parent)-1]
		}
		if parent == "" || parent == dir {
			break
		}
		dir = parent[:len(parent)-1]
	}

	return ""
}

func exitError(msg string) {
	fmt.Fprintf(os.Stderr, `{"status":"error","error":"%s"}`+"\n", msg)
	os.Exit(1)
}

func exitErrorf(format string, args ...any) {
	exitError(fmt.Sprintf(format, args...))
}
