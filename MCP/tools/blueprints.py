"""Blueprint tools — create blueprints and add components."""

import textwrap

from _bridge import mcp, _send


@mcp.tool()
def create_blueprint(
    name: str,
    path: str = "/Game/Blueprints",
    parent_class: str = "Actor",
) -> str:
    """Create a new Blueprint asset.

    Args:
        name: Blueprint name (e.g. "BP_MyActor")
        path: Content Browser path
        parent_class: Parent class (Actor, Pawn, Character, PlayerController, GameModeBase, etc.)
    """
    code = textwrap.dedent(f"""\
        import unreal
        factory = unreal.BlueprintFactory()
        parent = getattr(unreal, '{parent_class}', None)
        if not parent:
            result = 'Error: Class unreal.{parent_class} not found'
        else:
            factory.set_editor_property('parent_class', parent.static_class())
            asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
            bp = asset_tools.create_asset('{name}', '{path}', unreal.Blueprint, factory)
            if not bp:
                result = 'Error: Failed to create blueprint (may already exist)'
            else:
                unreal.EditorAssetLibrary.save_asset('{path}/{name}')
                result = 'Created blueprint: {path}/{name} (parent: {parent_class})'
    """)
    return _send(code)


@mcp.tool()
def add_component_to_blueprint(
    blueprint_path: str,
    component_class: str,
    component_name: str = "",
) -> str:
    """Add a component to a Blueprint's default components.

    Args:
        blueprint_path: Full path to blueprint (e.g. "/Game/Blueprints/BP_MyActor")
        component_class: Component class (e.g. "StaticMeshComponent", "PointLightComponent")
        component_name: Optional custom name for the component
    """
    name_str = f"'{component_name}'" if component_name else "None"
    code = textwrap.dedent(f"""\
        import unreal
        bp = unreal.EditorAssetLibrary.load_asset('{blueprint_path}')
        if not bp:
            result = 'Error: Blueprint not found'
        else:
            subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
            root_data_handles = subsystem.gather_subobject_data(bp)
            root_handle = root_data_handles[0] if root_data_handles else None
            comp_class = getattr(unreal, '{component_class}', None)
            if not comp_class:
                result = 'Error: Component class unreal.{component_class} not found'
            else:
                params = unreal.AddNewSubobjectParams()
                params.set_editor_property('parent_handle', root_handle)
                params.set_editor_property('new_class', comp_class)
                params.set_editor_property('blueprint_context', bp)
                new_handle, fail = subsystem.add_new_subobject(params)
                if fail:
                    result = f'Error: {{fail}}'
                else:
                    name = {name_str}
                    if name:
                        data = subsystem.find_subobject_data_from_handle(new_handle)
                        if data:
                            subsystem.rename_subobject(new_handle, name)
                    unreal.EditorAssetLibrary.save_asset('{blueprint_path}')
                    result = 'Added {component_class} to {blueprint_path}'
    """)
    return _send(code)
