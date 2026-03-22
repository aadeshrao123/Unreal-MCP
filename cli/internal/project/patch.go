package project

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
)

// UProjectFile represents the structure of a .uproject file.
type UProjectFile struct {
	raw map[string]any
}

// ReadUProject reads and parses a .uproject file.
func ReadUProject(projectDir, filename string) (*UProjectFile, error) {
	path := filepath.Join(projectDir, filename)
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read %s: %w", path, err)
	}

	var raw map[string]any
	if err := json.Unmarshal(data, &raw); err != nil {
		return nil, fmt.Errorf("failed to parse %s: %w", path, err)
	}

	return &UProjectFile{raw: raw}, nil
}

// HasPlugin checks if a plugin is already listed in the .uproject.
func (u *UProjectFile) HasPlugin(name string) bool {
	plugins, ok := u.raw["Plugins"].([]any)
	if !ok {
		return false
	}

	for _, p := range plugins {
		if plugin, ok := p.(map[string]any); ok {
			if pluginName, ok := plugin["Name"].(string); ok && pluginName == name {
				return true
			}
		}
	}
	return false
}

// AddPlugin adds a plugin entry to the .uproject plugins array.
func (u *UProjectFile) AddPlugin(name string, enabled bool) {
	plugins, ok := u.raw["Plugins"].([]any)
	if !ok {
		plugins = []any{}
	}

	entry := map[string]any{
		"Name":    name,
		"Enabled": enabled,
	}
	plugins = append(plugins, entry)
	u.raw["Plugins"] = plugins
}

// Save writes the .uproject file back to disk with proper formatting.
func (u *UProjectFile) Save(projectDir, filename string) error {
	path := filepath.Join(projectDir, filename)

	data, err := json.MarshalIndent(u.raw, "", "\t")
	if err != nil {
		return fmt.Errorf("failed to marshal .uproject: %w", err)
	}

	// Append newline
	data = append(data, '\n')

	if err := os.WriteFile(path, data, 0644); err != nil {
		return fmt.Errorf("failed to write %s: %w", path, err)
	}

	return nil
}

// EnsurePlugins ensures all required plugins are listed in the .uproject.
// Returns a list of plugins that were added.
func (u *UProjectFile) EnsurePlugins(plugins []string) []string {
	var added []string
	for _, name := range plugins {
		if !u.HasPlugin(name) {
			u.AddPlugin(name, true)
			added = append(added, name)
		}
	}
	return added
}
