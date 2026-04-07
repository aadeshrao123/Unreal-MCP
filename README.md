# UnrealMCP — AI Bridge for Unreal Engine 5

Control Unreal Engine 5 editor from AI coding assistants (Claude Code, Cursor, Windsurf, etc.). Create materials, blueprints, Niagara VFX, StateTrees, spawn actors, manage data tables, profile performance, and more — **238 commands** across 13 categories, all without leaving your terminal.

**Two ways to use it:**

| | CLI (new) | MCP Server |
|---|---|---|
| **Install** | `npm install -g unrealcli` | `pip install unrealmcp` |
| **Dependencies** | None (single binary) | Python 3.10+ |
| **Works with** | Claude Code (via Bash) | Claude Code, Cursor, Windsurf, VS Code, Gemini CLI, Rider, Zed, Amazon Q |
| **Protocol** | Direct TCP | MCP over stdio |

Both talk to the same C++ plugin inside the editor. Use whichever fits your workflow — or both.

---

## Quick Start

### Option A: CLI (Recommended for Claude Code)

```bash
# 1. Install
npm install -g unrealcli

# 2. Go to any UE5 project and install the plugin
cd YourProject/
ue-cli init

# 3. Open the editor, then verify
ue-cli health_check
```

That's it. No Python, no config files, no MCP setup.

### Option B: MCP Server (For Cursor, Windsurf, etc.)

```bash
# 1. Clone the plugin into your project
git clone https://github.com/aadeshrao123/Unreal-MCP.git Plugins/UnrealMCP

# 2. Install the Python MCP server
pip install unrealmcp

# 3. Add to your AI tool's config (see MCP Setup section below)
```

---

## How It Works

```
                    ┌──────────────────────────────────────────┐
                    │         Unreal Engine 5 Editor            │
                    │                                          │
                    │   C++ Plugin (UnrealMCPBridge)           │
                    │   TCP server on localhost:55557           │
                    │   238 commands: materials, blueprints,   │
                    │   niagara, statetree, actors, data       │
                    │   tables, profiling, and more            │
                    └──────────────┬───────────────────────────┘
                                   │ TCP/JSON
                    ┌──────────────┴───────────────────────────┐
                    │                                          │
          ┌─────────┴─────────┐              ┌─────────┴──────────┐
          │   CLI (ue-cli)    │              │  MCP Server        │
          │   Go binary       │              │  Python (unrealmcp)│
          │   Direct TCP      │              │  stdio → TCP       │
          │                   │              │                    │
          │  Claude Code      │              │  Cursor, Windsurf, │
          │  (via Bash tool)  │              │  VS Code, Rider... │
          └───────────────────┘              └────────────────────┘
```

The C++ plugin runs inside the editor and exposes 238 commands over TCP. The CLI and MCP server are two different front doors to the same plugin.

---

## CLI Reference

### Installation

```bash
# From npm (recommended)
npm install -g unrealcli

# Or download binary directly from GitHub Releases
# https://github.com/aadeshrao123/Unreal-MCP/releases
```

**Platforms:** Windows (x64), macOS (Intel + Apple Silicon), Linux (x64 + ARM64)

### Setup

```bash
# Navigate to your UE5 project
cd MyProject/

# Install the C++ plugin (one-time)
ue-cli init
# → Creates Plugins/UnrealMCP/ with C++ source
# → Patches .uproject to enable the plugin
# → Open editor to compile, then you're ready

# Verify everything works
ue-cli doctor
```

### Usage

```bash
# Every command follows this pattern:
ue-cli <command> [--flag value]

# Examples:
ue-cli health_check
ue-cli find_assets --class-type material --path /Game
ue-cli spawn_actor --name MyCube --type StaticMeshActor --location "[0,0,100]"
ue-cli get_data_table_rows --data-table-path /Game/Data/DT_Items
ue-cli save_all

# For complex params, use --json:
ue-cli build_material_graph --json '{"material_path":"/Game/M_Test","nodes":[...],"connections":[...]}'

# Or pipe from stdin:
echo '{"material_path":"/Game/M_Test","nodes":[...]}' | ue-cli build_material_graph --json -
```

### Help & Discovery

```bash
# See all commands grouped by category
ue-cli --help

# See flags for a specific command
ue-cli find_assets --help

# Dump all commands with descriptions, flags, and examples (for AI assistants)
ue-cli list_commands
```

### Global Flags

