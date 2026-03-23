"""Data Asset tools — create and modify any UDataAsset / UPrimaryDataAsset subclass."""

from unrealmcp._bridge import mcp
from unrealmcp._tcp_bridge import _call


@mcp.tool()
def list_data_asset_classes(
    filter: str = "",
    include_abstract: bool = False,
) -> str:
    """List all UDataAsset subclasses currently loaded in the editor.

    Returns name, full class path, parent class, whether it is a
    UPrimaryDataAsset, and the number of editable properties.
    """
    return _call("list_data_asset_classes", {
        "filter": filter,
        "include_abstract": include_abstract,
    })


@mcp.tool()
def create_data_asset(
    class_name: str,
    asset_path: str,
    initial_properties: dict | None = None,
) -> str:
    """Create a new data asset of any UDataAsset subclass.

    Args:
        class_name: Short name ("ItemData") or full path ("/Script/Jiggify.UItemData")
        asset_path: Full content path including name (e.g. "/Game/Data/Items/DA_IronOre")
        initial_properties: Optional dict of property values to set after creation.
            Supports primitives, structs, arrays, object paths, gameplay tags, etc.
    """
    params = {
        "class_name": class_name,
        "asset_path": asset_path,
    }
    if initial_properties:
        params["initial_properties"] = initial_properties
    return _call("create_data_asset", params)


@mcp.tool()
def get_data_asset_properties(
    asset_path: str,
    filter: str = "",
    include_inherited: bool = True,
) -> str:
    """Read ALL property values from a data asset (no visibility filter).

    Args:
        filter: Substring to filter property names (case-insensitive)
        include_inherited: Include properties from parent C++ classes (default True)
    """
    return _call("get_data_asset_properties", {
        "asset_path":        asset_path,
        "filter":            filter,
        "include_inherited": include_inherited,
    })


@mcp.tool()
def set_data_asset_property(asset_path: str, property_name: str, property_value) -> str:
    """Set a single property on a data asset.

    Supports all types: primitives, enums, FName, FText, structs ({"tagName": "..."}),
    arrays, object refs ("/Game/..."), soft refs, and None (clears reference).

    property_name is case-sensitive (exact FProperty name).

    **Mass Entity Config — Editing Trait Properties:**
    To edit trait properties on a MassEntityConfigAsset (e.g., BeltSpeed, SlotCount),
    set property_name="Config" with a JSON value containing the Traits array.
    Each trait needs a "_ClassName" with FULL path "/Script/Module.ClassName".

    Example — set BeltSpeed on a conveyor config:
        set_data_asset_property(
            asset_path="/Game/.../DA_ConveyorMassConfig_Mk2",
            property_name="Config",
            property_value={
                "Traits": [{"_ClassName": "/Script/Jiggify.MassConveyorBeltTrait", "BeltSpeed": 200}]
            }
        )

    IMPORTANT: _ClassName MUST be full path ("/Script/Jiggify.MassConveyorBeltTrait"),
    NOT short name ("MassConveyorBeltTrait"). Short names create wrong base class.
    For configs with multiple traits, include ALL traits in the array (entire array is replaced).
    Only set the properties you want to change — others use C++ defaults.
    """
    return _call("set_data_asset_property", {
        "asset_path":     asset_path,
        "property_name":  property_name,
        "property_value": property_value,
    })


@mcp.tool()
def set_data_asset_properties(asset_path: str, properties: dict) -> str:
    """Set multiple properties in a single call (one load + one save).

    More efficient than calling set_data_asset_property in a loop.
    Same value format as set_data_asset_property.
    """
    return _call("set_data_asset_properties", {
        "asset_path": asset_path,
        "properties": properties,
    })


