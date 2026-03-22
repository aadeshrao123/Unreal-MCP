package cmd

import (
	"fmt"
	"net"
	"os"
	"strings"
	"time"

	"github.com/epicgames/ue-cli/internal/bridge"
	"github.com/epicgames/ue-cli/internal/project"
	"github.com/spf13/cobra"
)

var doctorCmd = &cobra.Command{
	Use:   "doctor",
	Short: "Diagnose the ue-cli setup (plugin, editor, connection)",
	RunE: func(cmd *cobra.Command, args []string) error {
		cwd, err := os.Getwd()
		if err != nil {
			return err
		}

		allOk := true

		// Check 1: Find .uproject
		projDir, uprojectFile, err := project.FindProjectRoot(cwd)
		if err != nil {
			printCheck(false, "Project", "No .uproject found")
			allOk = false
		} else {
			printCheck(true, "Project", fmt.Sprintf("%s/%s", projDir, uprojectFile))

			// Check 2: Plugin directory
			if project.PluginExists(projDir) {
				printCheck(true, "Plugin directory", "Plugins/UnrealMCP/ exists")
			} else {
				printCheck(false, "Plugin directory", "Plugins/UnrealMCP/ not found — run: ue-cli init")
				allOk = false
			}

			// Check 3: Plugin source
			if project.PluginSourceExists(projDir) {
				printCheck(true, "Plugin source", "Source/UnrealMCPBridge/ exists")
			} else {
				printCheck(false, "Plugin source", "Source/UnrealMCPBridge/ not found — run: ue-cli init")
				allOk = false
			}

			// Check 4: .uproject plugin entry
			uproject, readErr := project.ReadUProject(projDir, uprojectFile)
			if readErr == nil {
				if uproject.HasPlugin("UnrealMCP") {
					printCheck(true, "UProject entry", "UnrealMCP plugin listed and enabled")
				} else {
					printCheck(false, "UProject entry", "UnrealMCP not in .uproject — run: ue-cli init")
					allOk = false
				}
			}

			// Check 5: Port file
			portFile, exists := project.PortFileExists(projDir)
			if exists {
				data, _ := os.ReadFile(portFile)
				printCheck(true, "Port file", fmt.Sprintf("port %s", strings.TrimSpace(string(data))))
			} else {
				printCheck(false, "Port file", "Saved/UnrealMCP/port.txt not found — is the editor running?")
				allOk = false
			}
		}

		// Check 6: TCP connection
		port := bridge.ResolvePort(projDir)
		addr := fmt.Sprintf("127.0.0.1:%d", port)
		conn, err := net.DialTimeout("tcp", addr, 3*time.Second)
		if err != nil {
			printCheck(false, "TCP connection", fmt.Sprintf("Cannot connect to %s — is the editor running?", addr))
			allOk = false
		} else {
			conn.Close()
			printCheck(true, "TCP connection", fmt.Sprintf("Connected to %s", addr))

			// Check 7: Health check
			c := bridge.NewClient(projDir, flagPort, 5*time.Second)
			defer c.Close()
			resp, err := c.Send("health_check", nil)
			if err != nil {
				printCheck(false, "Health check", fmt.Sprintf("Failed: %s", err))
				allOk = false
			} else {
				status, _ := resp["status"].(string)
				if status == "error" {
					errMsg, _ := resp["error"].(string)
					printCheck(false, "Health check", fmt.Sprintf("Error: %s", errMsg))
					allOk = false
				} else {
					printCheck(true, "Health check", "Bridge is responsive")
				}
			}
		}

		fmt.Println()
		if allOk {
			fmt.Println("All checks passed. ue-cli is ready to use.")
		} else {
			fmt.Println("Some checks failed. See above for details.")
		}
		return nil
	},
}

func printCheck(ok bool, label, detail string) {
	marker := "x"
	if ok {
		marker = "ok"
	}
	fmt.Printf("  [%s] %-18s %s\n", marker, label, detail)
}

func init() {
	rootCmd.AddCommand(doctorCmd)
}
