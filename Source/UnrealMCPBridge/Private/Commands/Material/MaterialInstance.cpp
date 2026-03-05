#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "EditorAssetLibrary.h"

// ---------------------------------------------------------------------------
// HandleGetMaterialInstanceParameters
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleGetMaterialInstanceParameters(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("material_path"), AssetPath) &&
	    !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Asset);
	if (!MI)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material instance not found: %s"), *AssetPath));
	}

	// Scalar
	TArray<FName> Names;
	UMaterialEditingLibrary::GetScalarParameterNames(MI, Names);
	auto ScalarObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		ScalarObj->SetNumberField(N.ToString(),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(MI, N));
	}

	// Vector
	UMaterialEditingLibrary::GetVectorParameterNames(MI, Names);
	auto VectorObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		FLinearColor C = UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(MI, N);
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShared<FJsonValueNumber>(C.R));
		Arr.Add(MakeShared<FJsonValueNumber>(C.G));
		Arr.Add(MakeShared<FJsonValueNumber>(C.B));
		Arr.Add(MakeShared<FJsonValueNumber>(C.A));
		VectorObj->SetArrayField(N.ToString(), Arr);
	}

	// Texture
	UMaterialEditingLibrary::GetTextureParameterNames(MI, Names);
	auto TexObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		UTexture* T = UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MI, N);
		TexObj->SetStringField(N.ToString(), T ? T->GetPathName() : TEXT("None"));
	}

	// Static switch
	UMaterialEditingLibrary::GetStaticSwitchParameterNames(MI, Names);
	auto SwitchObj = MakeShared<FJsonObject>();
	for (const FName& N : Names)
	{
		SwitchObj->SetBoolField(N.ToString(),
			UMaterialEditingLibrary::GetMaterialInstanceStaticSwitchParameterValue(MI, N));
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("path"), AssetPath);
	R->SetObjectField(TEXT("scalar"),        ScalarObj);
	R->SetObjectField(TEXT("vector"),        VectorObj);
	R->SetObjectField(TEXT("texture"),       TexObj);
	R->SetObjectField(TEXT("static_switch"), SwitchObj);
	return R;
}