@mcp.tool()
def get_property_valid_types(
    class_name: str,
    property_path: str,
    filter: str = "",
    include_abstract: bool = False,
) -> str:
    """Return valid classes/structs/enum-values for a property dropdown.

    Mirrors the exact enumeration the Unreal Details panel uses:
    - Instanced UObject* / TArray<UObject*>  -> GetDerivedClasses(PropertyClass)
    - TSubclassOf<T>                         -> GetDerivedClasses(MetaClass)
    - TArray<FInstancedStruct>               -> UScriptStruct descendants of BaseStruct
    - Enum properties                        -> all named entries + values

    Supports dot-notation: property_path="Config.Traits" navigates into nested structs.

    IMPORTANT: Use 'filter' to avoid huge responses — without it, properties like
    Fragments return 200+ entries (~10k tokens).

    Args:
        class_name: Class or struct name (e.g. "MassAssortedFragmentsTrait")
        property_path: Dot-separated path (e.g. "Config.Traits", "Fragments")
        filter: Case-insensitive substring filter on name and path
        include_abstract: Include abstract base classes (default False)
    """
    return _call("get_property_valid_types", {
        "class_name":       class_name,
        "property_path":    property_path,
        "filter":           filter,
        "include_abstract": include_abstract,
    })


@mcp.tool()
def search_class_paths(
    filter: str = "",
    parent_class: str = "",
    max_results: int = 50,
    include_properties: bool = False,
) -> str:
    """Search for UClass types by name filter and parent class constraint.
    Returns full /Script/Module.ClassName paths needed for _ClassName in trait editing.

    Use this to find the correct class_path before calling set_data_asset_property
    with Mass Entity Config traits.

    Args:
        filter: Case-insensitive substring match on class name (e.g. "Conveyor", "Miner")
        parent_class: Only return subclasses of this class (e.g. "MassEntityTraitBase"
            for traits, "MassFragment" for fragments, "MassTag" for tags,
            "MassProcessor" for processors). Empty = search all classes.
        max_results: Maximum results to return (default 50)
        include_properties: If true, also return each class's editable UPROPERTY names,
            types, and default values. Use sparingly — adds tokens per result.

    Examples:
        search_class_paths(filter="Conveyor", parent_class="MassEntityTraitBase")
        → finds MassConveyorBeltTrait with class_path "/Script/Jiggify.MassConveyorBeltTrait"

        search_class_paths(filter="Pipeline", parent_class="MassFragment")
        → finds all pipeline-related fragments

        search_class_paths(parent_class="MassEntityTraitBase")
        → lists ALL available traits (no filter)
    """
    return _call("search_class_paths", {
        "filter":             filter,
        "parent_class":       parent_class,
        "max_results":        max_results,
        "include_properties": include_properties,
    })


@mcp.tool()
def get_mass_config_traits(asset_path: str) -> str:
    """Read all traits from a Mass Entity Config asset with their properties expanded.

    Unlike get_data_asset_properties (which shows trait object paths), this tool
    expands each trait's editable properties with current values.

    Args:
        asset_path: Path to MassEntityConfigAsset (e.g. "/Game/.../DA_ConveyorMassConfig_Mk1")

    Returns per trait: index, name, class, class_path (for _ClassName), and all
    editable properties with current values.
    """
    return _call("get_mass_config_traits", {
        "asset_path": asset_path,
    })


@mcp.tool()
def add_mass_config_trait(
    asset_path: str,
    trait_class: str,
    properties: dict | None = None,
) -> str:
    """Add a SINGLE trait to a Mass Entity Config without replacing existing traits.

    Safe for configs with complex replicator settings — only appends, never replaces.
    Returns error if the trait already exists on the config.

    Args:
        asset_path: Path to MassEntityConfigAsset (e.g. "/Game/.../DA_MinerMassEntityConfig_Mk1")
        trait_class: Full class path (e.g. "/Script/Jiggify.MassBuildingHealthTrait")
        properties: Optional dict of property values to set on the new trait.
            Supports floats, ints, bools, strings.
            Example: {"MaxHealth": 150, "Resistance_Fire": 0.2}
    """
    params = {
        "asset_path": asset_path,
        "trait_class": trait_class,
    }
    if properties is not None:
        params["properties"] = properties
    return _call("add_mass_config_trait", params)


@mcp.tool()
def list_data_assets(
    path: str = "/Game",
    class_filter: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """List data assets in a content path, optionally filtered by class.

    Args:
        path: Content Browser path (e.g. "/Game/Data")
        class_filter: Class name filter (e.g. "UItemData"). Matches any UDataAsset if empty.
    """
    return _call("list_data_assets", {
        "path":         path,
        "class_filter": class_filter,
        "recursive":    recursive,
        "max_results":  max_results,
    })
