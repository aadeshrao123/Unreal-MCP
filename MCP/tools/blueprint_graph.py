"""Blueprint graph tools — node manipulation via C++ TCP bridge.

Wraps commands requiring direct K2Node API access not available through
the Python scripting plugin.
"""

from typing import Any, Dict, Optional

from _bridge import mcp
from _tcp_bridge import _call


# -- Graph Node Tools -------------------------------------------------------

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

    Node types by category:
      FLOW:      Branch, Comparison, Switch, SwitchEnum, SwitchInteger, ExecutionSequence
      DATA:      VariableGet, VariableSet, MakeArray
      CASTING:   DynamicCast, ClassDynamicCast, CastByteToEnum
      UTILITY:   Print, CallFunction, Select, SpawnActor
      SPECIAL:   Timeline, GetDataTableRow, AddComponentByClass, Self, Knot
      EVENT:     Event (specify event_type: BeginPlay, Tick, Destroyed, etc.)

    Extra args are only needed for specific node types (message for Print,
    variable_name for Variable nodes, target_function for CallFunction, etc.).
    """
    params: Dict[str, Any] = {"pos_x": pos_x, "pos_y": pos_y}
    # Only include optional fields when set — keeps the wire payload small
    for key, val in [
        ("message", message),
        ("event_type", event_type),
        ("variable_name", variable_name),
        ("target_function", target_function),
        ("target_blueprint", target_blueprint),
        ("function_name", function_name),
    ]:
        if val:
            params[key] = val

    return _call("add_blueprint_node", {
        "blueprint_name": blueprint_name,
        "node_type": node_type,
        "node_params": params,
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
        source_node_id: GUID of the source node
        source_pin_name: Output pin name on source
        target_node_id: GUID of the target node
        target_pin_name: Input pin name on target
        function_name: Function graph (empty = EventGraph)
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

    variable_type: bool, int, float, string, vector, rotator
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

    All parameters besides blueprint_name and variable_name are optional —
    only provided values are changed.
    """
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "variable_name": variable_name,
    }
    # Only send fields that were explicitly provided
    for key, val in [
        ("var_name", var_name),
        ("var_type", var_type),
        ("tooltip", tooltip),
        ("category", category),
        ("default_value", default_value),
    ]:
        if val:
            params[key] = val
    for key, val in [
        ("is_public", is_public),
        ("is_editable_in_instance", is_editable_in_instance),
        ("expose_on_spawn", expose_on_spawn),
        ("replication_enabled", replication_enabled),
        ("replication_condition", replication_condition),
    ]:
        if val is not None:
            params[key] = val
    return _call("set_blueprint_variable_properties", params)


@mcp.tool()
def add_event_node(
    blueprint_name: str,
    event_name: str,
    pos_x: float = 0,
    pos_y: float = 0,
) -> str:
    """Add an event node (ReceiveBeginPlay, ReceiveTick, ReceiveDestroyed, etc.)."""
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
    """Delete a node from a Blueprint graph by GUID."""
    params: Dict[str, Any] = {"blueprint_name": blueprint_name, "node_id": node_id}
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
    """Set a node property or perform semantic editing.

    Semantic actions (pass via 'action'):
      add_pin, remove_pin, set_enum_type, set_pin_type,
      set_value_type, set_cast_target, set_function_call, set_event_type

    Each action has associated params (pin_type, pin_name, enum_type, new_type,
    target_type, target_function, target_class, event_type).
    """
    params: Dict[str, Any] = {"blueprint_name": blueprint_name, "node_id": node_id}
    for key, val in [
        ("property_name", property_name),
        ("property_value", property_value),
        ("function_name", function_name),
        ("action", action),
        ("pin_type", pin_type),
        ("pin_name", pin_name),
        ("enum_type", enum_type),
        ("new_type", new_type),
        ("target_type", target_type),
        ("target_function", target_function),
        ("target_class", target_class),
        ("event_type", event_type),
    ]:
        if val:
            params[key] = val
    return _call("set_node_property", params)


# -- Function Tools ----------------------------------------------------------

@mcp.tool()
def create_blueprint_function(blueprint_name: str, function_name: str, return_type: str = "void") -> str:
    """Create a new function in a Blueprint."""
    return _call("create_function", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "return_type": return_type,
    })


@mcp.tool()
def add_function_input(
    blueprint_name: str, function_name: str,
    param_name: str, param_type: str, is_array: bool = False,
) -> str:
    """Add an input parameter to a Blueprint function."""
    return _call("add_function_input", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "param_name": param_name,
        "param_type": param_type,
        "is_array": is_array,
    })


@mcp.tool()
def add_function_output(
    blueprint_name: str, function_name: str,
    param_name: str, param_type: str, is_array: bool = False,
) -> str:
    """Add an output parameter to a Blueprint function."""
    return _call("add_function_output", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "param_name": param_name,
        "param_type": param_type,
        "is_array": is_array,
    })


@mcp.tool()
def delete_blueprint_function(blueprint_name: str, function_name: str) -> str:
    """Delete a function from a Blueprint."""
    return _call("delete_function", {"blueprint_name": blueprint_name, "function_name": function_name})


@mcp.tool()
def rename_blueprint_function(blueprint_name: str, old_function_name: str, new_function_name: str) -> str:
    """Rename a function in a Blueprint."""
    return _call("rename_function", {
        "blueprint_name": blueprint_name,
        "old_function_name": old_function_name,
        "new_function_name": new_function_name,
    })


# -- Inspection Tools --------------------------------------------------------

@mcp.tool()
def read_blueprint_content(
    blueprint_path: str,
    include_event_graph: bool = True,
    include_functions: bool = True,
    include_variables: bool = True,
    include_components: bool = True,
    include_interfaces: bool = True,
) -> str:
    """Read complete Blueprint content: event graph, functions, variables, components."""
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
    """Analyze a specific graph — nodes, connections, and execution flow."""
    return _call("analyze_blueprint_graph", {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
        "include_node_details": include_node_details,
        "include_pin_connections": include_pin_connections,
        "trace_execution_flow": trace_execution_flow,
    })


@mcp.tool()
def get_blueprint_variable_details(blueprint_path: str, variable_name: str = "") -> str:
    """Get detailed info about Blueprint variables (empty name = all variables)."""
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
    """Get detailed info about Blueprint functions (empty name = all functions)."""
    params: Dict[str, Any] = {"blueprint_path": blueprint_path, "include_graph": include_graph}
    if function_name:
        params["function_name"] = function_name
    return _call("get_blueprint_function_details", params)


@mcp.tool()
def compile_blueprint(blueprint_name: str) -> str:
    """Compile a Blueprint."""
    return _call("compile_blueprint", {"blueprint_name": blueprint_name})
