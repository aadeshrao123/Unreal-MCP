package bridge

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	defaultPort = 55557
	envVarName  = "UNREAL_MCP_PORT"
	portFileRel = "Saved/UnrealMCP/port.txt"
)

// ResolvePort finds the TCP port for the C++ bridge.
//
// Priority:
//  1. UNREAL_MCP_PORT environment variable
//  2. Saved/UnrealMCP/port.txt (relative to projectDir)
//  3. Default 55557
func ResolvePort(projectDir string) int {
	// 1. Environment variable
	if envVal := os.Getenv(envVarName); envVal != "" {
		if port, err := strconv.Atoi(strings.TrimSpace(envVal)); err == nil && port >= 1 && port <= 65535 {
			return port
		}
	}

	// 2. Port file
	if projectDir != "" {
		portFile := filepath.Join(projectDir, portFileRel)
		if data, err := os.ReadFile(portFile); err == nil {
			if port, err := strconv.Atoi(strings.TrimSpace(string(data))); err == nil && port >= 1 && port <= 65535 {
				return port
			}
		}
	}

	// 3. Default
	return defaultPort
}
