"""StateTree tools — create, read, modify, and compile StateTree AI assets."""

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


# ---------------------------------------------------------------------------
# Read / Query
# ---------------------------------------------------------------------------

@mcp.tool()
def get_statetree_info(asset_path: str) -> str:
    """Get compact overview of a StateTree: schema, state count, evaluator count, etc.

    Args:
        asset_path: Content path to the StateTree asset
    """
    return _call("get_statetree_info", {"asset_path": asset_path})


@mcp.tool()
def get_statetree_states(
    asset_path: str,
    max_depth: int = -1,
    name_filter: str | None = None,
) -> str:
    """Get state hierarchy as a JSON tree. Each state shows name, type, ID, task/transition counts.

    Use max_depth to limit recursion (0=roots only, 1=roots+children, -1=unlimited).
    Use name_filter for case-insensitive substring match on state names.

    Args:
        asset_path: Content path to the StateTree asset
        max_depth: Maximum depth to recurse (-1 for unlimited)
        name_filter: Only include states whose name contains this substring
    """
    params: dict = {"asset_path": asset_path, "max_depth": max_depth}
    if name_filter is not None:
        params["name_filter"] = name_filter
    return _call("get_statetree_states", params)


@mcp.tool()
def get_statetree_state(
    asset_path: str,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
) -> str:
    """Get detailed info for a single state: properties, tasks, conditions, transitions.

    Identify by index path (e.g. "0/1/2"), name, or GUID.

    Args:
        asset_path: Content path to the StateTree asset
        state: Index path (e.g. "0" for first root, "0/1" for second child of first root)
        state_name: State name (case-insensitive)
        state_guid: State GUID string
    """
    params: dict = {"asset_path": asset_path}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    return _call("get_statetree_state", params)


@mcp.tool()
def get_statetree_evaluators(
    asset_path: str,
    filter: str | None = None,
) -> str:
    """Get all global evaluators with their properties and instance data.

    Args:
        asset_path: Content path to the StateTree asset
        filter: Optional substring filter on evaluator class name
    """
    params: dict = {"asset_path": asset_path}
    if filter is not None:
        params["filter"] = filter
    return _call("get_statetree_evaluators", params)


@mcp.tool()
def get_statetree_global_tasks(
    asset_path: str,
    filter: str | None = None,
) -> str:
    """Get all global tasks with their properties.

    Args:
        asset_path: Content path to the StateTree asset
        filter: Optional substring filter on task class name
    """
    params: dict = {"asset_path": asset_path}
    if filter is not None:
        params["filter"] = filter
    return _call("get_statetree_global_tasks", params)


@mcp.tool()
def get_statetree_parameters(asset_path: str) -> str:
    """Get tree-level parameters (name, type, value).

    Args:
        asset_path: Content path to the StateTree asset
    """
    return _call("get_statetree_parameters", {"asset_path": asset_path})


@mcp.tool()
def get_statetree_node(
    asset_path: str,
    node_guid: str,
) -> str:
    """Get detailed info for a single node (task/evaluator/condition) by GUID.

    Returns the node's class, all properties, instance data, and where it lives
    in the tree (which state, evaluator list, or global tasks).

    Args:
        asset_path: Content path to the StateTree asset
        node_guid: GUID of the node (from get_statetree_state or get_statetree_evaluators)
    """
    return _call("get_statetree_node", {
        "asset_path": asset_path,
        "node_guid": node_guid,
    })


@mcp.tool()
def get_statetree_bindings(
    asset_path: str,
    node_guid: str | None = None,
) -> str:
    """Get property bindings. Optionally filter to bindings involving a specific node.

    Args:
        asset_path: Content path to the StateTree asset
        node_guid: Only show bindings involving this node (optional)
    """
    params: dict = {"asset_path": asset_path}
    if node_guid is not None:
        params["node_guid"] = node_guid
    return _call("get_statetree_bindings", params)


# ---------------------------------------------------------------------------
# Create / Add
# ---------------------------------------------------------------------------

@mcp.tool()
def create_statetree(
    asset_path: str,
    schema_class: str,
) -> str:
    """Create a new StateTree asset with the specified schema.

    Args:
        asset_path: Content path for the new asset (e.g. "/Game/AI/ST_MyTree")
        schema_class: Schema class name (e.g. "MassStateTreeSchema",
            "StateTreeComponentSchema", "StateTreeAIComponentSchema")
    """
    return _call("create_statetree", {
        "asset_path": asset_path,
        "schema_class": schema_class,
    })


