"""Blueprint graph tools — node manipulation via C++ TCP bridge (port 55557).

These tools wrap the C++ UnrealMCPBridge commands for Blueprint graph
operations that require direct K2Node API access not available through
the Python scripting plugin.
"""

from typing import Any, Dict, List, Optional

from _bridge import mcp
from _tcp_bridge import _tcp_send_raw


# ── helpers ────────────────────────────────────────────────────────────────

def _ok(resp: Dict[str, Any]) -> str:
    """Format a TCP bridge response as a human-readable string."""
    import json
    return json.dumps(resp, default=str, indent=2)


def _call(command: str, params: Dict[str, Any]) -> str:
    return _ok(_tcp_send_raw(command, params))


# ── Blueprint Graph Node Tools ─────────────────────────────────────────────

@mcp.tool()
def add_blueprint_node(
    blueprint_name: str,
    node_type: str,
    pos_x: float = 0,
    pos_y: float = 0,
    message: str = "",
    event_type: str = "",
    variable_name: str = "",
    target_function: str = "",
    target_blueprint: str = "",
    function_name: str = "",
) -> str:
    """Add a node to a Blueprint graph.

    Supports 23 node types organized by category:

    CONTROL FLOW: Branch, Comparison, Switch, SwitchEnum, SwitchInteger,
        ExecutionSequence
    DATA: VariableGet, VariableSet, MakeArray
    CASTING: DynamicCast, ClassDynamicCast, CastByteToEnum
    UTILITY: Print, CallFunction, Select, SpawnActor
    SPECIALIZED: Timeline, GetDataTableRow, AddComponentByClass, Self, Knot
    EVENT: Event (specify event_type: BeginPlay, Tick, Destroyed, etc.)

    Args:
        blueprint_name: Name of the Blueprint to modify
        node_type: Type of node to create (see list above)
        pos_x: X position in graph
        pos_y: Y position in graph
        message: For Print nodes, the text to print
        event_type: For Event nodes, the event name
        variable_name: For Variable nodes, the variable name
        target_function: For CallFunction nodes, the function to call
        target_blueprint: For CallFunction nodes, optional Blueprint path
        function_name: Name of function graph (if empty, uses EventGraph)
    """
    node_params: Dict[str, Any] = {"pos_x": pos_x, "pos_y": pos_y}
    if message:
        node_params["message"] = message
    if event_type:
        node_params["event_type"] = event_type
    if variable_name:
        node_params["variable_name"] = variable_name
    if target_function:
        node_params["target_function"] = target_function
    if target_blueprint:
        node_params["target_blueprint"] = target_blueprint
    if function_name:
        node_params["function_name"] = function_name

    return _call("add_blueprint_node", {
        "blueprint_name": blueprint_name,
        "node_type": node_type,
        "node_params": node_params,
    })


@mcp.tool()
def connect_blueprint_nodes(
    blueprint_name: str,
    source_node_id: str,
    source_pin_name: str,
    target_node_id: str,
    target_pin_name: str,
    function_name: str = "",
) -> str:
    """Connect two nodes in a Blueprint graph.

    Args:
        blueprint_name: Name of the Blueprint
        source_node_id: GUID of the source node
        source_pin_name: Name of the output pin on source
        target_node_id: GUID of the target node
        target_pin_name: Name of the input pin on target
        function_name: Function graph name (empty = EventGraph)
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "source_node_id": source_node_id,
        "source_pin_name": source_pin_name,
        "target_node_id": target_node_id,
        "target_pin_name": target_pin_name,
    }
    if function_name:
        params["function_name"] = function_name
    return _call("connect_nodes", params)


@mcp.tool()
def create_blueprint_variable(
    blueprint_name: str,
    variable_name: str,
    variable_type: str,
    default_value: str = "",
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default",
) -> str:
    """Create a variable in a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        variable_type: Type (bool, int, float, string, vector, rotator)
        default_value: Default value (optional)
        is_public: Whether the variable is public/editable
        tooltip: Tooltip text
        category: Category for organization
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "variable_name": variable_name,
        "variable_type": variable_type,
        "is_public": is_public,
        "tooltip": tooltip,
        "category": category,
    }
    if default_value:
        params["default_value"] = default_value
    return _call("create_variable", params)