| Flag | Description |
|------|-------------|
| `--port <int>` | TCP port override (default: auto-discover from port file) |
| `--timeout <int>` | Timeout in seconds (default: 30, large ops: 300) |
| `--json <string>` | Full params as JSON string (use `-` for stdin) |
| `--version` | Print version |

### Key Commands

#### Assets
```bash
ue-cli find_assets --class-type material --path /Game/Materials
ue-cli list_assets --path /Game --class-filter blueprint
ue-cli get_asset_properties --asset-path /Game/Materials/M_Base
ue-cli import_asset --source-file "C:/Art/texture.png" --destination-path /Game/Textures
ue-cli save_asset --asset-path /Game/Materials/M_Base
ue-cli save_all
```

#### Blueprints
```bash
ue-cli create_blueprint --name BP_MyActor --parent-class Actor
ue-cli add_component_to_blueprint --blueprint-path /Game/BP_MyActor --component-class StaticMeshComponent
ue-cli add_event_node --blueprint-name BP_MyActor --event-name BeginPlay
ue-cli compile_blueprint --blueprint-name BP_MyActor
ue-cli read_blueprint_content --blueprint-path /Game/BP_MyActor
```

#### Materials
```bash
ue-cli create_material --name M_Red --path /Game/Materials
ue-cli build_material_graph --material-path /Game/Materials/M_Red \
  --nodes '[{"type":"Constant3Vector","pos_x":-400,"properties":{"Constant":"(R=1,G=0,B=0)"}}]' \
  --connections '[{"from_node":0,"to_node":"material","to_pin":"BaseColor"}]'
ue-cli create_material_instance --parent-path /Game/Materials/M_Base --name MI_Red
```

#### Data Tables
```bash
ue-cli get_data_table_schema --data-table-path /Game/Data/DT_Items
ue-cli get_data_table_rows --data-table-path /Game/Data/DT_Items
ue-cli add_data_table_row --data-table-path /Game/Data/DT_Items --row-name CopperOre \
  --data '{"DisplayName":"Copper Ore","StackSize":100}'
ue-cli update_data_table_row --data-table-path /Game/Data/DT_Items --row-name CopperOre \
  --data '{"StackSize":200}'
```

#### Actors & Level
```bash
ue-cli get_actors_in_level
ue-cli spawn_actor --name MyCube --type StaticMeshActor --location "[0,0,100]"
ue-cli spawn_blueprint_actor --blueprint-path /Game/BP_MyActor --location "[500,0,0]"
ue-cli find_actors_by_name --pattern "Light"
ue-cli get_world_info
ue-cli take_screenshot
```

#### Performance Profiling
```bash
# Record → Stop → Analyze
ue-cli performance_start_trace --channels "cpu,gpu,frame"
# ... play the game ...
ue-cli performance_stop_trace
ue-cli performance_analyze_insight --query diagnose
ue-cli performance_analyze_insight --query flame --count 20
ue-cli performance_analyze_insight --query search --filter "ConveyorProcessor"
```

#### Enhanced Input
```bash
ue-cli create_input_action --asset-path /Game/Input/IA_Jump --value-type Boolean
ue-cli create_input_mapping_context --asset-path /Game/Input/IMC_Default
ue-cli add_key_mapping --context-path /Game/Input/IMC_Default --action-path /Game/Input/IA_Jump --key SpaceBar
```

#### Niagara VFX
```bash
# Create a system from an emitter template, then tweak it
ue-cli create_niagara_system --asset-path /Game/VFX/NS_Sparks \
  --template "/Niagara/DefaultAssets/FX_Sparks.FX_Sparks"
ue-cli get_niagara_system_info --asset-path /Game/VFX/NS_Sparks
ue-cli set_niagara_module_input --asset-path /Game/VFX/NS_Sparks \
  --emitter-name Sparks --stack SpawnStack \
  --module-name "Spawn Rate" --input-name SpawnRate --value 250
ue-cli compile_niagara_system --asset-path /Game/VFX/NS_Sparks

# Spawn it in the level
ue-cli spawn_niagara_effect --asset-path /Game/VFX/NS_Sparks --location "[0,0,200]"
```

#### StateTree
```bash
# Create a StateTree, add a state with a task, compile
ue-cli create_statetree --asset-path /Game/AI/ST_Enemy
ue-cli add_statetree_state --asset-path /Game/AI/ST_Enemy --state-name Patrol
ue-cli add_statetree_task --asset-path /Game/AI/ST_Enemy \
  --state-name Patrol --task-type "MassEnemyNestPatrolTask"
ue-cli add_statetree_transition --asset-path /Game/AI/ST_Enemy \
  --from-state Patrol --trigger OnEvent --event-tag "Enemy.SeePlayer"
ue-cli compile_statetree --asset-path /Game/AI/ST_Enemy
```