@mcp.tool()
def add_statetree_state(
    asset_path: str,
    name: str,
    parent: str | None = None,
    parent_name: str | None = None,
    parent_guid: str | None = None,
    state_type: str = "State",
    selection_behavior: str | None = None,
    position: int = -1,
) -> str:
    """Add a state to the tree. Omit parent params for a root-level state.

    Args:
        asset_path: Content path to the StateTree asset
        name: Name for the new state
        parent: Parent state index path (e.g. "0/1")
        parent_name: Parent state name
        parent_guid: Parent state GUID
        state_type: "State", "Group", "Linked", "LinkedAsset", or "Subtree"
        selection_behavior: "None", "TryEnterState", "TrySelectChildrenInOrder",
            "TrySelectChildrenAtRandom", "TrySelectChildrenWithHighestUtility",
            "TryFollowTransitions"
        position: Insert position (-1 = append)
    """
    params: dict = {"asset_path": asset_path, "name": name, "state_type": state_type, "position": position}
    if parent is not None:
        params["state"] = parent
    if parent_name is not None:
        params["state_name"] = parent_name
    if parent_guid is not None:
        params["state_guid"] = parent_guid
    if selection_behavior is not None:
        params["selection_behavior"] = selection_behavior
    return _call("add_statetree_state", params)


@mcp.tool()
def add_statetree_task(
    asset_path: str,
    task_class: str,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
    properties: dict | None = None,
    instance_properties: dict | None = None,
) -> str:
    """Add a task to a state.

    Args:
        asset_path: Content path to the StateTree asset
        task_class: Task struct name (e.g. "MassEnemyAttackTask",
            "MassNavMeshPathFollowTask") or full path
        state: State index path
        state_name: State name
        state_guid: State GUID
        properties: JSON object for task node properties
        instance_properties: JSON object for task instance data properties
    """
    params: dict = {"asset_path": asset_path, "task_class": task_class}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    if properties is not None:
        params["properties"] = properties
    if instance_properties is not None:
        params["instance_properties"] = instance_properties
    return _call("add_statetree_task", params)


@mcp.tool()
def add_statetree_evaluator(
    asset_path: str,
    evaluator_class: str,
    properties: dict | None = None,
    instance_properties: dict | None = None,
) -> str:
    """Add a global evaluator to the StateTree.

    Args:
        asset_path: Content path to the StateTree asset
        evaluator_class: Evaluator struct name (e.g. "MassEnemyHasTargetEvaluator")
        properties: JSON object for evaluator node properties
        instance_properties: JSON object for evaluator instance data properties
    """
    params: dict = {"asset_path": asset_path, "evaluator_class": evaluator_class}
    if properties is not None:
        params["properties"] = properties
    if instance_properties is not None:
        params["instance_properties"] = instance_properties
    return _call("add_statetree_evaluator", params)


@mcp.tool()
def add_statetree_global_task(
    asset_path: str,
    task_class: str,
    properties: dict | None = None,
    instance_properties: dict | None = None,
) -> str:
    """Add a global task to the StateTree.

    Args:
        asset_path: Content path to the StateTree asset
        task_class: Task struct name
        properties: JSON object for task node properties
        instance_properties: JSON object for task instance data properties
    """
    params: dict = {"asset_path": asset_path, "task_class": task_class}
    if properties is not None:
        params["properties"] = properties
    if instance_properties is not None:
        params["instance_properties"] = instance_properties
    return _call("add_statetree_global_task", params)


@mcp.tool()
def add_statetree_condition(
    asset_path: str,
    condition_class: str,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
    target: str = "enter_conditions",
    transition_index: int | None = None,
    operand: str = "And",
    properties: dict | None = None,
    instance_properties: dict | None = None,
) -> str:
    """Add a condition to a state's enter conditions or a transition's conditions.

    Args:
        asset_path: Content path to the StateTree asset
        condition_class: Condition struct name (e.g. "StateTreeCompareIntCondition")
        state: State index path
        state_name: State name
        state_guid: State GUID
        target: "enter_conditions" or "transition"
        transition_index: Required when target="transition"
        operand: "And" or "Or" (how to combine with previous conditions)
        properties: JSON object for condition properties
        instance_properties: JSON object for condition instance data
    """
    params: dict = {
        "asset_path": asset_path,
        "condition_class": condition_class,
        "target": target,
        "operand": operand,
    }
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    if transition_index is not None:
        params["transition_index"] = transition_index
    if properties is not None:
        params["properties"] = properties
    if instance_properties is not None:
        params["instance_properties"] = instance_properties
    return _call("add_statetree_condition", params)


