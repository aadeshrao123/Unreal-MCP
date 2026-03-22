package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
)

var listCommandsCmd = &cobra.Command{
	Use:   "list_commands",
	Short: "List all available commands with flags, descriptions, and examples",
	Long:  "Outputs a comprehensive reference of all CLI commands. Designed for AI assistants to understand what each command does, what parameters it accepts, and how to use it.",
	RunE: func(cmd *cobra.Command, args []string) error {
		for _, c := range rootCmd.Commands() {
			if c.Hidden || c.Name() == "help" || c.Name() == "completion" || c.Name() == "list_commands" {
				continue
			}

			fmt.Printf("## %s\n", c.Name())
			fmt.Printf("  %s\n", c.Short)

			if c.Long != "" {
				fmt.Printf("  %s\n", c.Long)
			}

			hasFlags := false
			c.Flags().VisitAll(func(f *pflag.Flag) {
				if f.Name == "help" {
					return
				}
				if !hasFlags {
					fmt.Println("  Flags:")
					hasFlags = true
				}

				req := "optional"
				annotations := f.Annotations
				if _, ok := annotations[cobra.BashCompOneRequiredFlag]; ok {
					req = "REQUIRED"
				}

				def := ""
				if f.DefValue != "" && f.DefValue != "false" && f.DefValue != "0" && f.DefValue != "[]" && f.DefValue != "0.000000" {
					def = fmt.Sprintf(", default: %s", f.DefValue)
				}

				fmt.Printf("    --%s (%s, %s%s) — %s\n", f.Name, f.Value.Type(), req, def, f.Usage)
			})

			if c.Example != "" {
				fmt.Printf("  Example:\n    %s\n", c.Example)
			}

			fmt.Println()
		}
		return nil
	},
}

func init() {
	rootCmd.AddCommand(listCommandsCmd)
}
