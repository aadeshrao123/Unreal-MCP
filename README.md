# UnrealMCP — Model Context Protocol Bridge for Unreal Engine 5

UnrealMCP exposes the Unreal Engine 5 editor to AI coding assistants (Claude Code, Cursor, Windsurf, etc.) through the [Model Context Protocol](https://modelcontextprotocol.io/). Drop it into any UE5 project's `Plugins/` folder and let your AI assistant create materials, blueprints, spawn actors, manage data tables, profile performance, and more — all without leaving your terminal.

## How It Works

UnrealMCP has two halves:

1. **C++ Editor Plugin** (`UnrealMCPBridge` module) — runs inside the UE5 editor as a TCP server. It receives JSON commands and executes them in-editor using Unreal's C++ API.
2. **Python MCP Server** (`MCP/mcp_server.py`) — runs as a stdio process spawned by your AI tool. It translates MCP tool calls into TCP commands sent to the C++ bridge.

```
┌─────────────┐   stdio    ┌──────────────────┐   TCP/JSON    ┌──────────────────┐
│ Claude Code  │ ────────► │  Python MCP       │ ────────────► │  C++ Bridge      │
│ / Cursor     │ ◄──────── │  Server           │ ◄──────────── │  (in UE5 Editor) │
└─────────────┘            └──────────────────┘                └──────────────────┘
```

The C++ bridge writes its TCP port to `Saved/UnrealMCP/port.txt` on startup. The Python server reads that file automatically — no manual port configuration needed.

---

## Requirements

- **Unreal Engine 5** (5.3+ recommended, source build or binary)
- **Python 3.10+**
- **UE5 Plugins** (enabled automatically by the `.uplugin`):
  - `PythonScriptPlugin`
  - `EditorScriptingUtilities`
  - `EnhancedInput`

---

## Quick Install

Run one command from your **Unreal project root** (where your `.uproject` file is):

**Windows:**
```cmd
curl -o install.bat https://raw.githubusercontent.com/aadeshrao123/Unreal-MCP/main/install.bat && install.bat
```

**macOS / Linux:**
```bash
curl -sL https://raw.githubusercontent.com/aadeshrao123/Unreal-MCP/main/install.sh | bash
```

The installer will:
1. Clone the C++ plugin (project or engine-level, your choice)
2. Install `unrealmcp` via pip (global command, works from any project)
3. Set up the MCP config for your AI tool (Claude Code, Cursor, VS Code, Gemini, Windsurf, JetBrains, Zed, Amazon Q)

### Manual Quick Start

If you prefer to install manually:

```bash
# 1. Clone the C++ plugin into your project
git clone https://github.com/aadeshrao123/Unreal-MCP.git Plugins/UnrealMCP

# 2. Install the MCP server globally
pip install unrealmcp

# 3. Add to your MCP config (example for Claude Code .mcp.json):
#    { "mcpServers": { "unreal": { "type": "stdio", "command": "unrealmcp" } } }
```

Since `unrealmcp` is a global pip command, it works regardless of where the C++ plugin is installed (project or engine level).

---

## Manual Installation

### Step 1: Copy the Plugin

Copy the `UnrealMCP/` folder into your project's `Plugins/` directory:

```
YourProject/
├── Content/
├── Source/
├── Plugins/
│   └── UnrealMCP/          ← put it here
│       ├── UnrealMCP.uplugin
│       ├── MCP/
│       └── Source/
└── YourProject.uproject
```

### Step 2: Install Python Dependencies

```bash
pip install fastmcp requests
```

### Step 3: Regenerate Project Files

If you use Visual Studio or Rider, regenerate your project files so the new module is picked up:

```bash
# Windows (adjust path to your engine)
"<EnginePath>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
    -projectfiles \
    -project="<YourProject>/YourProject.uproject" \
    -game -engine
```

### Step 4: Build and Launch the Editor

Build your project and open the editor. You should see this in the Output Log:

```
LogUnrealMCPBridge: MCP Bridge initialized on port 55557
```

This confirms the C++ bridge is running and ready for connections.

### Step 5: Configure Your AI Tool

Each AI tool uses a slightly different config file and format. After installing via `pip install unrealmcp`, pick your tool below.

> **Tip:** Since `unrealmcp` is a global pip command, the same config works on Windows, macOS, and Linux — no platform-specific paths needed.

<details>
<summary><b>Claude Code</b> — <code>.mcp.json</code> (project root)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "type": "stdio",
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
Claude Code is the only tool that requires `"type": "stdio"` explicitly.
</details>

<details>
<summary><b>Cursor</b> — <code>.cursor/mcp.json</code> (project root)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
</details>

<details>
<summary><b>VS Code / Copilot</b> — <code>.vscode/mcp.json</code> (project root)</summary>

```json
{
  "servers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
Note: VS Code uses `"servers"`, not `"mcpServers"`.
</details>

<details>
<summary><b>Windsurf</b> — <code>~/.codeium/windsurf/mcp_config.json</code> (user-level)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
</details>

<details>
<summary><b>Gemini CLI</b> — <code>.gemini/settings.json</code> (project root)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
</details>

<details>
<summary><b>JetBrains / Rider</b> — <code>.junie/mcp/mcp.json</code> (project root)</summary>

```json
{
  "servers": [
    {
      "name": "unreal",
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  ]
}
```
Note: JetBrains uses `"servers"` as an **array** with a `"name"` field. Requires IDE 2025.1+ with AI Assistant.
</details>

<details>
<summary><b>Zed</b> — <code>~/.config/zed/settings.json</code></summary>

Add to your Zed `settings.json`:
```json
{
  "context_servers": {
    "unreal": {
      "source": "custom",
      "command": {
        "path": "unrealmcp",
        "args": [],
        "env": {}
      }
    }
  }
}
```
Note: Zed uses `"context_servers"` with a nested `"command"` object.
</details>

<details>
<summary><b>Amazon Q</b> — <code>.amazonq/mcp.json</code> (project root)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp",
      "args": [],
      "env": {}
    }
  }
}
```
</details>

### Step 6: Verify

Open your AI tool (e.g., Claude Code) in the project directory. The MCP server will auto-connect to the editor. You can verify with:

```
> Use the health_check tool
```

If it returns successfully, you're connected.

---

## Configuration (Optional)

### Custom Port

Add to `Config/DefaultEngine.ini`:

```ini
[UnrealMCP]
Port=55557
```

The bridge scans up to 100 ports from this base if the default is taken (useful when running multiple editor instances).

### Environment Variable Override

```bash
set UNREAL_MCP_PORT=55560
```

Forces the Python server to connect to a specific port instead of reading from `port.txt`.

### Token Debug Mode

Enable to see estimated token usage per command (useful for optimizing MCP calls):

```
> Use set_mcp_debug with enabled=true
> Use get_mcp_token_stats to see per-command statistics
```

---

## Available Tools

UnrealMCP provides **100+ tools** organized by category. Here's a summary of what you can do:

### Core

| Tool | Description |
|------|-------------|
| `health_check` | Verify the bridge is running and responsive |
| `execute_python` | Run arbitrary Python in the editor (set `result` to return data) |

### Asset Management

| Tool | Description |
|------|-------------|
| `find_assets` | Search the Asset Registry by class, path, or name pattern |
| `list_assets` | List assets in a Content Browser directory |
| `get_asset_info` | Get asset metadata (class, package, disk path) |
| `get_asset_properties` | Read all editable properties of any asset |
| `set_asset_property` | Set a single property on any asset |
| `find_references` | Find dependents or dependencies of an asset |
| `import_asset` | Import an external file (FBX, PNG, WAV, etc.) |
| `import_assets_batch` | Batch import from a file list or directory scan |
| `duplicate_asset` | Copy an asset to a new location |
| `rename_asset` | Rename/move an asset (auto-fixes references) |
| `delete_asset` | Delete an asset (checks for references first) |
| `save_asset` / `save_all` | Save one or all dirty assets |
| `open_asset` | Open an asset in the editor |
| `sync_browser` | Navigate Content Browser to an asset |
| `get_selected_assets` | Get currently selected Content Browser assets |

### Blueprints

| Tool | Description |
|------|-------------|
| `search_parent_classes` | Find valid parent classes for Blueprint creation |
| `create_blueprint` | Create a Blueprint from any C++ or BP parent class |
| `compile_blueprint` | Compile a Blueprint |
| `read_blueprint_content` | Read full BP structure: graphs, variables, components |
| `add_component_to_blueprint` | Add a component (StaticMesh, Box Collision, etc.) |
| `create_blueprint_variable` | Create a variable in a Blueprint |
| `get/set_blueprint_class_defaults` | Read/write Class Default Object (CDO) properties |
| `get/set_blueprint_variable_properties` | Inspect/modify variable settings |
| `create/delete/rename_blueprint_function` | Manage Blueprint functions |
| `add_function_input` / `add_function_output` | Add parameters to functions |
| `get_blueprint_function_details` | Inspect function signatures and graphs |

### Blueprint Graph

| Tool | Description |
|------|-------------|
| `add_blueprint_node` | Add a node (Branch, ForLoop, CallFunction, Print, etc.) |
| `add_event_node` | Add an event (BeginPlay, Tick, ActorBeginOverlap, etc.) |
| `connect_blueprint_nodes` | Wire two nodes together (execution or data pins) |
| `delete_blueprint_node` | Remove a node by ID |
| `set_blueprint_node_property` | Edit node properties (add pins, set enum, etc.) |
| `analyze_blueprint_graph` | Get structural analysis of a graph |

### Materials

| Tool | Description |
|------|-------------|
| `create_material` | Create a new material with blend/shading mode |
| `create_material_instance` | Create a Material Instance with parameter overrides |
| `build_material_graph` | Build a complete node graph atomically (nodes + connections) |
| `get_material_info` | Inspect material properties, parameters, textures |
| `set_material_properties` | Set blend mode, shading model, two-sided, etc. |
| `add/delete/move/duplicate_material_expression` | Manage expression nodes |
| `connect_material_expressions` | Wire expression outputs to inputs |
| `set_material_expression_property` | Set properties on expression nodes |
| `layout_material_expressions` | Auto-layout expression nodes |
| `recompile_material` | Force recompile and save |
| `get/set_material_instance_parameter` | Read/write MI parameter overrides |
| `list_material_expression_types` | Discover available expression types |
| `add_material_comments` | Add comment boxes to the material graph |
| `apply_material_to_actor` / `apply_material_to_blueprint` | Apply materials |
| `get_available_materials` | List materials in project or engine |

### Data Tables

| Tool | Description |
|------|-------------|
| `get_data_table_schema` | Get column names and types from the row struct |
| `get_data_table_rows` / `get_data_table_row` | Read all or one row |
| `add_data_table_row` | Add a row with optional initial data |
| `update_data_table_row` | Partial update of row fields |
| `delete_data_table_row` | Delete a row |
| `duplicate_data_table_row` | Copy a row under a new name |
| `rename_data_table_row` | Rename a row in-place |

### Data Assets

| Tool | Description |
|------|-------------|
| `create_data_asset` | Create any UDataAsset subclass with initial properties |
| `get_data_asset_properties` | Read all properties (with optional filter) |
| `set_data_asset_property` / `set_data_asset_properties` | Set one or many properties |
| `list_data_assets` | Browse by path or class |
| `list_data_asset_classes` | List all loaded UDataAsset subclasses |
| `get_property_valid_types` | Query valid dropdown/enum values for a property |

### Actors & Level

| Tool | Description |
|------|-------------|
| `spawn_actor` | Spawn an actor (StaticMeshActor, PointLight, etc.) |
| `spawn_blueprint_actor` | Spawn a Blueprint actor instance in the level |
| `get_actors_in_level` | List all actors in the current level |
| `find_actors_by_name` | Find actors by name pattern |
| `get_actor_properties` | Read all properties from a placed actor |
| `set_actor_transform` | Set location, rotation, and/or scale |
| `delete_actor` | Delete an actor from the level |
| `get_selected_actors` | Get currently selected viewport actors |
| `get_world_info` | Get level name, actor count, etc. |
| `set_static_mesh_properties` | Set mesh on a component |
| `set_physics_properties` | Configure physics simulation |
| `set_mesh_material_color` | Set material color on a mesh component |
| `take_screenshot` | Capture viewport or editor window to PNG |

### Enhanced Input

| Tool | Description |
|------|-------------|
| `create_input_action` | Create a UInputAction (Boolean/Axis1D/2D/3D) |
| `get/set_input_action_properties` | Read/write action properties |
| `add/remove_input_action_trigger` | Manage triggers (Hold, Pressed, Tap, etc.) |
| `add/remove_input_action_modifier` | Manage modifiers (DeadZone, Negate, Scalar, etc.) |
| `create_input_mapping_context` | Create a UInputMappingContext |
| `get_input_mapping_context` | Read all key mappings |
| `add/remove/set_key_mapping` | Manage key-to-action mappings |
| `add/remove_mapping_trigger` / `add/remove_mapping_modifier` | Per-mapping overrides |
| `list_input_actions` / `list_input_mapping_contexts` | Browse input assets |
| `list_trigger_types` / `list_modifier_types` / `list_input_keys` | Discover available types |

### Widgets (UMG)

| Tool | Description |
|------|-------------|
| `get_widget_tree` | Read full widget hierarchy as JSON |
| `add_widget` | Add a widget to a parent slot |
| `remove_widget` / `move_widget` / `rename_widget` / `duplicate_widget` | Manage widgets |
| `get/set_widget_properties` | Read/write widget properties |
| `get/set_slot_properties` | Read/write layout slot properties |
| `list_widget_types` | Discover available UWidget classes |

### Performance Profiling

| Tool | Description |
|------|-------------|
| `performance_start_trace` | Start recording a live `.utrace` (CPU, GPU, frame, etc.) |
| `performance_stop_trace` | Stop recording and auto-load for analysis |
| `performance_analyze_insight` | Query loaded traces with smart analysis |

**Smart analysis queries:**
- `diagnose` — full automated report with severity findings
- `spikes` — worst frames with category breakdown
- `flame` — top timers by exclusive (self) time
- `bottlenecks` — auto-categorize frame into Animation/Slate/Network/etc.
- `hotpath` — drill into a category's children by time
- `compare` — compare a frame vs trace median
- `search` — find a timer across all frames (min/avg/max/p95/p99)
- `histogram` — frame time distribution

**Standard queries:** `summary`, `worst_frames`, `frame_details`, `timer_stats`, `butterfly`, `threads`, `counters`

**Provider queries** (require specific trace channels): `net_stats`, `loading`, `logs`, `memory`, `regions`, `bookmarks`, `session`, `modules`, `file_io`, `tasks`, `allocations`, `context_switches`, `screenshots`, `stack_samples`

### Debug

| Tool | Description |
|------|-------------|
| `set_mcp_debug` | Enable/disable token estimation debug headers |
| `get_mcp_token_stats` | Per-command token usage statistics |

---

## Usage Examples

### Create a Material

```
Create a red metallic material at /Game/Materials/M_RedMetal
with roughness 0.3 and metallic 1.0
```

The AI will call `create_material`, then `build_material_graph` to wire up constant nodes to Base Color, Metallic, and Roughness pins.

### Spawn Actors

```
Spawn 5 point lights in a circle around the origin at height 300
```

The AI will call `spawn_actor` with `actor_type: "PointLight"` five times with calculated positions.

### Blueprint Creation

```
Create a Blueprint actor called BP_HealthPickup based on Actor,
add a Sphere Collision component and a Static Mesh component,
set it up so on BeginOverlap it prints "Health Picked Up"
```

The AI will use `create_blueprint`, `add_component_to_blueprint`, `add_event_node`, `add_blueprint_node`, and `connect_blueprint_nodes`.

### Profile Performance

```
Start a performance trace, play for 10 seconds, stop it,
then diagnose the bottlenecks
```

The AI will call `performance_start_trace`, wait, `performance_stop_trace`, then `performance_analyze_insight` with `query: "diagnose"`.

### Data Table Management

```
Show me the schema of DT_Items, then add a new row called
"IronIngot" with StackSize 100
```

The AI will call `get_data_table_schema`, then `add_data_table_row` with the appropriate data.

---

## Architecture

### Plugin Structure

```
Plugins/UnrealMCP/
├── UnrealMCP.uplugin                  # Plugin manifest
├── MCP/                               # Python MCP server
│   ├── mcp_server.py                  # Entry point (stdio transport)
│   ├── _bridge.py                     # Shared FastMCP instance
│   ├── _tcp_bridge.py                 # TCP communication layer
│   └── tools/                         # Tool modules (one per category)
│       ├── __init__.py
│       ├── core.py                    # health_check, execute_python
│       ├── assets.py                  # Asset management
│       ├── blueprints.py              # Blueprint creation & components
│       ├── blueprint_graph.py         # Graph node manipulation
│       ├── materials.py               # Material creation & graphs
│       ├── editor_commands.py         # Actors, physics, screenshots
│       ├── data_tables.py             # Data Table CRUD
│       ├── data_assets.py             # Data Asset CRUD
│       ├── widgets.py                 # Widget tree manipulation
│       ├── enhanced_input.py          # Input Actions & Mapping Contexts
│       ├── level.py                   # Level info & actor selection
│       ├── profiling.py              # Performance tracing & analysis
│       └── debug.py                   # Token stats & debug mode
└── Source/UnrealMCPBridge/
    ├── Public/
    │   ├── EpicUnrealMCPBridge.h      # Main subsystem (UEditorSubsystem)
    │   ├── MCPServerRunnable.h        # TCP server thread
    │   └── Commands/                  # Command handler headers
    └── Private/
        ├── EpicUnrealMCPBridge.cpp    # Subsystem implementation
        ├── EpicUnrealMCPModule.cpp    # Module startup
        ├── MCPServerRunnable.cpp      # Server thread implementation
        └── Commands/                  # Command handler implementations
```

### Communication Protocol

The C++ bridge and Python server communicate over TCP using **length-prefix framing**:

```
[4 bytes: big-endian payload length] [N bytes: UTF-8 JSON]
```

**Request format:**
```json
{
  "type": "create_material",
  "params": {
    "name": "M_Example",
    "path": "/Game/Materials"
  }
}
```

**Response format:**
```json
{
  "status": "success",
  "result": { ... }
}
```

### Port Discovery

1. C++ bridge starts TCP listener on configured port (default `55557`)
2. Writes the actual port number to `<ProjectDir>/Saved/UnrealMCP/port.txt`
3. Python server reads that file on connection
4. If the default port is taken, the bridge scans up to 100 ports

This means multiple editor instances can run simultaneously without conflicts.

---

## Adding Custom Tools

### Python Side

Create a new file in `MCP/tools/`:

```python
# MCP/tools/my_tools.py
from _bridge import mcp
from _tcp_bridge import _call

@mcp.tool()
def my_custom_tool(param1: str, param2: int = 10) -> str:
    """Description shown to the AI assistant."""
    return _call("my_custom_command", {
        "param1": param1,
        "param2": param2,
    })
```

Register it in `MCP/tools/__init__.py`:

```python
from tools import my_tools  # noqa: F401
```

### C++ Side

1. Create a command handler class in `Source/UnrealMCPBridge/Private/Commands/`
2. Implement the command routing logic
3. Register the handler in `UEpicUnrealMCPBridge::ExecuteCommand()`

The C++ handler receives the `params` JSON object and returns a JSON result. All editor operations should be executed on the game thread (use `AsyncTask(ENamedThreads::GameThread, ...)` if called from the TCP thread).

---

## Troubleshooting

### "Connection refused" or tools not working

1. Make sure the UE5 editor is running
2. Check the Output Log for `LogUnrealMCPBridge: MCP Bridge initialized on port XXXXX`
3. Verify `Saved/UnrealMCP/port.txt` exists and contains a port number
4. Check that `PythonScriptPlugin` is enabled in your `.uproject`

### Python import errors

```bash
pip install fastmcp requests
```

Make sure the Python on your PATH is the same one your AI tool will invoke.

### Multiple editor instances

Each editor instance picks a unique port automatically. The `.mcp.json` config connects to whichever editor wrote `port.txt` most recently. If you need to target a specific instance, set the `UNREAL_MCP_PORT` environment variable.

### Plugin not loading

- Verify `UnrealMCP.uplugin` has `"Type": "Editor"` in its module definition
- Check that `PythonScriptPlugin` and `EditorScriptingUtilities` are available in your engine build
- Look for errors in the Output Log at editor startup

### Commands timing out

Large operations (profiling analysis, complex Blueprint graphs) have a 300-second timeout. If you hit it, break the operation into smaller steps.

---

## Supported AI Tools

UnrealMCP works with any tool that supports the [Model Context Protocol](https://modelcontextprotocol.io/):

| Tool | Config File | Status |
|------|------------|--------|
| **Claude Code** | `.mcp.json` | Tested |
| **Cursor** | `.cursor/mcp.json` | Tested |
| **VS Code / Copilot** | `.vscode/mcp.json` | Supported |
| **Windsurf** | `~/.codeium/windsurf/mcp_config.json` | Supported |
| **Gemini CLI** | `.gemini/settings.json` | Supported |
| **JetBrains / Rider** | `.junie/mcp/mcp.json` | Supported |
| **Zed** | `~/.config/zed/settings.json` | Supported |
| **Amazon Q** | `.amazonq/mcp.json` | Supported |

See [Step 5: Configure Your AI Tool](#step-5-configure-your-ai-tool) for the exact config format for each tool.

---

## Contributing

Contributions are welcome! Whether it's bug fixes, new tools, documentation, or feature ideas — feel free to get involved.

- **Found a bug?** [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new)
- **Have a feature idea?** [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new) and describe what you'd like
- **Want to contribute code?** [Fork the repo](https://github.com/aadeshrao123/Unreal-MCP/fork), make your changes, and open a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines on submitting PRs and adding new tools.

---

## License

MIT — see [LICENSE](LICENSE) for details.