@mcp.tool()
def set_blueprint_variable_properties(
    blueprint_name: str,
    variable_name: str,
    var_name: str = "",
    var_type: str = "",
    is_public: bool = None,
    is_editable_in_instance: bool = None,
    tooltip: str = "",
    category: str = "",
    default_value: str = "",
    expose_on_spawn: bool = None,
    replication_enabled: bool = None,
    replication_condition: int = None,
) -> str:
    """Modify properties of an existing Blueprint variable.

    Preserves all VariableGet/Set nodes connected to this variable.

    Args:
        blueprint_name: Name of the Blueprint
        variable_name: Variable to modify
        var_name: Rename the variable (optional)
        var_type: Change type (optional)
        is_public: Set visibility (optional)
        is_editable_in_instance: Modifiable on instances (optional)
        tooltip: Description (optional)
        category: Category (optional)
        default_value: New default (optional)
        expose_on_spawn: Show in spawn dialog (optional)
        replication_enabled: Enable replication (optional)
        replication_condition: Replication condition 0-7 (optional)
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "variable_name": variable_name,
    }
    if var_name:
        params["var_name"] = var_name
    if var_type:
        params["var_type"] = var_type
    if is_public is not None:
        params["is_public"] = is_public
    if is_editable_in_instance is not None:
        params["is_editable_in_instance"] = is_editable_in_instance
    if tooltip:
        params["tooltip"] = tooltip
    if category:
        params["category"] = category
    if default_value:
        params["default_value"] = default_value
    if expose_on_spawn is not None:
        params["expose_on_spawn"] = expose_on_spawn
    if replication_enabled is not None:
        params["replication_enabled"] = replication_enabled
    if replication_condition is not None:
        params["replication_condition"] = replication_condition
    return _call("set_blueprint_variable_properties", params)


@mcp.tool()
def add_event_node(
    blueprint_name: str,
    event_name: str,
    pos_x: float = 0,
    pos_y: float = 0,
) -> str:
    """Add an event node to a Blueprint graph.

    Args:
        blueprint_name: Name of the Blueprint
        event_name: Event name (ReceiveBeginPlay, ReceiveTick, ReceiveDestroyed, etc.)
        pos_x: X position in graph
        pos_y: Y position in graph
    """
    return _call("add_event_node", {
        "blueprint_name": blueprint_name,
        "event_name": event_name,
        "pos_x": pos_x,
        "pos_y": pos_y,
    })


@mcp.tool()
def delete_blueprint_node(
    blueprint_name: str,
    node_id: str,
    function_name: str = "",
) -> str:
    """Delete a node from a Blueprint graph.

    Args:
        blueprint_name: Name of the Blueprint
        node_id: GUID of the node to delete
        function_name: Function graph name (empty = EventGraph)
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "node_id": node_id,
    }
    if function_name:
        params["function_name"] = function_name
    return _call("delete_node", params)


