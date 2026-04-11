#include "Commands/EpicUnrealMCPNiagaraCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "NiagaraHelpers.h"
#include "NiagaraTypeHelpers.h"
#include "NiagaraPropertyIntrospection.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"
#include "NiagaraCommon.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeCustomHlsl.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraActions.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraEmitterHandle.h"
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "UObject/UObjectIterator.h"
#include "UObject/MetaData.h"


// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
namespace
{

bool MatchesFilter(const FString& Filter, std::initializer_list<const FString*> Fields)
{
	if (Filter.IsEmpty()) return true;
	for (const FString* Field : Fields)
	{
		if (Field && Field->Contains(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FString ScriptUsageToCategory(ENiagaraScriptUsage Usage)
{
	switch (Usage)
	{
	case ENiagaraScriptUsage::Module:             return TEXT("Module Script");
	case ENiagaraScriptUsage::DynamicInput:       return TEXT("Dynamic Input Script");
	case ENiagaraScriptUsage::Function:           return TEXT("Function Script");
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleUpdateScript:
	case ENiagaraScriptUsage::EmitterSpawnScript:
	case ENiagaraScriptUsage::EmitterUpdateScript:
	case ENiagaraScriptUsage::SystemSpawnScript:
	case ENiagaraScriptUsage::SystemUpdateScript: return TEXT("Emitter/System Script");
	default:                                      return TEXT("Script");
	}
}

/** Read common CDO metadata (tooltip/keywords/category) off a UClass default. */
void GatherClassMeta(UClass* Cls, FString& OutDisplayName, FString& OutTooltip, FString& OutCategory, FString& OutKeywords)
{
	if (!Cls) return;
	OutDisplayName = Cls->GetDisplayNameText().ToString();
	if (OutDisplayName.IsEmpty()) OutDisplayName = Cls->GetName();
#if WITH_EDITORONLY_DATA
	if (Cls->HasMetaData(TEXT("ToolTip"))) OutTooltip = Cls->GetMetaData(TEXT("ToolTip"));
	if (Cls->HasMetaData(TEXT("Category"))) OutCategory = Cls->GetMetaData(TEXT("Category"));
	if (Cls->HasMetaData(TEXT("Keywords"))) OutKeywords = Cls->GetMetaData(TEXT("Keywords"));
#endif
}

/** Strip the "UNiagaraNode" / "U" prefix from a UClass name to produce a short type name. */
FString ShortNodeTypeName(const UClass* Cls)
{
	if (!Cls) return FString();
	FString Name = Cls->GetName();
	if (Name.StartsWith(TEXT("NiagaraNode"))) Name = Name.RightChop(FString(TEXT("NiagaraNode")).Len());
	return Name;
}

/** Resolve a node short name (e.g. "CustomHlsl", "ParameterMapGet") to its UClass. */
UClass* FindNiagaraNodeClass(const FString& ShortOrFullName)
{
	if (ShortOrFullName.IsEmpty()) return nullptr;

	UClass* Exact = FindObject<UClass>(nullptr, *ShortOrFullName);
	if (Exact && Exact->IsChildOf<UNiagaraNode>()) return Exact;

	const FString WithPrefix = FString::Printf(TEXT("NiagaraNode%s"), *ShortOrFullName);
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (!Cls->IsChildOf<UNiagaraNode>()) continue;
		if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
		const FString Name = Cls->GetName();
		if (Name.Equals(ShortOrFullName, ESearchCase::IgnoreCase) ||
			Name.Equals(WithPrefix, ESearchCase::IgnoreCase))
		{
			return Cls;
		}
	}
	return nullptr;
}

/** Build the schema documentation block for a Custom HLSL node (like Material's get_expression_type_info). */
TSharedPtr<FJsonObject> BuildCustomHlslSchema()
{
	auto Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("description"),
		TEXT("Inserts raw HLSL into the Niagara translator. Pins are typed ports referenced in HLSL by name."));

	TArray<TSharedPtr<FJsonValue>> Fields;
	{
		auto F = MakeShared<FJsonObject>();
		F->SetStringField(TEXT("name"), TEXT("hlsl_code"));
		F->SetStringField(TEXT("type"), TEXT("string"));
		F->SetStringField(TEXT("description"), TEXT("HLSL source. Reference pins by name (e.g. Output = Input * 2.0f;)."));
		Fields.Add(MakeShared<FJsonValueObject>(F));
	}
	{
		auto F = MakeShared<FJsonObject>();
		F->SetStringField(TEXT("name"), TEXT("inputs"));
		F->SetStringField(TEXT("type"), TEXT("array<{name,type}>"));
		F->SetStringField(TEXT("description"), TEXT("Typed input pins. 'type' accepts float, int, bool, vec2/3/4, color, quat, matrix, position, ParameterMap, or any registered Niagara type."));
		Fields.Add(MakeShared<FJsonValueObject>(F));
	}
	{
		auto F = MakeShared<FJsonObject>();
		F->SetStringField(TEXT("name"), TEXT("outputs"));
		F->SetStringField(TEXT("type"), TEXT("array<{name,type}>"));
		F->SetStringField(TEXT("description"), TEXT("Typed output pins. Same type vocabulary as inputs."));
		Fields.Add(MakeShared<FJsonValueObject>(F));
	}
	Schema->SetArrayField(TEXT("fields"), Fields);

	auto Example = MakeShared<FJsonObject>();
	Example->SetStringField(TEXT("hlsl_code"), TEXT("Result = Value * Scale;"));
	TArray<TSharedPtr<FJsonValue>> ExIn;
	{
		auto A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("name"), TEXT("Value")); A->SetStringField(TEXT("type"), TEXT("vec3"));
		ExIn.Add(MakeShared<FJsonValueObject>(A));
		auto B = MakeShared<FJsonObject>(); B->SetStringField(TEXT("name"), TEXT("Scale")); B->SetStringField(TEXT("type"), TEXT("float"));
		ExIn.Add(MakeShared<FJsonValueObject>(B));
	}
	Example->SetArrayField(TEXT("inputs"), ExIn);
	TArray<TSharedPtr<FJsonValue>> ExOut;
	{
		auto A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("name"), TEXT("Result")); A->SetStringField(TEXT("type"), TEXT("vec3"));
		ExOut.Add(MakeShared<FJsonValueObject>(A));
	}
	Example->SetArrayField(TEXT("outputs"), ExOut);
	Schema->SetObjectField(TEXT("example"), Example);
	return Schema;
}

/** Enumerate pins of a function script asset as JSON arrays. */
void SerializeFunctionScriptPins(
	UNiagaraScript* Script,
	TArray<TSharedPtr<FJsonValue>>& OutInputs,
	TArray<TSharedPtr<FJsonValue>>& OutOutputs)
{
	if (!Script) return;
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
	if (!Source || !Source->NodeGraph) return;

	UNiagaraNodeOutput* OutputNode = Source->NodeGraph->FindEquivalentOutputNode(Script->GetUsage());
	if (!OutputNode) return;

	for (const FNiagaraVariable& Var : OutputNode->Outputs)
	{
		auto J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"), Var.GetName().ToString());
		J->SetStringField(TEXT("type"), NiagaraTypeHelpers::TypeDefToString(Var.GetType()));
		OutOutputs.Add(MakeShared<FJsonValueObject>(J));
	}

	// Inputs are the script's parameters with Module/Input namespace.
	// GetAllVariables is not NIAGARAEDITOR_API exported; use GetAllMetaData() instead.
	const UNiagaraGraph::FScriptVariableMap& MetaMap = Source->NodeGraph->GetAllMetaData();
	for (const TPair<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& Pair : MetaMap)
	{
		const FNiagaraVariable& Var = Pair.Key;
		const FString NameStr = Var.GetName().ToString();
		if (!NameStr.StartsWith(TEXT("Module.")) && !NameStr.StartsWith(TEXT("Input."))) continue;
		auto J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"), NameStr);
		J->SetStringField(TEXT("type"), NiagaraTypeHelpers::TypeDefToString(Var.GetType()));
		OutInputs.Add(MakeShared<FJsonValueObject>(J));
	}
}

} // namespace
#endif // WITH_EDITORONLY_DATA