// ---------------------------------------------------------------------------
// HandleSetMaterialInstanceParameter
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleSetMaterialInstanceParameter(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("material_path"), AssetPath) &&
	    !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
	}
	FString ParamName;
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
	}
	FString ParamType;
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'param_type': scalar | vector | texture | static_switch"));
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Asset);
	if (!MI)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Material instance not found: %s"), *AssetPath));
	}

	FName ParamFName(*ParamName);
	FString Lower = ParamType.ToLower();
	bool bSuccess = false;
	FString ErrMsg;

	if (Lower == TEXT("scalar") || Lower == TEXT("float"))
	{
		// UE5 bug: SetMaterialInstanceScalarParameterValue never sets bResult=true.
		// Call SetScalarParameterValueEditorOnly directly.
		double Val;
		if (Params->TryGetNumberField(TEXT("value"), Val))
		{
			MI->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), (float)Val);
			bSuccess = true;
		}
		else
		{
			// value may arrive as a string (e.g. "0.5") — parse it
			FString ValStr;
			if (Params->TryGetStringField(TEXT("value"), ValStr) && FCString::IsNumeric(*ValStr))
			{
				MI->SetScalarParameterValueEditorOnly(
					FMaterialParameterInfo(ParamFName), FCString::Atof(*ValStr));
				bSuccess = true;
			}
			else
			{
				ErrMsg = TEXT("Missing 'value' number");
			}
		}
	}
	else if (Lower == TEXT("vector") || Lower == TEXT("color"))
	{
		// Try direct JSON array first, then fall back to parsing a string-encoded array
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		TSharedPtr<FJsonValue> ParsedValue;
		TArray<TSharedPtr<FJsonValue>> ParsedArray;

		if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr && Arr->Num() >= 3)
		{
			// Direct JSON array — use as-is
		}
		else
		{
			// value is likely a string like "[1.0, 0.1, 0.05, 1.0]" — parse it
			FString ValStr;
			if (Params->TryGetStringField(TEXT("value"), ValStr))
			{
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ValStr);
				if (FJsonSerializer::Deserialize(Reader, ParsedValue) && ParsedValue.IsValid()
					&& ParsedValue->Type == EJson::Array)
				{
					ParsedArray = ParsedValue->AsArray();
					Arr = &ParsedArray;
				}
			}
		}

		if (!Arr || Arr->Num() < 3)
		{
			ErrMsg = TEXT("Missing 'value' [r,g,b] or [r,g,b,a] array");
		}
		else
		{
			FLinearColor C(
				(float)(*Arr)[0]->AsNumber(),
				(float)(*Arr)[1]->AsNumber(),
				(float)(*Arr)[2]->AsNumber(),
				Arr->Num() > 3 ? (float)(*Arr)[3]->AsNumber() : 1.0f);
			// UE5 bug: UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue
			// never sets bResult=true. Call SetVectorParameterValueEditorOnly directly.
			MI->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), C);
			bSuccess = true;
		}
	}
	else if (Lower == TEXT("texture"))
	{
		FString TexPath;
		if (!Params->TryGetStringField(TEXT("value"), TexPath))
		{
			ErrMsg = TEXT("Missing 'value' texture path");
		}
		else
		{
			UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexPath));
			if (!Tex)
			{
				ErrMsg = FString::Printf(TEXT("Texture not found: %s"), *TexPath);
			}
			else
			{
				// UE5 bug: same bResult issue as scalar/vector — call directly.
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParamFName), Tex);
				bSuccess = true;
			}
		}
	}
	else if (Lower == TEXT("static_switch") || Lower == TEXT("switch") || Lower == TEXT("bool"))
	{
		bool Val = false;
		Params->TryGetBoolField(TEXT("value"), Val);
		bSuccess = UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(
			MI, ParamFName, Val);
	}
	else
	{
		ErrMsg = FString::Printf(
			TEXT("Unknown param_type '%s'. Use: scalar|vector|texture|static_switch"), *ParamType);
	}

	if (!ErrMsg.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ErrMsg);
	}

	if (bSuccess)
	{
		// Mirror what the static switch setter does (MaterialEditingLibrary.cpp:1285-1286):
		// Create a transient UMaterialEditorInstanceConstant and call SetSourceInstance().
		// This triggers RegenerateArrays() which properly registers parameter overrides
		// so the MI editor doesn't wipe values on first open.
		UMaterialEditorInstanceConstant* EditorInst = NewObject<UMaterialEditorInstanceConstant>(
			GetTransientPackage(), NAME_None, RF_Transactional);
		EditorInst->SetSourceInstance(MI);

		UMaterialEditingLibrary::UpdateMaterialInstance(MI);

		// Rebuild any already-open MI editors so parameter checkboxes refresh immediately
		UMaterial* BaseMaterial = MI->GetMaterial();
		if (BaseMaterial)
		{
			UMaterialEditingLibrary::RebuildMaterialInstanceEditors(BaseMaterial);
		}

		UEditorAssetLibrary::SaveAsset(AssetPath);
	}

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), bSuccess);
	R->SetStringField(TEXT("path"), AssetPath);
	R->SetStringField(TEXT("param_name"), ParamName);
	R->SetStringField(TEXT("param_type"), ParamType);
	if (!bSuccess)
	{
		R->SetStringField(TEXT("error"),
			TEXT("SetParameter returned false — parameter may not exist in this instance's parent"));
	}
	return R;
}