#### Mass Config Traits (Surgical Editing)
```bash
# Modify a single trait property without touching the rest of the trait array
ue-cli get_mass_config_traits --asset-path /Game/Mass/Enemy_Config
ue-cli set_mass_config_trait_property --asset-path /Game/Mass/Enemy_Config \
  --trait-class MassMovementTrait --property-name MaxSpeed --property-value 600
```

#### Widgets (UMG)
```bash
ue-cli get_widget_tree --widget-blueprint-path /Game/UI/WBP_HUD
ue-cli add_widget --widget-blueprint-path /Game/UI/WBP_HUD --widget-class TextBlock \
  --parent-widget-name RootCanvas --widget-name TitleText \
  --widget-properties '{"Text":"Hello World"}'
```

### Diagnostics

```bash
# Check entire setup
ue-cli doctor

# Output:
#   [ok] Project            MyProject/MyProject.uproject
#   [ok] Plugin directory   Plugins/UnrealMCP/ exists
#   [ok] Plugin source      Source/UnrealMCPBridge/ exists
#   [ok] UProject entry     UnrealMCP plugin listed and enabled
#   [ok] Port file          port 55557
#   [ok] TCP connection     Connected to 127.0.0.1:55557
#   [ok] Health check       Bridge is responsive
#   All checks passed. ue-cli is ready to use.
```

---

## MCP Server Setup

For AI tools that use the Model Context Protocol (Cursor, Windsurf, VS Code, etc.).

### Requirements

- **Unreal Engine 5.7** (tested on 5.7, may work on earlier 5.x versions)
- **Python 3.10+**
- **UE5 Plugins** (enabled automatically by the `.uplugin`):
  - `PythonScriptPlugin`
  - `EditorScriptingUtilities`
  - `EnhancedInput`

### Install

```bash
pip install unrealmcp
```

### Setup Script (Alternative to Manual Config)

The setup script installs the `unrealmcp` pip package and creates the MCP config for your AI tool automatically.

**Windows:**
```cmd
cd Plugins\UnrealMCP
install.bat
```

**macOS / Linux:**
```bash
cd Plugins/UnrealMCP
bash install.sh
```

The script asks where to create the MCP config:

| Scope | What it does | When to use |
|-------|-------------|-------------|
| **Project** | Creates config next to your `.uproject` | Only want UnrealMCP in this project |
| **Global** | Creates config in your user folder | Want UnrealMCP in all projects |

### Configure Your AI Tool

<details>
<summary><b>Claude Code</b> — <code>.mcp.json</code> (project root)</summary>

```json
{
  "mcpServers": {
    "unreal": {
      "type": "stdio",
      "command": "unrealmcp"
    }
  }
}
```
</details>

<details>
<summary><b>Cursor</b> — <code>.cursor/mcp.json</code></summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp"
    }
  }
}
```
</details>

<details>
<summary><b>VS Code / Copilot</b> — <code>.vscode/mcp.json</code></summary>

```json
{
  "servers": {
    "unreal": {
      "command": "unrealmcp"
    }
  }
}
```
</details>

<details>
<summary><b>Windsurf</b> — <code>~/.codeium/windsurf/mcp_config.json</code></summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp"
    }
  }
}
```
</details>

