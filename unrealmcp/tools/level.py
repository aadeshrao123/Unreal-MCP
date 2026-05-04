"""Level/world tools — spawn actors, inspect viewport selection, world info."""

from typing import Any

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


@mcp.tool()
def spawn_actor(
    class_name: str,
    location_x: float = 0.0,
    location_y: float = 0.0,
    location_z: float = 0.0,
    rotation_yaw: float = 0.0,
    rotation_pitch: float = 0.0,
    rotation_roll: float = 0.0,
) -> str:
    """Spawn an actor in the current editor level.

    Args:
        class_name: Blueprint path ("/Game/BP_Foo") or built-in class name
                    ("StaticMeshActor", "PointLight", "SpotLight", "DirectionalLight", "CameraActor")
        location_x, location_y, location_z: World position
        rotation_yaw, rotation_pitch, rotation_roll: Rotation in degrees
    """
    return _call("spawn_actor_from_class", {
        "class_name":     class_name,
        "location_x":     location_x,
        "location_y":     location_y,
        "location_z":     location_z,
        "rotation_yaw":   rotation_yaw,
        "rotation_pitch": rotation_pitch,
        "rotation_roll":  rotation_roll,
    })


@mcp.tool()
def get_selected_actors() -> str:
    """Get all currently selected actors in the editor viewport."""
    return _call("get_selected_actors")


@mcp.tool()
def get_world_info() -> str:
    """Get current editor level info (world name, actor count, actor list)."""
    return _call("get_world_info")


@mcp.tool()
def get_actor_properties(
    actor_label: str,
    filter: str = "",
    include_components: bool = False,
    flat: bool = False,
    max_depth: int = 3,
    include_metadata: bool = False,
    expand_arrays: bool = True,
    array_element_limit: int = 16,
    category: str = "",
    include_inherited: bool = True,
    max_entries: int = 200,
    cursor: int = 0,
) -> str:
    """Get property values from a live actor instance placed in the world.

    Unlike get_blueprint_class_defaults (CDO), this reads the placed instance —
    per-instance overrides are returned correctly.

    Two output modes:
      - Nested (default, flat=False): top-level FProperty names, structs as nested JSON.
        Filter only matches top-level names — best for quickly inspecting one actor.
      - Flat (flat=True): every leaf returned as "Settings.BloomIntensity": value
        with a path-aware filter. Best for AI search ("find me everything containing 'bloom'").

    Args:
        actor_label: Outliner label or UObject name.
        filter: Case-insensitive substring. Nested mode = top-level only;
            flat mode = matches full dotted path.
        include_components: Also return components' properties.
        flat: Return flat dotted-path dict instead of nested structure.
        max_depth: (flat mode) Max struct nesting depth. Default 3.
        include_metadata: (flat mode) Replace each value with full metadata object
            (type info, clamps, enum values, etc.).
        expand_arrays: (flat mode) Emit array elements as Field[N] entries.
        array_element_limit: (flat mode) Max elements emitted per array. Default 16.
    """
    return _call("get_actor_properties", {
        "actor_label":         actor_label,
        "filter":              filter,
        "include_components":  include_components,
        "flat":                flat,
        "max_depth":           max_depth,
        "include_metadata":    include_metadata,
        "expand_arrays":       expand_arrays,
        "array_element_limit": array_element_limit,
        "category":            category,
        "include_inherited":   include_inherited,
        "max_entries":         max_entries,
        "cursor":              cursor,
    })


@mcp.tool()
def set_actor_property(
    actor_label: str,
    property_path: str,
    property_value: Any,
    component_name: str = "",
) -> str:
    """Set a property at any nested path on a placed level actor instance.

    Writes to the actual placed actor (not its CDO/Blueprint defaults), so editor
    overrides are preserved. Walks struct fields and array indices, fires
    PostEditChangeProperty on the top-level property, and marks the level dirty.

    Args:
        actor_label: Actor's display label in the Outliner (or its UObject name).
        property_path: Dot-separated property path. Examples:
            "Settings.BloomIntensity"
            "Settings.ColorGradingHighlights.Gain"
            "Tags[0]"
            "Settings.bOverride_BloomIntensity"
        property_value: The value to write. Accepts:
            - Numbers, bools, strings for primitives and enums
            - JSON object for structs ({"R":1.0,"G":0.5,"B":0.0,"A":1.0} for FLinearColor)
            - UE text format string for structs ("(R=1.0,G=0.5,B=0.0,A=1.0)")
            - Asset path string for object/class references
        component_name: Optional. If set, the path is resolved starting from a
            component on the actor instead of the actor itself.

    Returns: actor identification + the resolved top-level FProperty name.
    """
    params: dict = {
        "actor_label":    actor_label,
        "property_path":  property_path,
        "property_value": property_value,
    }
    if component_name:
        params["component_name"] = component_name
    return _call("set_actor_property", params)