@mcp.tool()
def add_statetree_transition(
    asset_path: str,
    trigger: str,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
    target_state: str | None = None,
    target_state_name: str | None = None,
    target_state_guid: str | None = None,
    priority: str = "Normal",
    delay: float | None = None,
    delay_variance: float | None = None,
    event_tag: str | None = None,
) -> str:
    """Add a transition to a state.

    Args:
        asset_path: Content path to the StateTree asset
        trigger: "OnStateCompleted", "OnStateFailed", "OnStateSucceeded", "OnTick", "OnEvent"
        state: Source state index path
        state_name: Source state name
        state_guid: Source state GUID
        target_state: Target state index path, or special: "TreeSucceeded", "TreeFailed", "TreeStopped", "NextSibling"
        target_state_name: Target state name
        target_state_guid: Target state GUID
        priority: "Low", "Normal", "Medium", "High", "Critical"
        delay: Delay in seconds before transitioning
        delay_variance: Random variance (+/-) on delay
        event_tag: Gameplay tag for OnEvent trigger
    """
    params: dict = {"asset_path": asset_path, "trigger": trigger, "priority": priority}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    if target_state is not None:
        params["target_state"] = target_state
    if target_state_name is not None:
        params["target_state_name"] = target_state_name
    if target_state_guid is not None:
        params["target_state_guid"] = target_state_guid
    if delay is not None:
        params["delay"] = delay
    if delay_variance is not None:
        params["delay_variance"] = delay_variance
    if event_tag is not None:
        params["event_tag"] = event_tag
    return _call("add_statetree_transition", params)


@mcp.tool()
def add_statetree_parameter(
    asset_path: str,
    name: str,
    type: str,
) -> str:
    """Add a tree-level parameter.

    Args:
        asset_path: Content path to the StateTree asset
        name: Parameter name
        type: Property type ("bool", "int32", "float", "FVector", "FName", "FString", etc.)
    """
    return _call("add_statetree_parameter", {
        "asset_path": asset_path,
        "name": name,
        "type": type,
    })


# ---------------------------------------------------------------------------
# Modify
# ---------------------------------------------------------------------------

@mcp.tool()
def set_statetree_state_property(
    asset_path: str,
    property_name: str,
    value: str | float | int | bool,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
) -> str:
    """Set a property on a state (Name, Type, SelectionBehavior, Enabled, Tag, Weight, etc.).

    Args:
        asset_path: Content path to the StateTree asset
        property_name: Property to set
        value: New value
        state: State index path
        state_name: State name
        state_guid: State GUID
    """
    params: dict = {"asset_path": asset_path, "property_name": property_name, "value": value}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    return _call("set_statetree_state_property", params)


@mcp.tool()
def set_statetree_node_property(
    asset_path: str,
    node_guid: str,
    property_name: str,
    property_value: str | float | int | bool | dict | list,
    target: str = "node",
) -> str:
    """Set a property on a task, evaluator, or condition by GUID.

    Args:
        asset_path: Content path to the StateTree asset
        node_guid: GUID of the node
        property_name: Property to set
        property_value: New value (structs as dict, arrays as list, enums as string)
        target: "node" for the node struct itself, "instance" for instance data
    """
    return _call("set_statetree_node_property", {
        "asset_path": asset_path,
        "node_guid": node_guid,
        "property_name": property_name,
        "property_value": property_value,
        "target": target,
    })


@mcp.tool()
def set_statetree_transition_property(
    asset_path: str,
    transition_index: int,
    property_name: str,
    value: str | float | int | bool,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
) -> str:
    """Set a property on a transition (Trigger, Priority, TargetState, Delay, Enabled, etc.).

    Args:
        asset_path: Content path to the StateTree asset
        transition_index: Index of the transition in the state's Transitions array
        property_name: Property to set
        value: New value
        state: State index path
        state_name: State name
        state_guid: State GUID
    """
    params: dict = {
        "asset_path": asset_path,
        "transition_index": transition_index,
        "property_name": property_name,
        "value": value,
    }
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    return _call("set_statetree_transition_property", params)


@mcp.tool()
def set_statetree_schema(
    asset_path: str,
    schema_class: str,
) -> str:
    """Change the schema of a StateTree. Warning: existing nodes may become invalid.

    Args:
        asset_path: Content path to the StateTree asset
        schema_class: Schema class name (e.g. "MassStateTreeSchema")
    """
    return _call("set_statetree_schema", {
        "asset_path": asset_path,
        "schema_class": schema_class,
    })


@mcp.tool()
def add_statetree_binding(
    asset_path: str,
    source_node_guid: str,
    source_property: str,
    target_node_guid: str,
    target_property: str,
) -> str:
    """Add a property binding connecting a source node's output to a target node's input.

    Args:
        asset_path: Content path to the StateTree asset
        source_node_guid: GUID of the source node
        source_property: Property path on source (e.g. "TargetLocation")
        target_node_guid: GUID of the target node
        target_property: Property path on target (e.g. "TargetLocation")
    """
    return _call("add_statetree_binding", {
        "asset_path": asset_path,
        "source_node_guid": source_node_guid,
        "source_property": source_property,
        "target_node_guid": target_node_guid,
        "target_property": target_property,
    })