<details>
<summary><b>Gemini CLI</b> — <code>.gemini/settings.json</code></summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp"
    }
  }
}
```
</details>

<details>
<summary><b>JetBrains / Rider</b> — <code>.junie/mcp/mcp.json</code></summary>

```json
{
  "servers": [
    {
      "name": "unreal",
      "command": "unrealmcp"
    }
  ]
}
```
</details>

<details>
<summary><b>Zed</b> — <code>~/.config/zed/settings.json</code></summary>

```json
{
  "context_servers": {
    "unreal": {
      "source": "custom",
      "command": { "path": "unrealmcp" }
    }
  }
}
```
</details>

<details>
<summary><b>Amazon Q</b> — <code>.amazonq/mcp.json</code></summary>

```json
{
  "mcpServers": {
    "unreal": {
      "command": "unrealmcp"
    }
  }
}
```
</details>

---

## All 238 Commands

| Category | Count | Highlights |
|----------|------:|-----------|
| [Core](#core-2) | 2 | health_check, execute_python |
| [Asset Management](#asset-management-16) | 16 | find/list/import/duplicate/rename/delete/save |
| [Blueprints](#blueprints-22) | 22 | create, compile, variables, functions, graph nodes |
| [Materials](#materials-35) | 35 | create, build_material_graph, material functions, Substrate |
| [Data Tables](#data-tables-8) | 8 | full CRUD on rows + schema introspection |
| [Data Assets](#data-assets-12) | 12 | data assets + **surgical Mass Config trait editing** |
| [Actors & Level](#actors--level-19) | 19 | spawn, transform, properties, screenshot |
| [Enhanced Input](#enhanced-input-21) | 21 | actions, mapping contexts, triggers, modifiers |
| [Widgets — UMG](#widgets--umg-11) | 11 | widget tree, add/move/rename, slot props |
| [**Niagara VFX**](#niagara-vfx-54-new) | **54** | systems, emitters, modules, renderers, scratch pad, parameters |
| [**StateTree**](#statetree-33-new) | **33** | states, tasks, evaluators, transitions, conditions, bindings |
| [Performance Profiling](#performance-profiling-3) | 3 | record .utrace + smart analysis (diagnose/spikes/flame) |
| [Debug](#debug-2) | 2 | token tracking, debug toggle |

### Core (2)
| Command | Description |
|---------|-------------|
| `health_check` | Verify the bridge is running |
| `execute_python` | Run arbitrary Python in the editor |

### Asset Management (16)
| Command | Description |
|---------|-------------|
| `find_assets` | Search Asset Registry by class/path/name |
| `list_assets` | List assets in a directory |
| `get_asset_info` | Asset metadata |
| `get_asset_properties` | All editable properties |
| `set_asset_property` | Set a property |
| `find_references` | Find dependents/dependencies |
| `import_asset` | Import external file (PNG, FBX, etc.) |
| `import_assets_batch` | Batch import |
| `duplicate_asset` | Copy to new location |
| `rename_asset` | Rename/move (auto-fix references) |
| `delete_asset` | Delete (checks references) |
| `save_asset` / `save_all` | Save dirty assets |
| `open_asset` | Open in editor |
| `sync_browser` | Navigate Content Browser |
| `get_selected_assets` | Currently selected assets |

### Blueprints (22)
| Command | Description |
|---------|-------------|
| `search_parent_classes` | Find valid parent classes |
| `create_blueprint` | Create from any parent class |
| `compile_blueprint` | Compile |
| `read_blueprint_content` | Full structure readout |
| `analyze_blueprint_graph` | Graph analysis |
| `add_component_to_blueprint` | Add component |
| `create/get/set_blueprint_variable` | Variable management |
| `set_blueprint_variable_properties` | Modify variable settings |
| `create/delete/rename_blueprint_function` | Function management |
| `add_function_input/output` | Function parameters |
| `get_blueprint_function_details` | Function inspection |
| `get/set_blueprint_class_defaults` | CDO properties |
| `add_blueprint_node` | Add graph node (23+ types) |
| `add_event_node` | Add event (BeginPlay, Tick, etc.) |
| `connect_blueprint_nodes` | Wire nodes together |
| `delete_blueprint_node` | Remove node |
| `set_blueprint_node_property` | Edit node properties |

### Materials (35)
| Command | Description |
|---------|-------------|
| `create_material` | Create with blend/shading mode |
| `create_material_instance` | Create with parameter overrides |
| `build_material_graph` | Build complete node graph atomically |
| `get_material_info` | Inspect properties/params/textures |
| `set_material_properties` | Bulk-set material properties |
| `add/delete/move/duplicate_material_expression` | Manage nodes |
| `connect_material_expressions` | Wire nodes |
| `set_material_expression_property` | Set node property |
| `disconnect_material_expression` | Break connection |
| `layout_material_expressions` | Auto-layout |
| `recompile_material` | Force recompile |
| `get_material_errors` | Compilation errors |
| `get/set_material_instance_parameter` | MI parameter overrides |
| `list_material_expression_types` | Discover node types |
| `get_expression_type_info` | Node pins & properties |
| `search_material_functions` | Find Material Functions |
| `validate_material_graph` | Diagnose issues |
| `trace_material_connection` | Trace data flow |
| `cleanup_material_graph` | Remove orphaned nodes |
| `add_material_comments` | Comment boxes |
| `create/get_material_function` | Material Function management |
| `build_material_function_graph` | Build MF graph |
| `add/set_material_function_input/output` | MF pins |
| `validate/cleanup_material_function` | MF diagnostics |
| `apply_material_to_actor/blueprint` | Apply materials |
| `get_available_materials` | List materials |
| `get_actor/blueprint_material_info` | Material slot info |

### Data Tables (8)
| Command | Description |
|---------|-------------|
| `get_data_table_schema` | Column names and types |
| `get_data_table_rows` / `get_data_table_row` | Read rows |
| `add_data_table_row` | Add with initial data |
| `update_data_table_row` | Partial update |
| `delete_data_table_row` | Delete row |
| `duplicate_data_table_row` | Copy row |
| `rename_data_table_row` | Rename row |

### Data Assets (12)
| Command | Description |
|---------|-------------|
| `create_data_asset` | Create any UDataAsset subclass |
| `get/set_data_asset_property(ies)` | Read/write properties (single or batch) |
| `list_data_assets` | Browse by path/class |
| `list_data_asset_classes` | Discover all loaded UDataAsset subclasses |
| `get_property_valid_types` | Valid dropdown values for a property slot |
| `search_class_paths` | Find class paths |
| `get_mass_config_traits` | Inspect all traits on a Mass Entity Config asset |
| `add_mass_config_trait` | Append a new trait to a Mass Config (non-destructive) |
| `set_mass_config_trait_property` | **Surgical** in-place edit of a single trait property — never replaces the Traits array |
| `remove_mass_config_trait` | Remove a single trait by index or class without affecting siblings |

### Actors & Level (19)
| Command | Description |
|---------|-------------|
| `spawn_actor` | Spawn built-in actor types |
| `spawn_blueprint_actor` | Spawn BP actor |
| `spawn_actor_from_class` | Spawn from class name |
| `get_actors_in_level` | List all actors |
| `find_actors_by_name` | Search by name pattern |
| `get_actor_properties` | Read actor properties |
| `set_actor_transform` | Set location/rotation/scale |
| `delete_actor` | Remove from level |
| `get_selected_actors` | Viewport selection |
| `get_world_info` | Level info |
| `set_static_mesh_properties` | Mesh assignment |
| `set_physics_properties` | Physics config |
| `set_mesh_material_color` | Material color |
| `apply_material_to_actor/blueprint` | Apply material |
| `get_actor/blueprint_material_info` | Material slots |
| `get_available_materials` | List materials |
| `take_screenshot` | Capture viewport |

### Enhanced Input (21)
| Command | Description |
|---------|-------------|
| `create_input_action` | Create UInputAction |
| `get/set_input_action_properties` | Action properties |
| `add/remove_input_action_trigger` | Action triggers |
| `add/remove_input_action_modifier` | Action modifiers |
| `list_input_actions` | Browse actions |
| `create_input_mapping_context` | Create UInputMappingContext |
| `get_input_mapping_context` | Read mappings |
| `add/remove/set_key_mapping` | Key bindings |
| `add/remove_mapping_trigger/modifier` | Per-mapping overrides |
| `list_input_mapping_contexts` | Browse contexts |
| `list_trigger_types` / `list_modifier_types` | Discover types |
| `list_input_keys` | Valid key names |

### Widgets — UMG (11)
| Command | Description |
|---------|-------------|
| `get_widget_tree` | Widget hierarchy |
| `add_widget` | Add to parent |
| `remove/move/rename/duplicate_widget` | Widget operations |
| `get/set_widget_properties` | Widget properties |
| `get/set_slot_properties` | Layout slot properties |
| `list_widget_types` | Available widget classes |

### Niagara VFX (54, NEW)

Full coverage of the Niagara editor — create systems from templates, add and configure emitters, manage modules and renderers, write scratch pad HLSL, and spawn effects in the level. Built on Niagara's ViewModel API for safe asset modification.

#### Systems
| Command | Description |
|---------|-------------|
| `create_niagara_system` | Create from emitter template or empty |
| `get_niagara_system_info` | System metadata, emitters, parameters |
| `list_niagara_systems` | Browse Niagara systems by path |
| `delete_niagara_system` | Delete a system |
| `compile_niagara_system` | Force recompile |
| `set_niagara_system_property` | Set top-level system property |
| `get_niagara_system_errors` | Compilation errors / warnings |
| `get_niagara_particle_stats` | Per-emitter particle stats |
| `get/set_niagara_playback_range` | Preview playback range |

#### Emitters
| Command | Description |
|---------|-------------|
| `get_niagara_emitters` | List emitters in a system |
| `add_niagara_emitter` | Add from template |
| `remove_niagara_emitter` | Remove an emitter |
| `duplicate_niagara_emitter` | Copy emitter with new name |
| `reorder_niagara_emitter` | Change emitter index |
| `set_niagara_emitter_property` | Set emitter property |
| `get_niagara_emitter_attributes` | Particle attributes (Position, Velocity, etc.) |

#### Modules (Spawn / Update / Render stacks)
| Command | Description |
|---------|-------------|
| `get_niagara_modules` | List modules in any stack |
| `add_niagara_module` | Add module from script asset |
| `remove_niagara_module` | Remove a module |
| `set_niagara_module_enabled` | Enable/disable a module |
| `reorder_niagara_module` | Reorder within stack |
| `get_niagara_module_inputs` | Inspect module inputs with current values |
| `set_niagara_module_input` | Set static value on a module input |
| `set_niagara_dynamic_input` | Replace input with a dynamic input function |
| `set_niagara_curve` | Set curve points on a curve input |
| `get_niagara_module_versions` | List script versions |

#### Parameters & Bindings
| Command | Description |
|---------|-------------|
| `get_niagara_user_parameters` | List User-namespace parameters |
| `add_niagara_user_parameter` | Add a User parameter |
| `set_niagara_user_parameter` | Set a User parameter value |
| `remove_niagara_user_parameter` | Remove a User parameter |
| `link_niagara_parameter` | Bind module input to a parameter |
| `get_niagara_rapid_iteration_parameters` | RI param introspection |
| `set_niagara_rapid_iteration_parameter` | Set RI param value |

#### Renderers
| Command | Description |
|---------|-------------|
| `add_niagara_renderer` | Add Sprite/Mesh/Ribbon/Light renderer |
| `remove_niagara_renderer` | Remove a renderer |
| `get_niagara_renderer_info` | Renderer summary |
| `get_niagara_renderer_properties` | Full renderer property dump |
| `set_niagara_renderer_property` | Set renderer property |
| `set_niagara_renderer_binding` | Bind renderer attribute to particle data |

#### Scratch Pad & Custom Modules
| Command | Description |
|---------|-------------|
| `create_niagara_scratch_pad_module` | Create per-emitter scratch module |
| `set_niagara_scratch_pad_hlsl` | Write HLSL into scratch pad |
| `create_niagara_module_asset` | Create reusable Niagara Module Script asset |

#### Events & Simulation Stages
| Command | Description |
|---------|-------------|
| `add_niagara_event_handler` | Add event handler stage |
| `add_niagara_simulation_stage` | Add simulation stage |
| `get_niagara_event_handlers` | Inspect handlers on emitter |

#### Level Spawning
| Command | Description |
|---------|-------------|
| `spawn_niagara_effect` | Spawn at world location |
| `control_niagara_effect` | Activate / deactivate / restart |
| `add_niagara_component` | Add NiagaraComponent to a Blueprint |
| `get_niagara_actors` | Find spawned Niagara actors |

#### Discovery
| Command | Description |
|---------|-------------|
| `list_niagara_modules` | List all available Niagara module scripts |
| `list_niagara_emitter_templates` | List emitter templates |
| `list_niagara_data_interfaces` | Available data interfaces (DI_*) |
| `list_niagara_parameter_types` | Parameter type registry |

### StateTree (33, NEW)

Read and author StateTree assets — states, tasks, evaluators, transitions, conditions, parameters, and bindings. Schema-aware: works with both `StateTreeSchemaBase` and Mass schema variants.

#### Reading
| Command | Description |
|---------|-------------|
| `get_statetree_info` | Asset summary (schema, states, evaluators) |
| `get_statetree_full_info` | Full recursive dump (states + tasks + transitions + bindings) |
| `get_statetree_states` | List all states (flat) |
| `get_statetree_state` | Single state details by ID/name |
| `get_statetree_node` | Inspect any node (task/evaluator/condition) |
| `get_statetree_evaluators` | Global evaluators |
| `get_statetree_global_tasks` | Global tasks |
| `get_statetree_parameters` | Tree parameters |
| `get_statetree_bindings` | All property bindings |
| `get_statetree_transition_targets` | Valid transition targets for a state |
| `search_statetree_nodes` | Search nodes by name / type |

#### Authoring
| Command | Description |
|---------|-------------|
| `create_statetree` | Create new StateTree asset |
| `set_statetree_schema` | Set schema (e.g. Mass schema) |
| `add_statetree_state` | Add a state (parent or root) |
| `add_statetree_task` | Add task to a state |
| `add_statetree_evaluator` | Add global evaluator |
| `add_statetree_global_task` | Add global task |
| `add_statetree_condition` | Add enter / transition condition |
| `add_statetree_transition` | Add transition (event / completed / delegate) |
| `add_statetree_parameter` | Add tree parameter |
| `add_statetree_binding` | Bind property between nodes |
| `compile_statetree` | Compile after edits |

#### Modification
| Command | Description |
|---------|-------------|
| `set_statetree_state_property` | Edit state property |
| `set_statetree_node_property` | Edit task / evaluator / condition property |
| `set_statetree_transition_property` | Edit transition property |
| `set_statetree_color` | Set state color |

#### Removal
| Command | Description |
|---------|-------------|
| `remove_statetree_state` | Remove a state |
| `remove_statetree_node` | Remove a task / evaluator / condition |
| `remove_statetree_transition` | Remove a transition |
| `remove_statetree_binding` | Remove a binding |
| `remove_statetree_parameter` | Remove a parameter |

#### Discovery
| Command | Description |
|---------|-------------|
| `list_statetree_node_types` | All available task / evaluator / condition types |
| `list_statetree_enum_values` | Enum values for property dropdowns |

### Performance Profiling (3)
| Command | Description |
|---------|-------------|
| `performance_start_trace` | Start recording .utrace |
| `performance_stop_trace` | Stop and auto-load |
| `performance_analyze_insight` | Smart analysis (diagnose, spikes, flame, hotpath, search, histogram, etc.) |

### Debug (2)
| Command | Description |
|---------|-------------|
| `set_mcp_debug` | Enable token tracking |
| `get_mcp_token_stats` | Token usage stats |

---

## Usage Examples

These work with both the CLI and MCP server. With the CLI, the AI calls `ue-cli` via Bash. With MCP, the AI calls tools directly.

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

## Configuration

### Custom Port

Add to `Config/DefaultEngine.ini`:
```ini
[UnrealMCP]
Port=55557
```

### Environment Variable

```bash
# Force a specific port (overrides port file)
set UNREAL_MCP_PORT=55560
```

### Multiple Editor Instances

Each editor picks a unique port automatically. The CLI and MCP server read the port from `Saved/UnrealMCP/port.txt`. Use `--port` flag or `UNREAL_MCP_PORT` env var to target a specific instance.

---

## Architecture

### Plugin Structure

```
Plugins/UnrealMCP/
├── UnrealMCP.uplugin              # Plugin manifest
├── cli/                            # Go CLI source (ue-cli)
│   ├── cmd/                        # Command definitions (238 commands)
│   ├── internal/bridge/            # TCP client
│   ├── internal/project/           # Plugin embedding & project detection
│   └── npm/                        # npm package wrapper
├── unrealmcp/                      # Python MCP server
│   ├── _tcp_bridge.py              # TCP communication
│   └── tools/                      # Tool modules (one per category)
├── Source/UnrealMCPBridge/         # C++ editor plugin
│   ├── Public/                     # Headers
│   └── Private/                    # Implementation + command handlers
└── .github/workflows/release.yml   # CI: builds + GitHub Release + npm publish
```

### Communication Protocol

TCP with length-prefix framing:
```
[4 bytes: big-endian payload length] [N bytes: UTF-8 JSON]
```

Request: `{"type": "command_name", "params": {...}}`
Response: `{"status": "success", "result": {...}}`

---

## Adding Custom Commands

### CLI Side (Go)

Add a `CommandSpec` to the appropriate `cmd/*.go` file:

```go
{
    Name:    "my_command",
    Group:   "mygroup",
    Short:   "What it does",
    Long:    "Detailed description.",
    Example: "ue-cli my_command --param value",
    Params: []ParamSpec{
        {Name: "param", Type: "string", Required: true, Help: "Description"},
    },
},
```

### MCP Side (Python)

Create a new file in `unrealmcp/tools/`:

```python
# unrealmcp/tools/my_tools.py
from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call

@mcp.tool()
def my_command(param: str) -> str:
    """Description shown to the AI assistant."""
    return _call("my_command", {"param": param})
```

Register it in `unrealmcp/tools/__init__.py`:

```python
from unrealmcp.tools import my_tools  # noqa: F401
```

### C++ Side

Add a command handler in `Source/UnrealMCPBridge/Private/Commands/` and register it in `ExecuteCommand()`.

---

## Supported AI Tools

| Tool | Interface | Config File | Status |
|------|-----------|------------|--------|
| **Claude Code** | CLI or MCP | Bash (CLI) / `.mcp.json` (MCP) | Tested |
| **Cursor** | MCP | `.cursor/mcp.json` | Tested |
| **VS Code / Copilot** | MCP | `.vscode/mcp.json` | Supported |
| **Windsurf** | MCP | `~/.codeium/windsurf/mcp_config.json` | Supported |
| **Gemini CLI** | MCP | `.gemini/settings.json` | Supported |
| **JetBrains / Rider** | MCP | `.junie/mcp/mcp.json` | Supported |
| **Zed** | MCP | `~/.config/zed/settings.json` | Supported |
| **Amazon Q** | MCP | `.amazonq/mcp.json` | Supported |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Connection refused" | Is the UE5 editor running? Check Output Log for `MCP Bridge initialized on port XXXXX` |
| `ue-cli doctor` shows port file missing | Editor hasn't started yet, or check `Saved/UnrealMCP/port.txt` |
| Commands timeout | Large operations (profiling, complex graphs) have 300s timeout. Use `--timeout 600` to increase |
| Multiple editors | Use `--port <num>` or `UNREAL_MCP_PORT` env var to target a specific instance |
| Plugin not compiling | Ensure `PythonScriptPlugin`, `EditorScriptingUtilities`, `EnhancedInput` are enabled |
| Python import errors | Run `pip install fastmcp requests` — make sure the Python on your PATH matches your AI tool's |
| Plugin not loading | Verify `UnrealMCP.uplugin` has `"Type": "Editor"` and required plugins are available in your engine build |

---

## Releases & Distribution

| Channel | Command |
|---------|---------|
| **npm** | `npm install -g unrealcli` |
| **GitHub Releases** | [Download binaries](https://github.com/aadeshrao123/Unreal-MCP/releases) |
| **pip** (MCP only) | `pip install unrealmcp` |

Releases are automated via GitHub Actions. Push a tag to trigger:
```bash
git tag v1.2.2 && git push origin v1.2.2
# → Builds 5 platform binaries
# → Creates GitHub Release
# → Publishes to npm + PyPI
```

---

## Contributing

- **Bug?** [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new)
- **Feature idea?** [Open an issue](https://github.com/aadeshrao123/Unreal-MCP/issues/new)
- **Code?** [Fork](https://github.com/aadeshrao123/Unreal-MCP/fork), make changes, open a PR

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## Enterprise & Studio Integration

Need Unreal MCP integrated into your studio's production pipeline? I work directly with game studios to build custom AI-assisted workflows on top of this tool.

**What I offer:**

- **Pipeline Integration** — Set up Unreal MCP within your existing build system, CI/CD, and team workflow so every artist and developer can use AI assistants with your Unreal project out of the box
- **Custom Tool Development** — Build MCP commands tailored to your studio's proprietary formats, internal tools, and specific production needs that the open-source version doesn't cover
- **AI Workflow Design** — Design and implement how your team uses AI assistants (Claude, Cursor, Copilot) with Unreal Engine — from material creation to level design to asset pipelines
- **Advanced Material & Substrate Systems** — Procedural material generation, Substrate workflows, complex shader pipelines — fully automated through MCP

**Get in touch:**

- Email: [aadeshrao80@gmail.com](mailto:aadeshrao80@gmail.com)
- LinkedIn: [linkedin.com/in/aadeshyadav](https://www.linkedin.com/in/aadeshyadav/)
- Discord: `destroyerpal`
- Portfolio: [aadeshyadav.vercel.app](https://aadeshyadav.vercel.app/)

---

## Custom Development & Support

Need help with your Unreal project? I'm available for contract work across the full UE5 C++ stack, from gameplay and multiplayer to editor tooling and Mass Entity systems. See [SUPPORT.md](SUPPORT.md) for details.

[aadeshrao80@gmail.com](mailto:aadeshrao80@gmail.com) · [LinkedIn](https://www.linkedin.com/in/aadeshyadav/) · [Portfolio](https://aadeshyadav.vercel.app/) · Discord: `destroyerpal`

---

## License

MPL-2.0 — see [LICENSE](LICENSE) for details.