@mcp.tool()
def get_actor_property_metadata(
    actor_label: str,
    property_path: str = "",
    filter: str = "",
    category: str = "",
    depth: int = 1,
    expand_enums: bool = True,
    include_inherited: bool = True,
    descend_into_objects: bool = False,
    max_entries: int = 50,
    cursor: int = 0,
    component_name: str = "",
) -> str:
    """Inspect type/clamp/enum metadata for properties on a placed actor.

    Returns a flat dict keyed by dotted path with per-property metadata
    (cpp_type, ue_type, category, display_name, tooltip, current_value, clamp_min/max,
    ui_min/max, valid_values for enums, inner/element/key/value types for containers,
    object_class / meta_class for refs, plus is_struct/is_array/is_enum/is_object
    /is_bool / editable / transient / readonly flags).

    ALSO returns a "_summary" header containing total_available, total_returned,
    truncated, next_cursor, own_count, inherited_count, class_chain, and a
    "categories" map (UPROPERTY Category -> count) — use this to navigate without
    flooding context. max_entries=0 returns ONLY the summary so you can plan first.

    Args:
        actor_label: Outliner label or UObject name.
        property_path: Optional dotted path. Empty = enumerate top-level properties.
            If the path is a struct, enumerates that struct's fields. If a leaf
            scalar/enum, returns metadata for just that property. If an OBJECT
            REFERENCE, returns single-property metadata for the ref + a hint
            (use component_name to inspect the target's properties), unless
            descend_into_objects=True is passed.
        filter: Case-insensitive substring on full dotted path.
        category: Case-insensitive exact match against UPROPERTY(Category="X").
            Sub-categories use "|" — e.g. "Lens|Bloom".
        depth: How many struct levels to expand below the start point. 0 = flat.
        expand_enums: Include valid_values list for enum-typed fields.
        include_inherited: Include properties from super classes. False = only
            those declared on the most-derived class (use to scope down components).
        descend_into_objects: When property_path lands on an FObjectProperty,
            enumerate the target object's class instead of returning a single
            metadata blob. Disabled by default — prefer component_name.
        max_entries: Hard cap on emitted entries. Default 50. 0 = summary only.
            Pair with cursor for pagination.
        cursor: Pagination offset. Use _summary.next_cursor from the previous
            response to fetch the next page.
        component_name: Optional. Anchor target object to a named component.
    """
    params: dict = {
        "actor_label":          actor_label,
        "depth":                depth,
        "expand_enums":         expand_enums,
        "include_inherited":    include_inherited,
        "descend_into_objects": descend_into_objects,
        "max_entries":          max_entries,
        "cursor":               cursor,
    }
    if property_path:
        params["property_path"] = property_path
    if filter:
        params["filter"] = filter
    if category:
        params["category"] = category
    if component_name:
        params["component_name"] = component_name
    return _call("get_actor_property_metadata", params)


@mcp.tool()
def spawn_actor_by_class(
    class_path: str,
    name: str = "",
    location: list[float] | None = None,
    rotation: list[float] | None = None,
    scale: list[float] | None = None,
) -> str:
    """Spawn ANY AActor subclass by full path, short name, or Blueprint asset path.

    Replaces the hardcoded whitelist of spawn_actor. Resolves class_path in this order:
    1. Full UClass path with '.' (e.g. "/Script/Engine.PostProcessVolume",
       "/Script/Engine.SkyAtmosphere")
    2. Blueprint asset path (e.g. "/Game/Blueprints/BP_Miner") — loads the asset and
       uses its GeneratedClass.
    3. Short name fallback — iterates loaded UClasses for a name match
       (handles "PostProcessVolume", "APostProcessVolume", "Sky_Sphere_C", etc.)

    Validates: must be AActor subclass, not abstract, not deprecated.
    Uses UEditorActorSubsystem::SpawnActorFromClass when available so Ctrl+Z restores.

    Args:
        class_path: Full path, BP asset path, or short class name.
        name:       Optional outliner label + UObject name.
        location:   [x, y, z] world location.
        rotation:   [pitch, yaw, roll] degrees.
        scale:      [x, y, z] scale.
    """
    params: dict = {"class_path": class_path}
    if name:
        params["name"] = name
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    return _call("spawn_actor_by_class", params)


@mcp.tool()
def find_actors(
    name_pattern: str = "",
    label_pattern: str = "",
    class_filter: str = "",
    tag: str = "",
    exact_class: bool = False,
    max_results: int = 100,
    include_transform: bool = False,
) -> str:
    """Flexible actor search across the level — combine any number of filters.

    Use this to answer "is X in the world", "first 5 actors of class Y", "all actors
    starting with name Z and tagged W". All filters AND together. Returns total count
    scanned/matched even when results are truncated.

    Args:
        name_pattern: Case-insensitive substring on UObject name (e.g. "Light").
        label_pattern: Case-insensitive substring on outliner display label.
        class_filter: Class name (short or full path). When it resolves to a real
            UClass, uses IsA() (or exact_class for ==). Otherwise falls back to
            substring match on the actor's class name/path.
        tag: Match actors with this FName in their Tags.
        exact_class: When class_filter resolves to a UClass, require exact match
            instead of subclass.
        max_results: Cap on returned entries (0 = unlimited; truncated flag set).
        include_transform: Include location/rotation/scale per entry.
    """
    params: dict = {
        "max_results": max_results,
        "exact_class": exact_class,
        "include_transform": include_transform,
    }
    if name_pattern:
        params["name_pattern"] = name_pattern
    if label_pattern:
        params["label_pattern"] = label_pattern
    if class_filter:
        params["class_filter"] = class_filter
    if tag:
        params["tag"] = tag
    return _call("find_actors", params)
