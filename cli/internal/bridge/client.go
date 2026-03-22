package bridge

import (
	"encoding/json"
	"fmt"
	"math"
	"net"
	"sync"
	"time"
)

const (
	tcpHost        = "127.0.0.1"
	connectTimeout = 10 * time.Second
	defaultTimeout = 30 * time.Second
	largeOpTimeout = 300 * time.Second
	maxRetries     = 3
	baseRetryDelay = 500 * time.Millisecond
)

// largeOperationCommands get a 300s timeout instead of 30s.
var largeOperationCommands = map[string]bool{
	"get_available_materials":    true,
	"read_blueprint_content":     true,
	"analyze_blueprint_graph":    true,
	"build_material_graph":       true,
	"build_material_function_graph": true,
	"performance_start_trace":    true,
	"performance_analyze_insight": true,
	"add_widget":                 true,
	"set_widget_properties":      true,
	"duplicate_widget":           true,
}

// Client manages a TCP connection to the C++ UnrealMCPBridge.
type Client struct {
	conn       net.Conn
	mu         sync.Mutex
	port       int
	projectDir string
	portOverride int // from --port flag, 0 = auto-discover
	timeoutOverride time.Duration // from --timeout flag, 0 = auto
}

// NewClient creates a new bridge client.
func NewClient(projectDir string, portOverride int, timeoutOverride time.Duration) *Client {
	return &Client{
		projectDir:      projectDir,
		portOverride:    portOverride,
		timeoutOverride: timeoutOverride,
	}
}

func (c *Client) resolvePort(forceRefresh bool) int {
	if c.portOverride > 0 {
		return c.portOverride
	}
	if c.port == 0 || forceRefresh {
		c.port = ResolvePort(c.projectDir)
	}
	return c.port
}

func (c *Client) connect() error {
	port := c.resolvePort(false)
	addr := fmt.Sprintf("%s:%d", tcpHost, port)

	conn, err := net.DialTimeout("tcp", addr, connectTimeout)
	if err != nil {
		return fmt.Errorf("cannot connect to C++ bridge at %s: %w", addr, err)
	}

	// Set socket options
	if tcpConn, ok := conn.(*net.TCPConn); ok {
		tcpConn.SetNoDelay(true)
		tcpConn.SetKeepAlive(true)
		tcpConn.SetReadBuffer(4 * 1024 * 1024)
		tcpConn.SetWriteBuffer(4 * 1024 * 1024)
	}

	c.conn = conn
	return nil
}

func (c *Client) close() {
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
}

func (c *Client) getTimeout(command string) time.Duration {
	if c.timeoutOverride > 0 {
		return c.timeoutOverride
	}
	if largeOperationCommands[command] {
		return largeOpTimeout
	}
	return defaultTimeout
}

// Send sends a command to the C++ bridge and returns the parsed response.
func (c *Client) Send(command string, params map[string]any) (map[string]any, error) {
	var lastErr error

	for attempt := 0; attempt <= maxRetries; attempt++ {
		result, err := c.sendOnce(command, params)
		if err == nil {
			return result, nil
		}

		lastErr = err
		c.mu.Lock()
		c.close()
		c.port = 0 // force re-resolve on next attempt
		c.mu.Unlock()

		if attempt < maxRetries {
			delay := time.Duration(math.Min(
				float64(baseRetryDelay)*math.Pow(2, float64(attempt)),
				5000,
			))
			time.Sleep(delay)
		}
	}

	return nil, fmt.Errorf("failed after %d attempts: %w", maxRetries+1, lastErr)
}

func (c *Client) sendOnce(command string, params map[string]any) (map[string]any, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Connect if needed
	if c.conn == nil {
		if err := c.connect(); err != nil {
			return nil, err
		}
	}

	// Build request
	request := map[string]any{
		"type":   command,
		"params": params,
	}
	payloadBytes, err := json.Marshal(request)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal request: %w", err)
	}

	// Send with length-prefix framing
	c.conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	frame := encodeFrame(payloadBytes)
	if _, err := c.conn.Write(frame); err != nil {
		c.close()
		return nil, fmt.Errorf("failed to send command: %w", err)
	}

	// Receive response
	timeout := c.getTimeout(command)
	c.conn.SetReadDeadline(time.Now().Add(timeout))
	raw, err := readFrame(c.conn)
	if err != nil {
		c.close()
		return nil, fmt.Errorf("failed to receive response: %w", err)
	}

	// Parse JSON
	var response map[string]any
	if err := json.Unmarshal(raw, &response); err != nil {
		return nil, fmt.Errorf("invalid JSON response: %w", err)
	}

	// Normalize: if response has success=false but no status, add it
	if success, ok := response["success"]; ok {
		if successBool, ok := success.(bool); ok && !successBool {
			if _, hasStatus := response["status"]; !hasStatus {
				response["status"] = "error"
			}
		}
	}

	return response, nil
}

// Close cleanly shuts down the connection.
func (c *Client) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.close()
}
