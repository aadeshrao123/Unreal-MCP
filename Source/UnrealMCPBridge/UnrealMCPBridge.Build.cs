using UnrealBuildTool;

public class UnrealMCPBridge : ModuleRules
{
	public UnrealMCPBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDefinitions.Add("UNREALMCPBRIDGE_EXPORTS=1");

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Public"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Nodes"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Function"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Profiling"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Niagara")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Nodes"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Function"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Material"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Profiling"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Niagara"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/StateTree"),
				// Niagara editor private headers — we need NiagaraNodeParameterMapGet/Set
				// and NiagaraParameterMapHistory, which live under Private/ despite being
				// referenced by several public headers. UBT lets a downstream module see
				// an upstream module's Private path when we list it here explicitly.
				System.IO.Path.Combine(EngineDirectory, "Plugins/FX/Niagara/Source/NiagaraEditor/Private")
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"PhysicsCore",
				"RHI",
				"UnrealEd",
				"BlueprintGraph",
				"KismetCompiler"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"Kismet",
				"Projects",
				"AssetRegistry",
				"PythonScriptPlugin",
				"RenderCore",
				"MaterialEditor",
				"AssetTools",
				"DataTableEditor",
				"StructUtils",
				"UMG",
				"UMGEditor",
				"LevelEditor",
				"ImageWrapper",
				"EnhancedInput",
				"TraceAnalysis",
				"TraceServices",
				"TraceLog",
				"Niagara",
				"NiagaraEditor",
				"NiagaraShader",
				"StateTreeModule",
				"StateTreeEditorModule",
				"GameplayTags",
				"PropertyBindingUtils"
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"ToolMenus",
					"BlueprintEditorLibrary",
					"Blutility"
				}
			);
		}
	}
}
