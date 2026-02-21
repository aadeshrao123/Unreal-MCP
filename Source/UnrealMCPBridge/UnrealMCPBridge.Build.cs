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
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Function")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Nodes"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Function")
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
				"MaterialEditor",
				"AssetTools",
				"DataTableEditor",
				"StructUtils",
				"UMG",
				"UMGEditor",
				"LevelEditor",
				"ImageWrapper"
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
