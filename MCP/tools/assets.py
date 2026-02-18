"""Asset management tools — search, inspect, modify, delete, save, import, duplicate, rename."""

from _bridge import mcp, _send

# ---------------------------------------------------------------------------
# Constant maps
# ---------------------------------------------------------------------------
_ASSET_CLASS_SHORTCUTS = {
    "material":          ("/Script/Engine", "Material"),
    "material_instance": ("/Script/Engine", "MaterialInstanceConstant"),
    "static_mesh":       ("/Script/Engine", "StaticMesh"),
    "skeletal_mesh":     ("/Script/Engine", "SkeletalMesh"),
    "texture":           ("/Script/Engine", "Texture2D"),
    "blueprint":         ("/Script/Engine", "Blueprint"),
    "widget_blueprint":  ("/Script/UMGEditor", "WidgetBlueprint"),
    "data_table":        ("/Script/Engine", "DataTable"),
    "sound_wave":        ("/Script/Engine", "SoundWave"),
    "sound_cue":         ("/Script/Engine", "SoundCue"),
    "particle_system":   ("/Script/Engine", "ParticleSystem"),
    "niagara_system":    ("/Script/Niagara", "NiagaraSystem"),
    "niagara_emitter":   ("/Script/Niagara", "NiagaraEmitter"),
    "anim_blueprint":    ("/Script/Engine", "AnimBlueprint"),
    "anim_sequence":     ("/Script/Engine", "AnimSequence"),
    "anim_montage":      ("/Script/Engine", "AnimMontage"),
    "level":             ("/Script/Engine", "World"),
    "curve_float":       ("/Script/Engine", "CurveFloat"),
    "enum":              ("/Script/Engine", "UserDefinedEnum"),
    "struct":            ("/Script/Engine", "UserDefinedStruct"),
}


def _resolve_class(class_type: str):
    """Resolve a class_type string into (package_path, class_name) or None."""
    lower = class_type.strip().lower()
    if lower in _ASSET_CLASS_SHORTCUTS:
        return _ASSET_CLASS_SHORTCUTS[lower]
    if "." in class_type:
        parts = class_type.rsplit(".", 1)
        return (parts[0], parts[1])
    return None


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

