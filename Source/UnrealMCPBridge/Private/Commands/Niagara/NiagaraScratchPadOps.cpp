#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraCommon.h"
#include "NiagaraTypes.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraSystemEditorData.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraScriptVariable.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/SavePackage.h"

// ---------------------------------------------------------------------------
// HandleCreateNiagaraScratchPadModule
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCreateNiagaraScratchPadModule(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString ModuleName = TEXT("ScratchPadModule");
	Params->TryGetStringField(TEXT("module_name"), ModuleName);

	FString ScriptUsageStr = TEXT("particle_update");
	Params->TryGetStringField(TEXT("script_usage"), ScriptUsageStr);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Get editor data for scratch pad access
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor data on system"));
	}

	// Create a new scratch pad script
	UNiagaraScript* ScratchScript = NewObject<UNiagaraScript>(
		System, FName(*ModuleName), RF_Transactional);

	if (!ScratchScript)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create scratch pad script"));
	}

	ScratchScript->SetUsage(ENiagaraScriptUsage::Module);

	// Create a script source with a graph
	UNiagaraScriptSource* ScriptSource = NewObject<UNiagaraScriptSource>(
		ScratchScript, FName(TEXT("ScriptSource")), RF_Transactional);

	UNiagaraGraph* Graph = NewObject<UNiagaraGraph>(
		ScriptSource, FName(TEXT("NiagaraGraph")), RF_Transactional);

	ScriptSource->NodeGraph = Graph;
	ScratchScript->SetLatestSource(ScriptSource);

	// Create the output node for the scratch pad module
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (Schema)
	{
		// Create Map Get and output nodes via the graph schema
		FGraphNodeCreator<UNiagaraNodeOutput> OutputCreator(*Graph);
		UNiagaraNodeOutput* OutputNode = OutputCreator.CreateNode(false);
		OutputNode->SetUsage(ENiagaraScriptUsage::Module);
		OutputNode->NodePosX = 400;
		OutputNode->NodePosY = 0;
		OutputCreator.Finalize();
	}

	// Add to system's scratch pad scripts array
	System->ScratchPadScripts.Add(ScratchScript);

	System->MarkPackageDirty();
	System->PostEditChange();

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetStringField(TEXT("system_path"), SystemPath);
	Result->SetNumberField(TEXT("scratch_pad_count"), System->ScratchPadScripts.Num());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSetNiagaraScratchPadHlsl
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSetNiagaraScratchPadHlsl(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
	}

	FString ModuleName;
	if (!Params->TryGetStringField(TEXT("module_name"), ModuleName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter"));
	}

	FString HlslCode;
	if (!Params->TryGetStringField(TEXT("hlsl_code"), HlslCode))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'hlsl_code' parameter"));
	}

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
	if (!EditorData)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor data on system"));
	}

	// Find the scratch pad script by name
	UNiagaraScript* TargetScript = nullptr;
	for (UNiagaraScript* Script : System->ScratchPadScripts)
	{
		if (Script && Script->GetName().Equals(ModuleName, ESearchCase::IgnoreCase))
		{
			TargetScript = Script;
			break;
		}
	}

	if (!TargetScript)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found"), *ModuleName));
	}

	// Get the script source and graph
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(TargetScript->GetLatestSource());
	if (!Source || !Source->NodeGraph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Scratch pad module has no valid graph"));
	}

	UNiagaraGraph* Graph = Source->NodeGraph;

	// Find or create a Custom HLSL node in the graph
	TArray<UNiagaraNodeCustomHlsl*> CustomNodes;
	Graph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(CustomNodes);

	UNiagaraNodeCustomHlsl* HlslNode = nullptr;
	if (CustomNodes.Num() > 0)
	{
		HlslNode = CustomNodes[0];
	}
	else
	{
		// Create a new Custom HLSL node
		FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*Graph);
		HlslNode = Creator.CreateNode(false);
		HlslNode->NodePosX = 0;
		HlslNode->NodePosY = 0;
		Creator.Finalize();
	}

	// Set the HLSL code via reflection — NiagaraEditor methods are not exported
	FStrProperty* HlslProp = CastField<FStrProperty>(
		UNiagaraNodeCustomHlsl::StaticClass()->FindPropertyByName(TEXT("CustomHlsl")));
	if (HlslProp)
	{
		HlslProp->SetPropertyValue_InContainer(HlslNode, HlslCode);
	}
	HlslNode->MarkNodeRequiresSynchronization(TEXT("HLSL set via MCP"), true);

	// Parse inputs from JSON if provided
	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("inputs"), InputsArray))
	{
		for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
		{
			const TSharedPtr<FJsonObject>* InputObj = nullptr;
			if (!InputVal->TryGetObject(InputObj))
			{
				continue;
			}

			FString InputName;
			FString InputType = TEXT("float");
			(*InputObj)->TryGetStringField(TEXT("name"), InputName);
			(*InputObj)->TryGetStringField(TEXT("type"), InputType);

			if (InputName.IsEmpty())
			{
				continue;
			}

			// Add the input pin to the custom HLSL node
			FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetFloatDef();
			FString LowerType = InputType.ToLower();
			if (LowerType == TEXT("float"))
			{
				TypeDef = FNiagaraTypeDefinition::GetFloatDef();
			}
			else if (LowerType == TEXT("int") || LowerType == TEXT("int32"))
			{
				TypeDef = FNiagaraTypeDefinition::GetIntDef();
			}
			else if (LowerType == TEXT("bool"))
			{
				TypeDef = FNiagaraTypeDefinition::GetBoolDef();
			}
			else if (LowerType == TEXT("vector") || LowerType == TEXT("vec3"))
			{
				TypeDef = FNiagaraTypeDefinition::GetVec3Def();
			}
			else if (LowerType == TEXT("vector4") || LowerType == TEXT("vec4"))
			{
				TypeDef = FNiagaraTypeDefinition::GetVec4Def();
			}
			else if (LowerType == TEXT("color") || LowerType == TEXT("linearcolor"))
			{
				TypeDef = FNiagaraTypeDefinition::GetColorDef();
			}

			// Custom HLSL inputs are declared in the HLSL code itself
			// The node auto-creates pins from the HLSL parameter declarations
			// Nothing to do here — inputs are parsed from the HLSL code
		}
	}

	// Parse outputs from JSON if provided
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("outputs"), OutputsArray))
	{
		for (const TSharedPtr<FJsonValue>& OutputVal : *OutputsArray)
		{
			const TSharedPtr<FJsonObject>* OutputObj = nullptr;
			if (!OutputVal->TryGetObject(OutputObj))
			{
				continue;
			}

			FString OutputName;
			FString OutputType = TEXT("float");
			(*OutputObj)->TryGetStringField(TEXT("name"), OutputName);
			(*OutputObj)->TryGetStringField(TEXT("type"), OutputType);

			if (OutputName.IsEmpty())
			{
				continue;
			}

			FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetFloatDef();
			FString LowerType = OutputType.ToLower();
			if (LowerType == TEXT("float"))
			{
				TypeDef = FNiagaraTypeDefinition::GetFloatDef();
			}
			else if (LowerType == TEXT("int") || LowerType == TEXT("int32"))
			{
				TypeDef = FNiagaraTypeDefinition::GetIntDef();
			}
			else if (LowerType == TEXT("bool"))
			{
				TypeDef = FNiagaraTypeDefinition::GetBoolDef();
			}
			else if (LowerType == TEXT("vector") || LowerType == TEXT("vec3"))
			{
				TypeDef = FNiagaraTypeDefinition::GetVec3Def();
			}
			else if (LowerType == TEXT("vector4") || LowerType == TEXT("vec4"))
			{
				TypeDef = FNiagaraTypeDefinition::GetVec4Def();
			}
			else if (LowerType == TEXT("color") || LowerType == TEXT("linearcolor"))
			{
				TypeDef = FNiagaraTypeDefinition::GetColorDef();
			}

			// Custom HLSL outputs are declared in the HLSL code itself
			// The node auto-creates pins from the HLSL parameter declarations
		}
	}

	Graph->NotifyGraphChanged();
	NiagaraHelpers::CompileAndSync(System);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("module_name"), ModuleName);
	Result->SetNumberField(TEXT("hlsl_length"), HlslCode.Len());
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleCreateNiagaraModuleAsset
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleCreateNiagaraModuleAsset(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	FString ModuleType = TEXT("module");
	Params->TryGetStringField(TEXT("module_type"), ModuleType);

	// Parse path
	FString PackagePath;
	FString AssetName;
	int32 LastSlash = INDEX_NONE;
	if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash <= 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Invalid asset_path format. Expected '/Game/Path/AssetName'"));
	}
	PackagePath = AssetPath.Left(LastSlash);
	AssetName = AssetPath.Mid(LastSlash + 1);

	if (AssetName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset name is empty"));
	}

	// Create the package
	FString FullPackagePath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to create package at '%s'"), *FullPackagePath));
	}
	Package->FullyLoad();

	// Create the script
	UNiagaraScript* NewScript = NewObject<UNiagaraScript>(
		Package, FName(*AssetName), RF_Public | RF_Standalone);
	if (!NewScript)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Niagara script"));
	}

	// Set usage based on module type
	FString LowerModuleType = ModuleType.ToLower();
	if (LowerModuleType == TEXT("module"))
	{
		NewScript->SetUsage(ENiagaraScriptUsage::Module);
	}
	else if (LowerModuleType == TEXT("dynamic_input"))
	{
		NewScript->SetUsage(ENiagaraScriptUsage::DynamicInput);
	}
	else if (LowerModuleType == TEXT("function"))
	{
		NewScript->SetUsage(ENiagaraScriptUsage::Function);
	}
	else
	{
		NewScript->SetUsage(ENiagaraScriptUsage::Module);
	}

	// Create script source and graph
	UNiagaraScriptSource* ScriptSource = NewObject<UNiagaraScriptSource>(
		NewScript, FName(TEXT("ScriptSource")), RF_Transactional);

	UNiagaraGraph* Graph = NewObject<UNiagaraGraph>(
		ScriptSource, FName(TEXT("NiagaraGraph")), RF_Transactional);

	ScriptSource->NodeGraph = Graph;
	NewScript->SetLatestSource(ScriptSource);

	// Create output node
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (Schema)
	{
		FGraphNodeCreator<UNiagaraNodeOutput> OutputCreator(*Graph);
		UNiagaraNodeOutput* OutputNode = OutputCreator.CreateNode(false);
		OutputNode->SetUsage(NewScript->GetUsage());
		OutputNode->NodePosX = 400;
		OutputNode->NodePosY = 0;
		OutputCreator.Finalize();
	}

	// Set description if provided — use SetPropertyByName to avoid forward-decl issues in unity builds
	FString DescriptionStr;
	if (Params->TryGetStringField(TEXT("description"), DescriptionStr))
	{
		FTextProperty* DescProp = CastField<FTextProperty>(
			UNiagaraScript::StaticClass()->FindPropertyByName(TEXT("Description")));
		if (DescProp)
		{
			DescProp->SetPropertyValue_InContainer(NewScript, FText::FromString(DescriptionStr));
		}
	}

	// Mark and save
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewScript);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		FullPackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewScript, *PackageFileName, SaveArgs);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), NewScript->GetPathName());
	Result->SetStringField(TEXT("asset_name"), AssetName);
	Result->SetStringField(TEXT("module_type"), ModuleType);
	Result->SetStringField(TEXT("package_path"), PackagePath);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
