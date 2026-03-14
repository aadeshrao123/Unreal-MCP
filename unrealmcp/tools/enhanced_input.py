"""Enhanced Input tools — create and modify UInputAction and UInputMappingContext assets."""

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


# ---------------------------------------------------------------------------
# Input Action
# ---------------------------------------------------------------------------

@mcp.tool()
def create_input_action(
    asset_path: str,
    value_type: str = "Boolean",
    properties: dict | None = None,
) -> str:
    """Create a new UInputAction asset.

    Args:
        asset_path: Full content path including name (e.g. "/Game/Input/IA_Jump")
        value_type: Boolean | Axis1D | Axis2D | Axis3D
        properties: Optional dict of additional properties to set after creation
    """
    params = {
        "asset_path": asset_path,
        "value_type": value_type,
    }
    if properties:
        params["properties"] = properties
    return _call("create_input_action", params)


@mcp.tool()
def get_input_action(asset_path: str) -> str:
    """Read all properties, triggers, and modifiers of an Input Action.

    Returns value_type, consume_input, trigger_when_paused, accumulation_behavior,
    and full details of all triggers and modifiers with their properties.
    """
    return _call("get_input_action", {"asset_path": asset_path})


@mcp.tool()
def set_input_action_properties(
    asset_path: str,
    value_type: str | None = None,
    properties: dict | None = None,
) -> str:
    """Set properties on an Input Action.

    Args:
        asset_path: Path to the InputAction asset
        value_type: Convenience shortcut — Boolean | Axis1D | Axis2D | Axis3D
        properties: Dict of property names to values (e.g. {"bConsumeInput": false})
    """
    params: dict = {"asset_path": asset_path}
    if value_type:
        params["value_type"] = value_type
    if properties:
        params["properties"] = properties
    return _call("set_input_action_properties", params)


@mcp.tool()
def add_input_action_trigger(
    asset_path: str,
    trigger_type: str,
    properties: dict | None = None,
) -> str:
    """Add a trigger to an Input Action.

    Args:
        asset_path: Path to the InputAction asset
        trigger_type: Class name — InputTriggerDown | InputTriggerPressed | InputTriggerReleased |
            InputTriggerHold | InputTriggerHoldAndRelease | InputTriggerTap |
            InputTriggerRepeatedTap | InputTriggerPulse | InputTriggerChordAction | InputTriggerCombo
        properties: Optional trigger properties (e.g. {"HoldTimeThreshold": 0.5, "bIsOneShot": true})
    """
    params: dict = {
        "asset_path": asset_path,
        "trigger_type": trigger_type,
    }
    if properties:
        params["properties"] = properties
    return _call("add_input_action_trigger", params)


@mcp.tool()
def add_input_action_modifier(
    asset_path: str,
    modifier_type: str,
    properties: dict | None = None,
) -> str:
    """Add a modifier to an Input Action.

    Args:
        asset_path: Path to the InputAction asset
        modifier_type: Class name — InputModifierDeadZone | InputModifierScalar | InputModifierNegate |
            InputModifierSwizzleAxis | InputModifierSmooth | InputModifierSmoothDelta |
            InputModifierScaleByDeltaTime | InputModifierResponseCurveExponential |
            InputModifierResponseCurveUser | InputModifierFOVScaling | InputModifierToWorldSpace
        properties: Optional modifier properties (e.g. {"LowerThreshold": 0.2, "Type": "Radial"})
    """
    params: dict = {
        "asset_path": asset_path,
        "modifier_type": modifier_type,
    }
    if properties:
        params["properties"] = properties
    return _call("add_input_action_modifier", params)


@mcp.tool()
def remove_input_action_trigger(asset_path: str, index: int) -> str:
    """Remove a trigger from an Input Action by index.

    Use get_input_action to see current trigger indices.
    """
    return _call("remove_input_action_trigger", {
        "asset_path": asset_path,
        "index": index,
    })


@mcp.tool()
def remove_input_action_modifier(asset_path: str, index: int) -> str:
    """Remove a modifier from an Input Action by index.

    Use get_input_action to see current modifier indices.
    """
    return _call("remove_input_action_modifier", {
        "asset_path": asset_path,
        "index": index,
    })