@mcp.tool()
def set_blueprint_node_property(
    blueprint_name: str,
    node_id: str,
    property_name: str = "",
    property_value: str = "",
    function_name: str = "",
    action: str = "",
    pin_type: str = "",
    pin_name: str = "",
    enum_type: str = "",
    new_type: str = "",
    target_type: str = "",
    target_function: str = "",
    target_class: str = "",
    event_type: str = "",
) -> str:
    """Set a property on a Blueprint node or perform semantic editing.

    Supports both simple property setting and advanced semantic actions:
      - "add_pin": Add a pin (requires pin_type)
      - "remove_pin": Remove a pin (requires pin_name)
      - "set_enum_type": Set enum type (requires enum_type)
      - "set_pin_type": Change pin type (requires pin_name, new_type)
      - "set_value_type": Change value type (requires new_type)
      - "set_cast_target": Change cast target (requires target_type)
      - "set_function_call": Change function (requires target_function)
      - "set_event_type": Change event type (requires event_type)

    Args:
        blueprint_name: Name of the Blueprint
        node_id: GUID of the node
        property_name: Property to set (legacy mode)
        property_value: Value to set (legacy mode)
        function_name: Function graph name (empty = EventGraph)
        action: Semantic action (see above)
        pin_type: Pin type for add_pin action
        pin_name: Pin name for remove_pin/set_pin_type
        enum_type: Enum path for set_enum_type
        new_type: New type for type-change actions
        target_type: Target class for cast actions
        target_function: Function name for set_function_call
        target_class: Class containing target_function
        event_type: Event type for set_event_type
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "node_id": node_id,
    }
    if property_name:
        params["property_name"] = property_name
    if property_value:
        params["property_value"] = property_value
    if function_name:
        params["function_name"] = function_name
    if action:
        params["action"] = action
    if pin_type:
        params["pin_type"] = pin_type
    if pin_name:
        params["pin_name"] = pin_name
    if enum_type:
        params["enum_type"] = enum_type
    if new_type:
        params["new_type"] = new_type
    if target_type:
        params["target_type"] = target_type
    if target_function:
        params["target_function"] = target_function
    if target_class:
        params["target_class"] = target_class
    if event_type:
        params["event_type"] = event_type
    return _call("set_node_property", params)


# ── Blueprint Function Tools ───────────────────────────────────────────────

@mcp.tool()
def create_blueprint_function(
    blueprint_name: str,
    function_name: str,
    return_type: str = "void",
) -> str:
    """Create a new function in a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint
        function_name: Name for the new function
        return_type: Return type (default: void)
    """
    return _call("create_function", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "return_type": return_type,
    })


@mcp.tool()
def add_function_input(
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False,
) -> str:
    """Add an input parameter to a Blueprint function.

    Args:
        blueprint_name: Name of the Blueprint
        function_name: Name of the function
        param_name: Parameter name
        param_type: Parameter type (bool, int, float, string, vector, etc.)
        is_array: Whether the parameter is an array
    """
    return _call("add_function_input", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "param_name": param_name,
        "param_type": param_type,
        "is_array": is_array,
    })


@mcp.tool()
def add_function_output(
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False,
) -> str:
    """Add an output parameter to a Blueprint function.

    Args:
        blueprint_name: Name of the Blueprint
        function_name: Name of the function
        param_name: Parameter name
        param_type: Parameter type (bool, int, float, string, vector, etc.)
        is_array: Whether the parameter is an array
    """
    return _call("add_function_output", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "param_name": param_name,
        "param_type": param_type,
        "is_array": is_array,
    })


@mcp.tool()
def delete_blueprint_function(
    blueprint_name: str,
    function_name: str,
) -> str:
    """Delete a function from a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint
        function_name: Name of the function to delete
    """
    return _call("delete_function", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
    })


@mcp.tool()
def rename_blueprint_function(
    blueprint_name: str,
    old_function_name: str,
    new_function_name: str,
) -> str:
    """Rename a function in a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint
        old_function_name: Current name
        new_function_name: New name
    """
    return _call("rename_function", {
        "blueprint_name": blueprint_name,
        "old_function_name": old_function_name,
        "new_function_name": new_function_name,
    })


# ── Blueprint Inspection Tools ─────────────────────────────────────────────

@mcp.tool()
def read_blueprint_content(
    blueprint_path: str,
    include_event_graph: bool = True,
    include_functions: bool = True,
    include_variables: bool = True,
    include_components: bool = True,
    include_interfaces: bool = True,
) -> str:
    """Read complete Blueprint content: event graph, functions, variables, components.

    Args:
        blueprint_path: Full path (e.g. "/Game/Blueprints/BP_MyActor")
        include_event_graph: Include event graph nodes and connections
        include_functions: Include custom functions and their graphs
        include_variables: Include all variables with types and defaults
        include_components: Include component hierarchy
        include_interfaces: Include implemented interfaces
    """
    return _call("read_blueprint_content", {
        "blueprint_path": blueprint_path,
        "include_event_graph": include_event_graph,
        "include_functions": include_functions,
        "include_variables": include_variables,
        "include_components": include_components,
        "include_interfaces": include_interfaces,
    })


@mcp.tool()
def analyze_blueprint_graph(
    blueprint_path: str,
    graph_name: str = "EventGraph",
    include_node_details: bool = True,
    include_pin_connections: bool = True,
    trace_execution_flow: bool = True,
) -> str:
    """Analyze a specific graph within a Blueprint.

    Args:
        blueprint_path: Full path to the Blueprint
        graph_name: Graph to analyze ("EventGraph", function name, etc.)
        include_node_details: Include detailed node properties
        include_pin_connections: Include pin-to-pin connections
        trace_execution_flow: Trace execution flow through the graph
    """
    return _call("analyze_blueprint_graph", {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
        "include_node_details": include_node_details,
        "include_pin_connections": include_pin_connections,
        "trace_execution_flow": trace_execution_flow,
    })


@mcp.tool()
def get_blueprint_variable_details(
    blueprint_path: str,
    variable_name: str = "",
) -> str:
    """Get detailed info about Blueprint variables.

    Args:
        blueprint_path: Full path to the Blueprint
        variable_name: Specific variable (empty = all variables)
    """
    params: Dict[str, Any] = {"blueprint_path": blueprint_path}
    if variable_name:
        params["variable_name"] = variable_name
    return _call("get_blueprint_variable_details", params)


@mcp.tool()
def get_blueprint_function_details(
    blueprint_path: str,
    function_name: str = "",
    include_graph: bool = True,
) -> str:
    """Get detailed info about Blueprint functions.

    Args:
        blueprint_path: Full path to the Blueprint
        function_name: Specific function (empty = all functions)
        include_graph: Include the function's graph nodes
    """
    params: Dict[str, Any] = {
        "blueprint_path": blueprint_path,
        "include_graph": include_graph,
    }
    if function_name:
        params["function_name"] = function_name
    return _call("get_blueprint_function_details", params)


@mcp.tool()
def compile_blueprint(blueprint_name: str) -> str:
    """Compile a Blueprint.

    Args:
        blueprint_name: Name of the Blueprint to compile
    """
    return _call("compile_blueprint", {"blueprint_name": blueprint_name})
