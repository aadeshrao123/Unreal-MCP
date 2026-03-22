package cmd

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/spf13/cobra"
)

// ParamSpec defines a single command parameter.
type ParamSpec struct {
	Name     string // TCP param name (snake_case)
	Type     string // "string", "bool", "int", "float", "json", "stringlist"
	Default  any    // default value
	Required bool
	Help     string
}

// CommandSpec defines a CLI command that maps to a TCP bridge command.
type CommandSpec struct {
	Name    string      // TCP command name (also the CLI subcommand name)
	Group   string      // help text grouping (e.g., "assets", "materials")
	Short   string      // one-line description
	Params  []ParamSpec // parameters
	LargeOp bool        // use 300s timeout
}

// registerCommand creates a cobra command from a CommandSpec and adds it to root.
func registerCommand(spec CommandSpec) {
	c := &cobra.Command{
		Use:   spec.Name,
		Short: spec.Short,
		GroupID: spec.Group,
		RunE: func(cmd *cobra.Command, args []string) error {
			params, err := buildParams(cmd, spec)
			if err != nil {
				return err
			}

			result, err := client.Send(spec.Name, params)
			if err != nil {
				exitError(err.Error())
			}

			// Remove _debug field for cleaner output
			delete(result, "_debug")

			enc := json.NewEncoder(os.Stdout)
			enc.SetIndent("", "  ")
			return enc.Encode(result)
		},
	}

	// Register flags from param specs
	for _, p := range spec.Params {
		flag := toFlagName(p.Name)
		switch p.Type {
		case "string", "json":
			def := ""
			if p.Default != nil {
				def = fmt.Sprintf("%v", p.Default)
			}
			c.Flags().String(flag, def, p.Help)
		case "bool":
			def := false
			if p.Default != nil {
				if b, ok := p.Default.(bool); ok {
					def = b
				}
			}
			c.Flags().Bool(flag, def, p.Help)
		case "int":
			def := 0
			if p.Default != nil {
				switch v := p.Default.(type) {
				case int:
					def = v
				case float64:
					def = int(v)
				}
			}
			c.Flags().Int(flag, def, p.Help)
		case "float":
			def := 0.0
			if p.Default != nil {
				if f, ok := p.Default.(float64); ok {
					def = f
				}
			}
			c.Flags().Float64(flag, def, p.Help)
		case "stringlist":
			var def []string
			c.Flags().StringSlice(flag, def, p.Help)
		}

		if p.Required {
			c.MarkFlagRequired(flag)
		}
	}

	rootCmd.AddCommand(c)
}

// registerCommands registers a batch of command specs.
func registerCommands(specs []CommandSpec) {
	for _, spec := range specs {
		registerCommand(spec)
	}
}

// ensureGroup creates a command group if it doesn't exist yet.
func ensureGroup(id, title string) {
	for _, g := range rootCmd.Groups() {
		if g.ID == id {
			return
		}
	}
	rootCmd.AddGroup(&cobra.Group{ID: id, Title: title})
}

// buildParams extracts flag values and builds the params map for the TCP command.
func buildParams(cmd *cobra.Command, spec CommandSpec) (map[string]any, error) {
	// If --json is provided globally, use that as the entire params
	if flagJSON != "" {
		return parseJSONInput(flagJSON)
	}

	params := make(map[string]any)

	for _, p := range spec.Params {
		flag := toFlagName(p.Name)

		// Skip flags that weren't explicitly set (use server defaults)
		if !cmd.Flags().Changed(flag) {
			// Still include required params with their defaults
			if p.Required && p.Default != nil {
				params[p.Name] = p.Default
			}
			continue
		}

		switch p.Type {
		case "string":
			val, _ := cmd.Flags().GetString(flag)
			params[p.Name] = val

		case "json":
			val, _ := cmd.Flags().GetString(flag)
			if val != "" {
				// Try to parse as JSON, if it fails, send as raw string
				var parsed any
				if err := json.Unmarshal([]byte(val), &parsed); err == nil {
					params[p.Name] = parsed
				} else {
					params[p.Name] = val
				}
			}

		case "bool":
			val, _ := cmd.Flags().GetBool(flag)
			params[p.Name] = val

		case "int":
			val, _ := cmd.Flags().GetInt(flag)
			params[p.Name] = val

		case "float":
			val, _ := cmd.Flags().GetFloat64(flag)
			params[p.Name] = val

		case "stringlist":
			val, _ := cmd.Flags().GetStringSlice(flag)
			if len(val) > 0 {
				params[p.Name] = val
			}
		}
	}

	return params, nil
}

// parseJSONInput parses JSON from a string or stdin (if value is "-").
func parseJSONInput(input string) (map[string]any, error) {
	var data []byte

	if input == "-" {
		var err error
		data, err = io.ReadAll(os.Stdin)
		if err != nil {
			return nil, fmt.Errorf("failed to read stdin: %w", err)
		}
	} else {
		data = []byte(input)
	}

	var result map[string]any
	if err := json.Unmarshal(data, &result); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	return result, nil
}

// toFlagName converts a snake_case param name to a CLI flag name.
// We keep snake_case for consistency with the TCP command names.
func toFlagName(name string) string {
	return strings.ReplaceAll(name, "_", "-")
}