@mcp.tool()
def set_statetree_color(
    asset_path: str,
    name: str,
    color: str,
) -> str:
    """Add or update a color in the StateTree theme.

    Args:
        asset_path: Content path to the StateTree asset
        name: Display name for the color
        color: Color as hex "#RRGGBB" or "R,G,B,A" floats (e.g. "0.5,0.2,0.8,1.0")
    """
    return _call("set_statetree_color", {
        "asset_path": asset_path,
        "name": name,
        "color": color,
    })


# ---------------------------------------------------------------------------
# Remove
# ---------------------------------------------------------------------------

@mcp.tool()
def remove_statetree_state(
    asset_path: str,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
) -> str:
    """Remove a state and all its children from the tree.

    Args:
        asset_path: Content path to the StateTree asset
        state: State index path
        state_name: State name
        state_guid: State GUID
    """
    params: dict = {"asset_path": asset_path}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    return _call("remove_statetree_state", params)


@mcp.tool()
def remove_statetree_node(
    asset_path: str,
    node_guid: str,
) -> str:
    """Remove a task, evaluator, or condition by GUID.

    Args:
        asset_path: Content path to the StateTree asset
        node_guid: GUID of the node to remove
    """
    return _call("remove_statetree_node", {
        "asset_path": asset_path,
        "node_guid": node_guid,
    })


@mcp.tool()
def remove_statetree_transition(
    asset_path: str,
    transition_index: int,
    state: str | None = None,
    state_name: str | None = None,
    state_guid: str | None = None,
) -> str:
    """Remove a transition from a state.

    Args:
        asset_path: Content path to the StateTree asset
        transition_index: Index of the transition to remove
        state: State index path
        state_name: State name
        state_guid: State GUID
    """
    params: dict = {"asset_path": asset_path, "transition_index": transition_index}
    if state is not None:
        params["state"] = state
    if state_name is not None:
        params["state_name"] = state_name
    if state_guid is not None:
        params["state_guid"] = state_guid
    return _call("remove_statetree_transition", params)


@mcp.tool()
def remove_statetree_binding(
    asset_path: str,
    binding_index: int,
) -> str:
    """Remove a property binding by index.

    Args:
        asset_path: Content path to the StateTree asset
        binding_index: Index of the binding to remove
    """
    return _call("remove_statetree_binding", {
        "asset_path": asset_path,
        "binding_index": binding_index,
    })


@mcp.tool()
def remove_statetree_parameter(
    asset_path: str,
    name: str,
) -> str:
    """Remove a tree-level parameter by name.

    Args:
        asset_path: Content path to the StateTree asset
        name: Parameter name to remove
    """
    return _call("remove_statetree_parameter", {
        "asset_path": asset_path,
        "name": name,
    })


# ---------------------------------------------------------------------------
# Compile / Discovery
# ---------------------------------------------------------------------------

@mcp.tool()
def compile_statetree(asset_path: str) -> str:
    """Compile a StateTree and return errors/warnings.

    Args:
        asset_path: Content path to the StateTree asset
    """
    return _call("compile_statetree", {"asset_path": asset_path})


@mcp.tool()
def list_statetree_node_types(
    asset_path: str,
    category: str,
    filter: str | None = None,
) -> str:
    """List available task/evaluator/condition types for the StateTree's schema.

    Args:
        asset_path: Content path to the StateTree asset (determines schema)
        category: "task", "evaluator", or "condition"
        filter: Optional substring filter on class name
    """
    params: dict = {"asset_path": asset_path, "category": category}
    if filter is not None:
        params["filter"] = filter
    return _call("list_statetree_node_types", params)


@mcp.tool()
def get_statetree_transition_targets(asset_path: str) -> str:
    """Get all valid transition targets for a StateTree.

    Returns two lists:
    - 'states': All states with name, GUID, index_path, and type (for GotoState transitions)
    - 'meta_targets': Built-in targets (None, TreeSucceeded, TreeFailed, TreeStopped,
      NextSibling, NextState, NextSelectableState)

    Use this before add_statetree_transition or set_statetree_transition_property
    to discover valid target_state values.

    Args:
        asset_path: Content path to the StateTree asset
    """
    return _call("get_statetree_transition_targets", {"asset_path": asset_path})


@mcp.tool()
def list_statetree_enum_values(category: str) -> str:
    """List valid enum values for StateTree properties.

    Use this to discover valid values BEFORE creating states, transitions, or conditions.
    Does not require an asset_path — these are engine-level constants.

    Categories:
    - "trigger": OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent
    - "priority": Low, Normal, Medium, High, Critical
    - "state_type": State, Group, Linked, LinkedAsset, Subtree
    - "selection_behavior": None, TryEnterState, TrySelectChildrenInOrder, etc.
    - "operand": And, Or (for condition expressions)

    Args:
        category: One of: trigger, priority, state_type, selection_behavior, operand
    """
    return _call("list_statetree_enum_values", {"category": category})
