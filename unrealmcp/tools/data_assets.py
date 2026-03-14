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