// ---------------------------------------------------------------------------
// HandleListMaterialExpressionTypes
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPMaterialCommands::HandleListMaterialExpressionTypes(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	static const FString ExpressionPrefix = TEXT("MaterialExpression");

	// Classes to exclude (same as Epic's MaterialExpressionClasses.cpp)
	static TSet<FName> ExcludedClasses = {
		TEXT("MaterialExpressionComment"),            // Handled by add_material_comments
		TEXT("MaterialExpressionParameter"),           // Abstract base for parameter nodes
		TEXT("MaterialExpressionNamedRerouteUsage"),   // Internal reroute usage
		TEXT("MaterialExpressionMaterialLayerOutput"), // Internal layer output
	};

	TArray<TSharedPtr<FJsonValue>> ResultArray;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		if (!Class->IsChildOf(UMaterialExpression::StaticClass()))
		{
			continue;
		}
		if (Class->HasMetaData(TEXT("Private")))
		{
			continue;
		}
		if (ExcludedClasses.Contains(Class->GetFName()))
		{
			continue;
		}

		FString ClassName = Class->GetName();

		// Strip the "MaterialExpression" prefix for the short name
		FString ShortName = ClassName;
		if (ShortName.StartsWith(ExpressionPrefix, ESearchCase::CaseSensitive))
		{
			ShortName = ShortName.Mid(ExpressionPrefix.Len());
		}

		// Get display name if available
		FString DisplayName;
		if (Class->HasMetaData(TEXT("DisplayName")))
		{
			DisplayName = Class->GetDisplayNameText().ToString();
		}

		// Get CDO for keywords, categories, descriptions
		UMaterialExpression* CDO = Cast<UMaterialExpression>(Class->GetDefaultObject());

		FString Keywords;
		if (CDO)
		{
			Keywords = CDO->GetKeywords().ToString();
		}

		FString CreationName;
		if (CDO)
		{
			CreationName = CDO->GetCreationName().ToString();
		}

		// Apply filter — match against all searchable fields
		if (!Filter.IsEmpty())
		{
			bool bMatches = ShortName.Contains(Filter, ESearchCase::IgnoreCase)
				|| (!DisplayName.IsEmpty() && DisplayName.Contains(Filter, ESearchCase::IgnoreCase))
				|| (!Keywords.IsEmpty() && Keywords.Contains(Filter, ESearchCase::IgnoreCase))
				|| (!CreationName.IsEmpty() && CreationName.Contains(Filter, ESearchCase::IgnoreCase));

			if (!bMatches && CDO)
			{
				for (const FText& Cat : CDO->MenuCategories)
				{
					if (Cat.ToString().Contains(Filter, ESearchCase::IgnoreCase))
					{
						bMatches = true;
						break;
					}
				}
			}

			if (!bMatches)
			{
				continue;
			}
		}

		// Build result entry — only include fields that have values to minimize tokens
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("type"), ShortName);

		if (!DisplayName.IsEmpty() && DisplayName != ShortName)
		{
			Entry->SetStringField(TEXT("display_name"), DisplayName);
		}

		if (!Keywords.IsEmpty())
		{
			Entry->SetStringField(TEXT("keywords"), Keywords);
		}

		if (CDO)
		{
			if (CDO->MenuCategories.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> CatArray;
				for (const FText& Cat : CDO->MenuCategories)
				{
					CatArray.Add(MakeShared<FJsonValueString>(Cat.ToString()));
				}
				Entry->SetArrayField(TEXT("categories"), CatArray);
			}

			FString Desc = CDO->GetCreationDescription().ToString();
			if (!Desc.IsEmpty())
			{
				Entry->SetStringField(TEXT("description"), Desc);
			}
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Sort by short name
	ResultArray.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("type")) <
		       B->AsObject()->GetStringField(TEXT("type"));
	});

	auto R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetNumberField(TEXT("count"), ResultArray.Num());
	if (!Filter.IsEmpty())
	{
		R->SetStringField(TEXT("filter"), Filter);
	}
	R->SetArrayField(TEXT("expression_types"), ResultArray);
	return R;
}