// ---------------------------------------------------------------------------
// HandleListNiagaraNodeTypes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleListNiagaraNodeTypes(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString KindFilter = TEXT("all");
	Params->TryGetStringField(TEXT("kind"), KindFilter);
	const FString KindLower = KindFilter.ToLower();

	bool bIncludeEngine = false;
	Params->TryGetBoolField(TEXT("include_engine"), bIncludeEngine);

	int32 MaxResults = 500;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);

	const bool bWantNodeClass = (KindLower == TEXT("all") || KindLower == TEXT("node_class"));
	const bool bWantModule    = (KindLower == TEXT("all") || KindLower == TEXT("module_script"));
	const bool bWantDynIn     = (KindLower == TEXT("all") || KindLower == TEXT("dynamic_input_script"));
	const bool bWantFunc      = (KindLower == TEXT("all") || KindLower == TEXT("function_script"));
	const bool bWantDI        = (KindLower == TEXT("all") || KindLower == TEXT("data_interface"));

	TArray<TSharedPtr<FJsonValue>> Results;

	// ---- 1. Node classes (UNiagaraNode subclasses) ----
	if (bWantNodeClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			UClass* Cls = *It;
			if (!Cls->IsChildOf<UNiagaraNode>()) continue;
			if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_HideDropDown)) continue;

			FString DisplayName, Tooltip, Category, Keywords;
			GatherClassMeta(Cls, DisplayName, Tooltip, Category, Keywords);
			const FString ShortName = ShortNodeTypeName(Cls);

			if (!MatchesFilter(Filter, { &ShortName, &DisplayName, &Category, &Keywords, &Tooltip }))
				continue;

			auto Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("kind"), TEXT("node_class"));
			Entry->SetStringField(TEXT("type"), ShortName);
			Entry->SetStringField(TEXT("class_path"), Cls->GetPathName());
			Entry->SetStringField(TEXT("display_name"), DisplayName);
			if (!Category.IsEmpty()) Entry->SetStringField(TEXT("category"), Category);
			if (!Tooltip.IsEmpty())  Entry->SetStringField(TEXT("tooltip"), Tooltip);
			if (!Keywords.IsEmpty()) Entry->SetStringField(TEXT("keywords"), Keywords);
			Results.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	// ---- 2. Script assets by usage (module / dynamic_input / function) ----
	if (bWantModule || bWantDynIn || bWantFunc)
	{
		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

		FARFilter ARFilter;
		ARFilter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());
		ARFilter.bRecursivePaths = true;
		if (!bIncludeEngine)
		{
			ARFilter.PackagePaths.Add(FName(TEXT("/Game")));
			ARFilter.PackagePaths.Add(FName(TEXT("/Niagara")));
		}

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(ARFilter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (Results.Num() >= MaxResults) break;

			// Usage is exposed as an asset registry tag "Usage"
			FString UsageStr;
			Asset.GetTagValue(TEXT("Usage"), UsageStr);

			bool bWanted = false;
			FString KindLabel;
			if (bWantModule && UsageStr.Contains(TEXT("Module")))         { bWanted = true; KindLabel = TEXT("module_script"); }
			else if (bWantDynIn && UsageStr.Contains(TEXT("DynamicInput"))) { bWanted = true; KindLabel = TEXT("dynamic_input_script"); }
			else if (bWantFunc && UsageStr.Contains(TEXT("Function")))    { bWanted = true; KindLabel = TEXT("function_script"); }

			// Fallback: if tag is missing, load to inspect usage
			if (!bWanted && UsageStr.IsEmpty())
			{
				UNiagaraScript* Script = Cast<UNiagaraScript>(Asset.GetAsset());
				if (!Script) continue;
				const ENiagaraScriptUsage Usage = Script->GetUsage();
				if      (bWantModule && Usage == ENiagaraScriptUsage::Module)       { bWanted = true; KindLabel = TEXT("module_script"); }
				else if (bWantDynIn && Usage == ENiagaraScriptUsage::DynamicInput) { bWanted = true; KindLabel = TEXT("dynamic_input_script"); }
				else if (bWantFunc && Usage == ENiagaraScriptUsage::Function)     { bWanted = true; KindLabel = TEXT("function_script"); }
			}
			if (!bWanted) continue;

			const FString AssetName = Asset.AssetName.ToString();
			const FString AssetPath = Asset.GetObjectPathString();
			const FString Category = FPackageName::GetLongPackagePath(Asset.PackageName.ToString());
			const FString Empty;

			if (!MatchesFilter(Filter, { &AssetName, &AssetPath, &Category, &Empty, &Empty }))
				continue;

			auto Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("kind"), KindLabel);
			Entry->SetStringField(TEXT("name"), AssetName);
			Entry->SetStringField(TEXT("script_path"), AssetPath);
			Entry->SetStringField(TEXT("category"), Category);
			Results.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	// ---- 3. Data Interfaces (UNiagaraDataInterface subclasses) ----
	if (bWantDI)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (Results.Num() >= MaxResults) break;
			UClass* Cls = *It;
			if (!Cls->IsChildOf<UNiagaraDataInterface>()) continue;
			if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;

			FString DisplayName, Tooltip, Category, Keywords;
			GatherClassMeta(Cls, DisplayName, Tooltip, Category, Keywords);
			const FString ShortName = Cls->GetName();

			if (!MatchesFilter(Filter, { &ShortName, &DisplayName, &Category, &Keywords, &Tooltip }))
				continue;

			auto Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("kind"), TEXT("data_interface"));
			Entry->SetStringField(TEXT("type"), ShortName);
			Entry->SetStringField(TEXT("class_path"), Cls->GetPathName());
			Entry->SetStringField(TEXT("display_name"), DisplayName);
			if (!Category.IsEmpty()) Entry->SetStringField(TEXT("category"), Category);
			if (!Tooltip.IsEmpty())  Entry->SetStringField(TEXT("tooltip"), Tooltip);
			Results.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}

	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("filter"), Filter);
	Out->SetStringField(TEXT("kind"), KindFilter);
	Out->SetNumberField(TEXT("count"), Results.Num());
	Out->SetArrayField(TEXT("nodes"), Results);
	return Out;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraNodeTypeInfo
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraNodeTypeInfo(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString TypeName;
	if (!Params->TryGetStringField(TEXT("type"), TypeName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
	}
	FString ScriptAssetPath;
	Params->TryGetStringField(TEXT("script_path"), ScriptAssetPath);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("type"), TypeName);

	// Case A: script asset info
	if (!ScriptAssetPath.IsEmpty())
	{
		UNiagaraScript* Script = LoadObject<UNiagaraScript>(nullptr, *ScriptAssetPath);
		if (!Script)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Failed to load script at '%s'"), *ScriptAssetPath));
		}
		Result->SetStringField(TEXT("kind"), TEXT("script"));
		Result->SetStringField(TEXT("script_path"), Script->GetPathName());
		Result->SetStringField(TEXT("usage"), NiagaraHelpers::ScriptUsageToString(Script->GetUsage()));

		TArray<TSharedPtr<FJsonValue>> InputsJson;
		TArray<TSharedPtr<FJsonValue>> OutputsJson;
		SerializeFunctionScriptPins(Script, InputsJson, OutputsJson);
		Result->SetArrayField(TEXT("inputs"), InputsJson);
		Result->SetArrayField(TEXT("outputs"), OutputsJson);
		return Result;
	}

	// Case B: node class info
	UClass* NodeClass = FindNiagaraNodeClass(TypeName);
	if (!NodeClass)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown Niagara node type '%s'"), *TypeName));
	}
	Result->SetStringField(TEXT("kind"), TEXT("node_class"));
	Result->SetStringField(TEXT("class_path"), NodeClass->GetPathName());

	FString DisplayName, Tooltip, Category, Keywords;
	GatherClassMeta(NodeClass, DisplayName, Tooltip, Category, Keywords);
	Result->SetStringField(TEXT("display_name"), DisplayName);
	if (!Category.IsEmpty()) Result->SetStringField(TEXT("category"), Category);
	if (!Tooltip.IsEmpty())  Result->SetStringField(TEXT("tooltip"), Tooltip);
	if (!Keywords.IsEmpty()) Result->SetStringField(TEXT("keywords"), Keywords);

	// Special-case: Custom HLSL schema
	if (NodeClass == UNiagaraNodeCustomHlsl::StaticClass())
	{
		Result->SetObjectField(TEXT("custom_hlsl_schema"), BuildCustomHlslSchema());
		Result->SetStringField(TEXT("note"),
			TEXT("Use set_niagara_scratch_pad_hlsl with inputs/outputs to (re)build pins, or add/rename/remove per-pin via add_niagara_custom_hlsl_input / rename_niagara_custom_hlsl_pin / remove_niagara_custom_hlsl_pin."));
		return Result;
	}

	// Generic reflection: list exposed UPROPERTY fields that can be set on an instance
	TArray<TSharedPtr<FJsonValue>> PropsJson;
	for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;
		auto P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"), Prop->GetName());
		P->SetStringField(TEXT("type"), Prop->GetCPPType());
		if (Prop->HasMetaData(TEXT("ToolTip")))
		{
			P->SetStringField(TEXT("tooltip"), Prop->GetMetaData(TEXT("ToolTip")));
		}
		PropsJson.Add(MakeShared<FJsonValueObject>(P));
	}
	Result->SetArrayField(TEXT("properties"), PropsJson);
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleSearchNiagaraFunctions  (script asset search by usage + name filter)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleSearchNiagaraFunctions(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	FString UsageFilter = TEXT("function");
	Params->TryGetStringField(TEXT("usage"), UsageFilter);

	int32 MaxResults = 100;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);
	bool bIncludeEngine = true;
	Params->TryGetBoolField(TEXT("include_engine"), bIncludeEngine);

	bool bUsageOk = false;
	ENiagaraScriptUsage TargetUsage = NiagaraHelpers::ParseScriptUsage(UsageFilter, bUsageOk);
	if (!bUsageOk)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid usage '%s'"), *UsageFilter));
	}

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());
	ARFilter.bRecursivePaths = true;
	if (!bIncludeEngine)
	{
		ARFilter.PackagePaths.Add(FName(TEXT("/Game")));
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(ARFilter, Assets);

	TArray<TSharedPtr<FJsonValue>> Results;
	for (const FAssetData& Asset : Assets)
	{
		if (Results.Num() >= MaxResults) break;

		FString UsageStr;
		Asset.GetTagValue(TEXT("Usage"), UsageStr);

		const FString NeedUsage = NiagaraHelpers::ScriptUsageToString(TargetUsage);
		if (!UsageStr.Contains(NeedUsage, ESearchCase::IgnoreCase))
		{
			// Fall back to loading for untagged assets
			UNiagaraScript* Script = Cast<UNiagaraScript>(Asset.GetAsset());
			if (!Script || Script->GetUsage() != TargetUsage) continue;
		}

		const FString AssetName = Asset.AssetName.ToString();
		const FString AssetPath = Asset.GetObjectPathString();
		if (!Filter.IsEmpty() &&
			!AssetName.Contains(Filter, ESearchCase::IgnoreCase) &&
			!AssetPath.Contains(Filter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		auto J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"), AssetName);
		J->SetStringField(TEXT("script_path"), AssetPath);
		J->SetStringField(TEXT("usage"), NeedUsage);
		Results.Add(MakeShared<FJsonValueObject>(J));
	}

	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("filter"), Filter);
	Out->SetStringField(TEXT("usage"), UsageFilter);
	Out->SetNumberField(TEXT("count"), Results.Num());
	Out->SetArrayField(TEXT("functions"), Results);
	return Out;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraSchemaActions  (calls UEdGraphSchema_Niagara::GetGraphActions)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraSchemaActions(
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
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'module_name' parameter — must be a scratch pad module"));
	}
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);
	int32 MaxResults = 300;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);

	FString Error;
	UNiagaraSystem* System = NiagaraHelpers::LoadNiagaraSystem(SystemPath, Error);
	if (!System)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UNiagaraScript* Script = nullptr;
	for (UNiagaraScript* S : System->ScratchPadScripts)
	{
		if (S && S->GetName().Equals(ModuleName, ESearchCase::IgnoreCase)) { Script = S; break; }
	}
	if (!Script)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Scratch pad module '%s' not found"), *ModuleName));
	}

	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource());
	if (!Source || !Source->NodeGraph)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Script has no graph"));
	}

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (!Schema)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Niagara schema unavailable"));
	}

	TArray<TSharedPtr<FNiagaraAction_NewNode>> Actions = Schema->GetGraphActions(
		Source->NodeGraph, nullptr, Source->NodeGraph);

	TArray<TSharedPtr<FJsonValue>> Results;
	for (const TSharedPtr<FNiagaraAction_NewNode>& Action : Actions)
	{
		if (!Action.IsValid()) continue;
		if (Results.Num() >= MaxResults) break;

		const FString DisplayName = Action->DisplayName.ToString();
		const FString Tooltip     = Action->ToolTip.ToString();
		const FString Keywords    = Action->Keywords.ToString();

		FString Category;
		if (Action->Categories.Num() > 0)
		{
			Category = FString::Join(Action->Categories, TEXT("|"));
		}

		if (!MatchesFilter(Filter, { &DisplayName, &Category, &Keywords, &Tooltip }))
			continue;

		auto J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("display_name"), DisplayName);
		if (!Category.IsEmpty()) J->SetStringField(TEXT("category"), Category);
		if (!Tooltip.IsEmpty())  J->SetStringField(TEXT("tooltip"), Tooltip);
		if (!Keywords.IsEmpty()) J->SetStringField(TEXT("keywords"), Keywords);
		if (UEdGraphNode* Template = Action->WeakNodeTemplate.Get())
		{
			J->SetStringField(TEXT("template_class"), Template->GetClass()->GetName());
		}
		Results.Add(MakeShared<FJsonValueObject>(J));
	}

	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("module_name"), ModuleName);
	Out->SetStringField(TEXT("filter"), Filter);
	Out->SetNumberField(TEXT("count"), Results.Num());
	Out->SetArrayField(TEXT("actions"), Results);
	return Out;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleDescribeNiagaraType  — generic Niagara type query
