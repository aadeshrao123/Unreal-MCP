package project

import (
	"embed"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
)

// PluginFS holds the embedded C++ plugin source files.
// This will be populated when the plugin/ directory exists at build time.
// For now, we provide a placeholder and the extraction logic.
//
//go:embed all:plugin
var PluginFS embed.FS

// ExtractPlugin extracts the embedded plugin files to the target project directory.
// It creates Plugins/UnrealMCP/ and writes all source files.
func ExtractPlugin(projectDir string) (int, error) {
	targetDir := filepath.Join(projectDir, "Plugins", "UnrealMCP")
	count := 0

	err := fs.WalkDir(PluginFS, "plugin", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		// Compute the relative path under the target directory
		// "plugin/UnrealMCP.uplugin" -> "UnrealMCP.uplugin"
		relPath, err := filepath.Rel("plugin", path)
		if err != nil {
			return err
		}
		if relPath == "." {
			return nil
		}

		targetPath := filepath.Join(targetDir, relPath)

		if d.IsDir() {
			return os.MkdirAll(targetPath, 0755)
		}

		// Read embedded file
		data, err := PluginFS.ReadFile(path)
		if err != nil {
			return fmt.Errorf("failed to read embedded file %s: %w", path, err)
		}

		// Ensure parent directory exists
		if err := os.MkdirAll(filepath.Dir(targetPath), 0755); err != nil {
			return fmt.Errorf("failed to create directory for %s: %w", targetPath, err)
		}

		// Write file
		if err := os.WriteFile(targetPath, data, 0644); err != nil {
			return fmt.Errorf("failed to write %s: %w", targetPath, err)
		}

		count++
		return nil
	})

	return count, err
}