@mcp.tool()
def find_assets(
    class_type: str = "",
    path: str = "",
    name_pattern: str = "",
    recursive: bool = True,
    max_results: int = 200,
) -> str:
    """Power search using UE5 Asset Registry with proper indexed class filtering.

    Args:
        class_type: Shortcut name (e.g. "material", "blueprint", "static_mesh",
            "texture", "data_table", "niagara_system") or "PackagePath.ClassName"
        path: Content path filter (e.g. "/Game/Materials"). Only returns assets under this path.
        name_pattern: Substring match on asset name (case-insensitive)
        recursive: Search subdirectories (default True)
        max_results: Maximum results to return (default 200)
    """
    resolved = _resolve_class(class_type) if class_type else None

    lines = [
        "import unreal",
        "reg = unreal.AssetRegistryHelpers.get_asset_registry()",
    ]

    if resolved:
        pkg, cls = resolved
        lines += [
            f"all_assets = reg.get_assets_by_class(unreal.TopLevelAssetPath({repr(pkg)}, {repr(cls)}))",
            "all_assets = list(all_assets) if all_assets else []",
        ]
    elif path:
        lines += [
            "ar_filter = unreal.ARFilter()",
            f"ar_filter.package_paths = [unreal.Name({repr(path)})]",
            f"ar_filter.recursive_paths = {recursive}",
            "all_assets = reg.get_assets(ar_filter)",
            "all_assets = list(all_assets) if all_assets else []",
        ]
    else:
        lines += [
            "all_assets = []",
        ]

    lines += [
        "matched = []",
        "for ad in all_assets:",
        "    pkg_name = str(ad.package_name)",
        "    asset_name = str(ad.asset_name)",
        "    asset_class = str(ad.asset_class_path.asset_name) if ad.asset_class_path else ''",
    ]

    if resolved and path:
        lines.append(f"    if not pkg_name.startswith({repr(path)}): continue")

    if name_pattern:
        lines.append(f"    if {repr(name_pattern.lower())} not in asset_name.lower(): continue")

    lines += [
        "    matched.append({'name': asset_name, 'path': pkg_name + '.' + asset_name, 'class': asset_class})",
        f"    if len(matched) >= {max_results}: break",
        "result = {'count': len(matched), 'assets': matched}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def list_assets(
    path: str = "/Game",
    class_filter: str = "",
    recursive: bool = True,
) -> str:
    """List assets in a Content Browser directory.

    Args:
        path: Content path (e.g. "/Game", "/Game/Materials")
        class_filter: Optional class shortcut (e.g. "material", "blueprint", "static_mesh").
            Uses Asset Registry for accurate class filtering instead of string matching.
        recursive: Search subdirectories
    """
    if class_filter:
        return find_assets(class_type=class_filter, path=path, recursive=recursive)

    lines = [
        "import unreal",
        f"raw = unreal.EditorAssetLibrary.list_assets({repr(path)}, recursive={recursive})",
        "assets = list(raw) if raw else []",
        "result = {'count': len(assets), 'assets': assets}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def open_asset(asset_path: str) -> str:
    """Open an asset in the UE5 editor (blueprint, material, data table, widget, etc.).

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    lines = [
        "import unreal",
        f"asset = unreal.EditorAssetLibrary.load_asset({repr(asset_path)})",
        "if not asset:",
        f"    result = {{'success': False, 'error': 'Asset not found: {asset_path}'}}",
        "else:",
        "    subsys = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)",
        "    subsys.open_editor_for_assets([asset])",
        f"    result = {{'success': True, 'message': 'Opened {asset_path}'}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def get_asset_info(asset_path: str) -> str:
    """Get comprehensive asset metadata: class, package, disk size, tags, and key properties.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    lines = [
        "import unreal",
        "reg = unreal.AssetRegistryHelpers.get_asset_registry()",
        f"asset = unreal.EditorAssetLibrary.load_asset({repr(asset_path)})",
        "if not asset:",
        f"    result = {{'success': False, 'error': 'Asset not found: {asset_path}'}}",
        "else:",
        "    info = {}",
        "    info['name'] = asset.get_name()",
        "    info['class'] = asset.get_class().get_name()",
        "    info['path'] = asset.get_path_name()",
        "    info['package'] = str(asset.get_outer().get_name()) if asset.get_outer() else ''",
        "    ad = reg.get_asset_by_object_path(asset.get_path_name())",
        "    if ad:",
        "        info['disk_size'] = ad.get_tag_value('Size') if hasattr(ad, 'get_tag_value') else 'N/A'",
        "    props = {}",
        "    for prop in dir(asset):",
        "        if prop.startswith('_'): continue",
        "        try:",
        "            val = asset.get_editor_property(prop)",
        "            props[prop] = str(val)",
        "        except Exception:",
        "            pass",
        "        if len(props) >= 50: break",
        "    info['properties'] = props",
        "    result = {'success': True, 'info': info}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def get_asset_properties(asset_path: str) -> str:
    """Get all editable properties of an asset. Use for deep inspection.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    lines = [
        "import unreal",
        f"asset = unreal.EditorAssetLibrary.load_asset({repr(asset_path)})",
        "if not asset:",
        f"    result = {{'success': False, 'error': 'Asset not found: {asset_path}'}}",
        "else:",
        "    props = {}",
        "    for prop in dir(asset):",
        "        if prop.startswith('_'): continue",
        "        try:",
        "            val = asset.get_editor_property(prop)",
        "            props[prop] = str(val)",
        "        except Exception:",
        "            pass",
        f"    result = {{'class': asset.get_class().get_name(), 'path': {repr(asset_path)}, 'properties': props}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def set_asset_property(
    asset_path: str,
    property_name: str,
    property_value: str,
) -> str:
    """Set a property on an asset. The value is evaluated as Python.

    Args:
        asset_path: Full asset path
        property_name: Property name (e.g. "two_sided", "blend_mode")
        property_value: Python expression (e.g. "True", "0.5", "unreal.BlendMode.BLEND_TRANSLUCENT")
    """
    lines = [
        "import unreal",
        f"asset = unreal.EditorAssetLibrary.load_asset({repr(asset_path)})",
        "if not asset:",
        "    result = 'Error: Asset not found'",
        "else:",
        f"    asset.set_editor_property({repr(property_name)}, {property_value})",
        f"    unreal.EditorAssetLibrary.save_asset({repr(asset_path)})",
        f"    result = 'Set {property_name} = {property_value} on {asset_path}'",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def find_references(
    asset_path: str,
    direction: str = "both",
) -> str:
    """Find all assets that reference or are referenced by a given asset.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
        direction: "dependents" (who uses this), "dependencies" (what this uses), or "both"
    """
    lines = [
        "import unreal",
        "reg = unreal.AssetRegistryHelpers.get_asset_registry()",
        "opts = unreal.AssetRegistryDependencyOptions()",
        "opts.include_soft_package_references = True",
        "opts.include_hard_package_references = True",
        "opts.include_searchable_names = False",
        "opts.include_soft_management_references = False",
        f"pkg = {repr(asset_path.rsplit('.', 1)[0] if '.' in asset_path else asset_path)}",
        "deps = []",
        "refs = []",
    ]

    if direction in ("dependencies", "both"):
        lines += [
            "raw_deps = reg.get_dependencies(unreal.Name(pkg), opts)",
            "if raw_deps:",
            "    deps = [str(d) for d in raw_deps]",
        ]

    if direction in ("dependents", "both"):
        lines += [
            "raw_refs = reg.get_referencers(unreal.Name(pkg), opts)",
            "if raw_refs:",
            "    refs = [str(r) for r in raw_refs]",
        ]

    lines.append("result = {'asset': pkg, 'dependents': refs, 'dependencies': deps}")
    return _send("\n".join(lines))


@mcp.tool()
def duplicate_asset(source_path: str, dest_path: str, dest_name: str) -> str:
    """Duplicate an asset to a new location.

    Args:
        source_path: Full path to source asset (e.g. "/Game/Materials/M_Base")
        dest_path: Destination directory (e.g. "/Game/Materials/Copies")
        dest_name: Name for the duplicate (e.g. "M_Base_Copy")
    """
    full_dest = f"{dest_path}/{dest_name}"
    lines = [
        "import unreal",
        f"if not unreal.EditorAssetLibrary.does_asset_exist({repr(source_path)}):",
        f"    result = {{'success': False, 'error': 'Source not found: {source_path}'}}",
        "else:",
        f"    ok = unreal.EditorAssetLibrary.duplicate_asset({repr(source_path)}, {repr(full_dest)})",
        f"    if ok: result = {{'success': True, 'path': {repr(full_dest)}}}",
        f"    else: result = {{'success': False, 'error': 'Duplicate failed (dest may already exist)'}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def rename_asset(source_path: str, dest_path: str) -> str:
    """Rename or move an asset. UE5 automatically fixes all references.

    Args:
        source_path: Current full asset path (e.g. "/Game/Materials/M_Old")
        dest_path: New full asset path (e.g. "/Game/Materials/M_New")
    """
    lines = [
        "import unreal",
        f"if not unreal.EditorAssetLibrary.does_asset_exist({repr(source_path)}):",
        f"    result = {{'success': False, 'error': 'Asset not found: {source_path}'}}",
        "else:",
        f"    ok = unreal.EditorAssetLibrary.rename_asset({repr(source_path)}, {repr(dest_path)})",
        f"    if ok: result = {{'success': True, 'from': {repr(source_path)}, 'to': {repr(dest_path)}}}",
        f"    else: result = {{'success': False, 'error': 'Rename failed'}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def delete_asset(asset_path: str, force: bool = False) -> str:
    """Delete an asset or directory from the Content Browser.

    When deleting a single asset, checks for references first (unless force=True).
    If asset_path has no dot and ends with a path-like segment, treats it as a directory.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Test") or directory (e.g. "/Game/OldStuff")
        force: Skip reference check and force delete (default False)
    """
    lines = [
        "import unreal",
        "reg = unreal.AssetRegistryHelpers.get_asset_registry()",
        "opts = unreal.AssetRegistryDependencyOptions()",
        "opts.include_soft_package_references = True",
        "opts.include_hard_package_references = True",
        "opts.include_searchable_names = False",
        "opts.include_soft_management_references = False",
    ]

    if not force:
        lines += [
            f"if unreal.EditorAssetLibrary.does_asset_exist({repr(asset_path)}):",
            f"    _refs = reg.get_referencers(unreal.Name({repr(asset_path)}), opts)",
            "    _ref_list = [str(r) for r in _refs] if _refs else []",
            f"    _ref_list = [r for r in _ref_list if '/Script/' not in r and r != {repr(asset_path)}]",
            "    if _ref_list:",
            "        result = {'success': False, 'error': 'Asset has references', 'referencers': _ref_list}",
            "    else:",
            f"        ok = unreal.EditorAssetLibrary.delete_asset({repr(asset_path)})",
            f"        result = {{'success': ok, 'deleted': {repr(asset_path)}}} if ok else {{'success': False, 'error': 'Delete failed'}}",
            f"elif unreal.EditorAssetLibrary.does_directory_exist({repr(asset_path)}):",
            f"    ok = unreal.EditorAssetLibrary.delete_directory({repr(asset_path)})",
            f"    result = {{'success': ok, 'deleted_directory': {repr(asset_path)}}} if ok else {{'success': False, 'error': 'Directory delete failed'}}",
            "else:",
            f"    result = {{'success': False, 'error': 'Not found: {asset_path}'}}",
        ]
    else:
        lines += [
            f"if unreal.EditorAssetLibrary.does_asset_exist({repr(asset_path)}):",
            f"    ok = unreal.EditorAssetLibrary.delete_asset({repr(asset_path)})",
            f"    result = {{'success': ok, 'deleted': {repr(asset_path)}}} if ok else {{'success': False, 'error': 'Delete failed'}}",
            f"elif unreal.EditorAssetLibrary.does_directory_exist({repr(asset_path)}):",
            f"    ok = unreal.EditorAssetLibrary.delete_directory({repr(asset_path)})",
            f"    result = {{'success': ok, 'deleted_directory': {repr(asset_path)}}} if ok else {{'success': False, 'error': 'Directory delete failed'}}",
            "else:",
            f"    result = {{'success': False, 'error': 'Not found: {asset_path}'}}",
        ]
    return _send("\n".join(lines))


@mcp.tool()
def save_asset(asset_path: str) -> str:
    """Save a specific asset to disk.

    Args:
        asset_path: Full asset path
    """
    lines = [
        "import unreal",
        f"if not unreal.EditorAssetLibrary.does_asset_exist({repr(asset_path)}):",
        f"    result = {{'success': False, 'error': 'Asset not found: {asset_path}'}}",
        "else:",
        f"    ok = unreal.EditorAssetLibrary.save_asset({repr(asset_path)})",
        f"    result = {{'success': ok, 'saved': {repr(asset_path)}}} if ok else {{'success': False, 'error': 'Save failed'}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def save_all() -> str:
    """Save all unsaved (dirty) assets."""
    return _send(
        "import unreal\n"
        "unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)\n"
        "result = {'success': True, 'message': 'All dirty assets saved'}"
    )


@mcp.tool()
def import_asset(source_file: str, destination_path: str) -> str:
    """Import an external file into the Content Browser.

    Args:
        source_file: Absolute path to file on disk (e.g. "C:/textures/wood.png")
        destination_path: Content Browser path (e.g. "/Game/Textures")
    """
    escaped = source_file.replace("\\", "/")
    lines = [
        "import unreal",
        "task = unreal.AssetImportTask()",
        f"task.set_editor_property('filename', {repr(escaped)})",
        f"task.set_editor_property('destination_path', {repr(destination_path)})",
        "task.set_editor_property('automated', True)",
        "task.set_editor_property('save', True)",
        "unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])",
        "imported = task.get_editor_property('imported_object_paths')",
        f"result = {{'success': True, 'destination': {repr(destination_path)}, 'imported': list(imported) if imported else [{repr(escaped)}]}}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def get_selected_assets() -> str:
    """Get currently selected assets in the Content Browser."""
    lines = [
        "import unreal",
        "selected = unreal.EditorUtilityLibrary.get_selected_assets()",
        "assets = []",
        "if selected:",
        "    for a in selected:",
        "        assets.append({'name': a.get_name(), 'path': a.get_path_name(), 'class': a.get_class().get_name()})",
        "result = {'count': len(assets), 'assets': assets}",
    ]
    return _send("\n".join(lines))


@mcp.tool()
def sync_browser(asset_path: str) -> str:
    """Navigate the Content Browser to show a specific asset.

    Args:
        asset_path: Full asset path (e.g. "/Game/Materials/M_Base")
    """
    lines = [
        "import unreal",
        f"unreal.EditorAssetLibrary.sync_browser_to_objects([{repr(asset_path)}])",
        f"result = {{'success': True, 'message': 'Synced Content Browser to {asset_path}'}}",
    ]
    return _send("\n".join(lines))
