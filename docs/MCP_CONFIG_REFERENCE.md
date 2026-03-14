# MCP Server Configuration Reference

Exact config file paths and JSON formats for adding a stdio MCP server to each AI coding tool.

---

## 1. Claude Code

**Config paths:**
- Project: `.mcp.json` (repo root)
- User: `~/.claude.json`

**Root key:** `"mcpServers"`

```json
{
  "mcpServers": {
    "my-server": {
      "type": "stdio",
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "${API_KEY}"
      }
    }
  }
}
```

**Notes:**
- `type` field required (`"stdio"` or `"sse"`)
- Supports `${VAR}` and `${VAR:-default}` env var expansion
- On Windows (non-WSL), npx-based servers need `"command": "cmd"` with `"args": ["/c", "npx", ...]`
- Timeout via `MCP_TIMEOUT` env var (ms)

---

## 2. Cursor

**Config paths:**
- Project: `.cursor/mcp.json`
- User: `~/.cursor/mcp.json`

**Root key:** `"mcpServers"`

```json
{
  "mcpServers": {
    "my-server": {
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "value"
      }
    }
  }
}
```

**Notes:**
- No `type` field needed for stdio (it is the default)
- Missing `"mcpServers"` root key silently fails with no warning
- Supports `env` for environment variables
- Also supports `"url"` for SSE transport

---

## 3. Windsurf (Codeium)

**Config path:**
- User: `~/.codeium/windsurf/mcp_config.json`
- Windows: `%USERPROFILE%\.codeium\windsurf\mcp_config.json`

**Root key:** `"mcpServers"`

```json
{
  "mcpServers": {
    "my-server": {
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "value"
      },
      "disabled": false,
      "alwaysAllow": ["tool_name"]
    }
  }
}
```

**Notes:**
- No `type` field for stdio
- Extra fields: `disabled` (bool) and `alwaysAllow` (string array of auto-approved tools)
- Supports env var interpolation in `command`, `args`, `env`
- Also supports Streamable HTTP and SSE via `"serverUrl"` or `"url"`
- No project-level config file; user-level only

---

## 4. VS Code (GitHub Copilot)

**Config paths:**
- Project: `.vscode/mcp.json`
- User: VS Code user settings (settings.json under `"mcp"` key)

**Root key:** `"servers"` (NOT `"mcpServers"`)

```json
{
  "servers": {
    "my-server": {
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "${input:my_api_key}"
      }
    }
  },
  "inputs": [
    {
      "type": "promptString",
      "id": "my_api_key",
      "description": "API Key",
      "password": true
    }
  ]
}
```

**Notes:**
- Root key is `"servers"`, not `"mcpServers"` -- unique among all tools
- No `type` field needed for stdio (presence of `command` implies stdio)
- For HTTP: `"type": "http"` with `"url"` field
- Supports `"inputs"` array for secure credential prompting via `${input:id}`
- Has IntelliSense support in the editor

---

## 5. Google Gemini CLI

**Config paths:**
- User: `~/.gemini/settings.json`
- Project: `.gemini/settings.json` (repo root)

**Root key:** `"mcpServers"`

```json
{
  "mcpServers": {
    "my-server": {
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "value"
      }
    }
  }
}
```

**Notes:**
- No `type` field for stdio
- For HTTP: use `"httpUrl"` and `"headers"` instead of `command`/`args`
- Env vars auto-expanded from shell environment
- Extra global settings under `"mcp"` key: `"mcp.allowed"` (whitelist), `"mcp.excluded"` (blacklist)
- Disabled server state tracked separately in `~/.gemini/mcp-server-enablement.json`
- Config lives inside `settings.json` alongside other Gemini CLI settings (not a standalone file)

---

## 6. JetBrains IDEs (Rider, IntelliJ, etc.)

**Config paths:**
- User (global):
  - macOS: `~/Library/Application Support/JetBrains/<Product><Version>/mcp.json`
  - Linux: `~/.config/JetBrains/<Product><Version>/mcp.json`
  - Windows: `%APPDATA%\JetBrains\<Product><Version>\mcp.json`
- Project: `.junie/mcp/mcp.json` (at project root)

**Root key:** `"servers"` (array, NOT object)

```json
{
  "servers": [
    {
      "name": "my-server",
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "value"
      }
    }
  ]
}
```

**Notes:**
- Uses `"servers"` as an ARRAY of objects (unique among all tools)
- Each entry has an explicit `"name"` field
- Requires JetBrains IDE 2025.1+ with AI Assistant plugin
- Can also configure via UI: Settings > Tools > MCP Server
- `IDE_PORT` and `HOST` env vars to target a specific IDE instance

---

## 7. Zed Editor

**Config path:**
- User settings: `~/.config/zed/settings.json` (Linux/macOS)
- Windows: `%APPDATA%\Zed\settings.json`

**Root key:** `"context_servers"` (NOT `"mcpServers"`)

```json
{
  "context_servers": {
    "my-server": {
      "source": "custom",
      "command": {
        "path": "npx",
        "args": ["-y", "my-mcp-server"],
        "env": {
          "API_KEY": "value"
        }
      }
    }
  }
}
```

**Notes:**
- Root key is `"context_servers"` -- unique among all tools
- `"source": "custom"` required for manually configured servers
- `command` can be a string OR an object with `path`, `args`, `env`
- Config lives inside `settings.json` alongside other Zed settings (not a standalone file)
- Auto-restarts server on settings save

---

## 8. Amazon Q Developer

**Config paths:**
- User (global): `~/.aws/amazonq/mcp.json`
- Project: `.amazonq/mcp.json` (repo root)

**Root key:** `"mcpServers"`

```json
{
  "mcpServers": {
    "my-server": {
      "command": "npx",
      "args": ["-y", "my-mcp-server"],
      "env": {
        "API_KEY": "value"
      },
      "timeout": 60000
    }
  }
}
```

**Notes:**
- No `type` field for stdio
- Extra field: `timeout` (ms, default 60000)
- For HTTP: `"type": "http"` with `"url"` field
- Env values must be strings

---

## Quick Comparison

| Tool | Config File | Root Key | Server ID | Stdio Type Field | Transport |
|---|---|---|---|---|---|
| Claude Code | `.mcp.json` | `mcpServers` | object key | `"type": "stdio"` required | stdio, sse |
| Cursor | `.cursor/mcp.json` | `mcpServers` | object key | not needed | stdio, sse |
| Windsurf | `~/.codeium/windsurf/mcp_config.json` | `mcpServers` | object key | not needed | stdio, http, sse |
| VS Code | `.vscode/mcp.json` | `servers` | object key | not needed | stdio, http |
| Gemini CLI | `.gemini/settings.json` | `mcpServers` | object key | not needed | stdio, http |
| JetBrains | `mcp.json` (varies) | `servers` (array) | `"name"` field | not needed | stdio |
| Zed | `settings.json` | `context_servers` | object key | not needed | stdio |
| Amazon Q | `.amazonq/mcp.json` | `mcpServers` | object key | not needed | stdio, http |

### Key Schema Differences
1. **VS Code** uses `"servers"` (object) instead of `"mcpServers"`
2. **JetBrains** uses `"servers"` as an **array** with explicit `"name"` fields
3. **Zed** uses `"context_servers"` with `"source": "custom"` and nested `command.path`
4. **Claude Code** is the only one requiring `"type": "stdio"` explicitly
5. **Windsurf** adds `disabled` and `alwaysAllow` fields
6. **Amazon Q** adds `timeout` field
7. **VS Code** has `"inputs"` array for secure credential prompting