@mcp.tool()
def list_input_actions(
    path: str = "/Game",
    filter: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """List all UInputAction assets, optionally filtered by name and path.

    Returns name, path, value_type, trigger_count, modifier_count for each.
    """
    return _call("list_input_actions", {
        "path": path,
        "filter": filter,
        "recursive": recursive,
        "max_results": max_results,
    })


# ---------------------------------------------------------------------------
# Input Mapping Context
# ---------------------------------------------------------------------------

@mcp.tool()
def create_input_mapping_context(
    asset_path: str,
    description: str = "",
) -> str:
    """Create a new UInputMappingContext asset.

    Args:
        asset_path: Full content path including name (e.g. "/Game/Input/IMC_Default")
        description: Optional localized description
    """
    params: dict = {"asset_path": asset_path}
    if description:
        params["description"] = description
    return _call("create_input_mapping_context", params)


@mcp.tool()
def get_input_mapping_context(asset_path: str) -> str:
    """Read all key mappings from an Input Mapping Context.

    Returns each mapping's key, action, triggers, and modifiers with full property details.
    """
    return _call("get_input_mapping_context", {"asset_path": asset_path})


@mcp.tool()
def add_key_mapping(
    context_path: str,
    action_path: str,
    key: str,
    triggers: list | None = None,
    modifiers: list | None = None,
) -> str:
    """Add a key-to-action mapping to an Input Mapping Context.

    Args:
        context_path: Path to the IMC asset
        action_path: Path to the InputAction asset
        key: Key name (e.g. "SpaceBar", "W", "Gamepad_LeftY", "LeftMouseButton")
        triggers: Optional list of triggers, each {"type": "InputTriggerHold", "properties": {...}}
        modifiers: Optional list of modifiers, each {"type": "InputModifierNegate", "properties": {...}}
    """
    params: dict = {
        "context_path": context_path,
        "action_path": action_path,
        "key": key,
    }
    if triggers:
        params["triggers"] = triggers
    if modifiers:
        params["modifiers"] = modifiers
    return _call("add_key_mapping", params)


@mcp.tool()
def remove_key_mapping(
    context_path: str,
    index: int | None = None,
    action_path: str | None = None,
    key: str | None = None,
) -> str:
    """Remove a key mapping from an Input Mapping Context.

    Remove by index OR by action_path + key combination.

    Args:
        context_path: Path to the IMC asset
        index: Mapping index to remove (from get_input_mapping_context)
        action_path: Alternative — remove by action + key combo
        key: Alternative — remove by action + key combo
    """
    params: dict = {"context_path": context_path}
    if index is not None:
        params["index"] = index
    if action_path:
        params["action_path"] = action_path
    if key:
        params["key"] = key
    return _call("remove_key_mapping", params)


@mcp.tool()
def set_key_mapping(
    context_path: str,
    index: int,
    key: str | None = None,
    action_path: str | None = None,
) -> str:
    """Change the key or action on an existing mapping in an IMC.

    Args:
        context_path: Path to the IMC asset
        index: Mapping index to modify
        key: New key name (optional)
        action_path: New action path (optional)
    """
    params: dict = {
        "context_path": context_path,
        "index": index,
    }
    if key:
        params["key"] = key
    if action_path:
        params["action_path"] = action_path
    return _call("set_key_mapping", params)


@mcp.tool()
def add_mapping_trigger(
    context_path: str,
    mapping_index: int,
    trigger_type: str,
    properties: dict | None = None,
) -> str:
    """Add a trigger to a specific mapping in an Input Mapping Context.

    Args:
        context_path: Path to the IMC asset
        mapping_index: Index of the mapping (from get_input_mapping_context)
        trigger_type: Trigger class name (e.g. "InputTriggerPressed")
        properties: Optional trigger properties
    """
    params: dict = {
        "context_path": context_path,
        "mapping_index": mapping_index,
        "trigger_type": trigger_type,
    }
    if properties:
        params["properties"] = properties
    return _call("add_mapping_trigger", params)


@mcp.tool()
def add_mapping_modifier(
    context_path: str,
    mapping_index: int,
    modifier_type: str,
    properties: dict | None = None,
) -> str:
    """Add a modifier to a specific mapping in an Input Mapping Context.

    Args:
        context_path: Path to the IMC asset
        mapping_index: Index of the mapping (from get_input_mapping_context)
        modifier_type: Modifier class name (e.g. "InputModifierNegate")
        properties: Optional modifier properties
    """
    params: dict = {
        "context_path": context_path,
        "mapping_index": mapping_index,
        "modifier_type": modifier_type,
    }
    if properties:
        params["properties"] = properties
    return _call("add_mapping_modifier", params)


@mcp.tool()
def remove_mapping_trigger(
    context_path: str,
    mapping_index: int,
    trigger_index: int,
) -> str:
    """Remove a trigger from a mapping in an Input Mapping Context.

    Args:
        context_path: Path to the IMC asset
        mapping_index: Index of the mapping
        trigger_index: Index of the trigger within that mapping
    """
    return _call("remove_mapping_trigger", {
        "context_path": context_path,
        "mapping_index": mapping_index,
        "trigger_index": trigger_index,
    })


@mcp.tool()
def remove_mapping_modifier(
    context_path: str,
    mapping_index: int,
    modifier_index: int,
) -> str:
    """Remove a modifier from a mapping in an Input Mapping Context.

    Args:
        context_path: Path to the IMC asset
        mapping_index: Index of the mapping
        modifier_index: Index of the modifier within that mapping
    """
    return _call("remove_mapping_modifier", {
        "context_path": context_path,
        "mapping_index": mapping_index,
        "modifier_index": modifier_index,
    })


@mcp.tool()
def list_input_mapping_contexts(
    path: str = "/Game",
    filter: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """List all UInputMappingContext assets, optionally filtered.

    Returns name, path, mapping_count, description for each.
    """
    return _call("list_input_mapping_contexts", {
        "path": path,
        "filter": filter,
        "recursive": recursive,
        "max_results": max_results,
    })


# ---------------------------------------------------------------------------
# Discovery / Utility
# ---------------------------------------------------------------------------

@mcp.tool()
def list_trigger_types(filter: str = "") -> str:
    """List all available UInputTrigger subclasses with their editable properties.

    Useful for discovering valid trigger_type values. Returns name, display_name,
    parent class, and editable property names/types.
    """
    return _call("list_trigger_types", {"filter": filter})


@mcp.tool()
def list_modifier_types(filter: str = "") -> str:
    """List all available UInputModifier subclasses with their editable properties.

    Useful for discovering valid modifier_type values. Returns name, display_name,
    parent class, and editable property names/types.
    """
    return _call("list_modifier_types", {"filter": filter})


@mcp.tool()
def list_input_keys(
    filter: str = "",
    category: str = "",
    max_results: int = 500,
) -> str:
    """List available FKey values (keyboard, mouse, gamepad keys).

    Args:
        filter: Substring filter on key name or display name
        category: Filter by category — keyboard | mouse | gamepad | analog | digital
        max_results: Max keys to return (default 500)
    """
    return _call("list_input_keys", {
        "filter": filter,
        "category": category,
        "max_results": max_results,
    })