// Handles enum, struct, data interface, primitives via the modular FProperty
// introspector. Replaces hand-rolled per-type branches; one tool covers every
// FNiagaraTypeDefinition the engine knows about.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleDescribeNiagaraType(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString TypeName;
	if (!Params->TryGetStringField(TEXT("type"), TypeName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
	}

	FNiagaraTypeDefinition TypeDef;
	if (!NiagaraIntrospection::ResolveTypeName(TypeName, TypeDef))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown Niagara type '%s'"), *TypeName));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("type"), TypeName);
	Result->SetObjectField(TEXT("schema"), NiagaraIntrospection::SerializeNiagaraType(TypeDef));
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}

// ---------------------------------------------------------------------------
// HandleGetNiagaraDataInterfaceSchema
// Returns the full FProperty schema (every editable field) of a
// UNiagaraDataInterface subclass — covers Array{Float,Vector,Color,Matrix},
// SkeletalMesh, Spline, Curve, RenderTarget2D, VolumeTexture, CameraQuery,
// and any other DI in the registry. Use to discover what configuration a DI
// supports before instantiating one.
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPNiagaraCommands::HandleGetNiagaraDataInterfaceSchema(
	const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
	FString ClassName;
	if (!Params->TryGetStringField(TEXT("class"), ClassName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'class' parameter"));
	}

	// Normalize the user-supplied class name. UClass::GetName() returns names
	// WITHOUT the leading "U" prefix, so the canonical short name for the
	// float-array DI is "NiagaraDataInterfaceArrayFloat" (no U). Accept any of:
	//   ArrayFloat
	//   NiagaraDataInterfaceArrayFloat
	//   UNiagaraDataInterfaceArrayFloat  (with U — strip it)
	//   /Script/Niagara.NiagaraDataInterfaceArrayFloat  (full path)
	FString Normalized = ClassName;
	if (Normalized.StartsWith(TEXT("U"), ESearchCase::CaseSensitive) &&
		Normalized.Len() > 1 &&
		FChar::IsUpper(Normalized[1]))
	{
		Normalized.RightChopInline(1);
	}

	UClass* DIClass = FindObject<UClass>(nullptr, *ClassName);
	if (!DIClass)
	{
		DIClass = FindObject<UClass>(nullptr, *Normalized);
	}
	if (!DIClass || !DIClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		const FString WithPrefix = FString::Printf(TEXT("NiagaraDataInterface%s"), *Normalized);
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UNiagaraDataInterface::StaticClass())) continue;
			if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
			const FString CandidateName = It->GetName();
			if (CandidateName.Equals(Normalized, ESearchCase::IgnoreCase) ||
				CandidateName.Equals(WithPrefix, ESearchCase::IgnoreCase))
			{
				DIClass = *It;
				break;
			}
		}
	}

	if (!DIClass || !DIClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("UNiagaraDataInterface subclass '%s' not found"), *ClassName));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("class"), DIClass->GetName());
	Result->SetStringField(TEXT("class_path"), DIClass->GetPathName());
	Result->SetObjectField(TEXT("schema"),
		NiagaraIntrospection::SerializeDataInterfaceClass(DIClass));
	return Result;
#else
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor-only operation"));
#endif
}
