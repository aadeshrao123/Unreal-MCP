package project

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// FindProjectRoot walks up from startDir looking for a .uproject file.
// Returns the directory containing the .uproject and the .uproject filename.
func FindProjectRoot(startDir string) (projectDir string, uprojectFile string, err error) {
	dir := startDir

	for {
		entries, readErr := os.ReadDir(dir)
		if readErr != nil {
			break
		}

		for _, entry := range entries {
			if !entry.IsDir() && strings.HasSuffix(entry.Name(), ".uproject") {
				return dir, entry.Name(), nil
			}
		}

		parent := filepath.Dir(dir)
		if parent == dir {
			break
		}
		dir = parent
	}

	return "", "", fmt.Errorf("no .uproject file found (searched from %s to filesystem root)", startDir)
}

// PluginExists checks if the UnrealMCP plugin directory exists in the project.
func PluginExists(projectDir string) bool {
	pluginDir := filepath.Join(projectDir, "Plugins", "UnrealMCP")
	info, err := os.Stat(pluginDir)
	return err == nil && info.IsDir()
}

// PluginSourceExists checks if the C++ source directory exists.
func PluginSourceExists(projectDir string) bool {
	sourceDir := filepath.Join(projectDir, "Plugins", "UnrealMCP", "Source", "UnrealMCPBridge")
	info, err := os.Stat(sourceDir)
	return err == nil && info.IsDir()
}

// PortFileExists checks if the port file exists and returns its path.
func PortFileExists(projectDir string) (string, bool) {
	portFile := filepath.Join(projectDir, "Saved", "UnrealMCP", "port.txt")
	_, err := os.Stat(portFile)
	return portFile, err == nil
}
