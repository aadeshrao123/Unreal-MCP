#include "NiagaraTypeHelpers.h"

#include "NiagaraTypes.h"
#include "NiagaraTypeRegistry.h"
#include "NiagaraCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"

#if WITH_EDITORONLY_DATA
#include "NiagaraNodeCustomHlsl.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#endif

#include "UObject/UnrealType.h"

namespace NiagaraTypeHelpers
{

bool ParseTypeDef(const FString& TypeString, FNiagaraTypeDefinition& OutType)
{
	const FString Lower = TypeString.ToLower().Replace(TEXT(" "), TEXT(""));

	// Built-in fast paths
	if (Lower == TEXT("float") || Lower == TEXT("scalar"))
	{
		OutType = FNiagaraTypeDefinition::GetFloatDef();
		return true;
	}
	if (Lower == TEXT("int") || Lower == TEXT("int32") || Lower == TEXT("integer"))
	{
		OutType = FNiagaraTypeDefinition::GetIntDef();
		return true;
	}
	if (Lower == TEXT("bool") || Lower == TEXT("boolean"))
	{
		OutType = FNiagaraTypeDefinition::GetBoolDef();
		return true;
	}
	if (Lower == TEXT("vec2") || Lower == TEXT("vector2") || Lower == TEXT("vector2d"))
	{
		OutType = FNiagaraTypeDefinition::GetVec2Def();
		return true;
	}
	if (Lower == TEXT("vec3") || Lower == TEXT("vector") || Lower == TEXT("vector3"))
	{
		OutType = FNiagaraTypeDefinition::GetVec3Def();
		return true;
	}
	if (Lower == TEXT("vec4") || Lower == TEXT("vector4"))
	{
		OutType = FNiagaraTypeDefinition::GetVec4Def();
		return true;
	}
	if (Lower == TEXT("color") || Lower == TEXT("linearcolor"))
	{
		OutType = FNiagaraTypeDefinition::GetColorDef();
		return true;
	}
	if (Lower == TEXT("quat") || Lower == TEXT("quaternion"))
	{
		OutType = FNiagaraTypeDefinition::GetQuatDef();
		return true;
	}
	if (Lower == TEXT("matrix") || Lower == TEXT("matrix4") || Lower == TEXT("mat4"))
	{
		OutType = FNiagaraTypeDefinition::GetMatrix4Def();
		return true;
	}
	if (Lower == TEXT("position"))
	{
		OutType = FNiagaraTypeDefinition::GetPositionDef();
		return true;
	}
	if (Lower == TEXT("parametermap") || Lower == TEXT("paramap") || Lower == TEXT("map"))
	{
		OutType = FNiagaraTypeDefinition::GetParameterMapDef();
		return true;
	}
	if (Lower == TEXT("id") || Lower == TEXT("niagaraid"))
	{
		OutType = FNiagaraTypeDefinition::GetIDDef();
		return true;
	}

	// Fallback: lookup in the registered type registry by name
	TOptional<FNiagaraTypeDefinition> Found = FNiagaraTypeRegistry::GetRegisteredTypeByName(FName(*TypeString));
	if (Found.IsSet() && Found.GetValue().IsValid())
	{
		OutType = Found.GetValue();
		return true;
	}

	return false;
}

FString TypeDefToString(const FNiagaraTypeDefinition& TypeDef)
{
	if (!TypeDef.IsValid())
	{
		return TEXT("Invalid");
	}
	return TypeDef.GetName();
}

// ---------------------------------------------------------------------------
// AddTypedPin  (reproduces UNiagaraNodeWithDynamicPins::RequestNewTypedPin)
// ---------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA

// Mirrors UNiagaraNodeWithDynamicPins::AddPinSubCategory (not exported from NiagaraEditor).
// See Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraNodeWithDynamicPins.cpp
static const FName NiagaraAddPinSubCategory(TEXT("DynamicAddPin"));

bool IsAddPin(const UEdGraphPin* Pin)
{
	return Pin &&
		Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc &&
		Pin->PinType.PinSubCategory == NiagaraAddPinSubCategory;
}

// Local alias for in-file readability
static bool IsNiagaraAddPin(const UEdGraphPin* Pin) { return IsAddPin(Pin); }

static UEdGraphPin* FindAddPin(UNiagaraNode* Node, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && IsNiagaraAddPin(Pin))
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* CreateReplacementAddPin(UNiagaraNode* Node, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}
	const FEdGraphPinType AddPinType(
		UEdGraphSchema_Niagara::PinCategoryMisc,
		NiagaraAddPinSubCategory,
		nullptr,
		EPinContainerType::None,
		false,
		FEdGraphTerminalType());
	return Node->CreatePin(Direction, AddPinType, TEXT("Add"));
}

UEdGraphPin* AddTypedPin(
	UNiagaraNode* Node,
	EEdGraphPinDirection Direction,
	const FNiagaraTypeDefinition& Type,
	const FName& InName,
	bool bRebuildSignature)
{
	if (!Node || !Type.IsValid())
	{
		return nullptr;
	}

	Node->Modify();
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (!Schema)
	{
		return nullptr;
	}

	// Look for an existing "Add" pin to convert — this matches the engine's
	// RequestNewTypedPin which reuses the Add pin and spawns a replacement.
	UEdGraphPin* Target = FindAddPin(Node, Direction);
	if (Target)
	{
		Target->Modify();
		Target->PinType = Schema->TypeDefinitionToPinType(Type);
		Target->PinName = InName;
		// Restore the Add pin so the UI keeps a "+" affordance
		CreateReplacementAddPin(Node, Direction);
	}
	else
	{
		// Node has no Add pin yet (e.g. freshly created scratch pad custom hlsl
		// node we just constructed). Create the typed pin directly.
		const FEdGraphPinType PinType = Schema->TypeDefinitionToPinType(Type);
		Target = Node->CreatePin(Direction, PinType, InName);
	}

	if (bRebuildSignature)
	{
		if (UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node))
		{
			RebuildSignatureFromPins(FuncCall);
		}
	}

	Node->MarkNodeRequiresSynchronization(__FUNCTION__, true);
	if (UEdGraph* Graph = Node->GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
	return Target;
}

void RebuildSignatureFromPins(UNiagaraNodeFunctionCall* FunctionCallNode)
{
	if (!FunctionCallNode)
	{
		return;
	}
	FunctionCallNode->Modify();

	const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(FunctionCallNode->GetSchema());
	if (!Schema)
	{
		Schema = GetDefault<UEdGraphSchema_Niagara>();
	}
	if (!Schema)
	{
		return;
	}

	FNiagaraFunctionSignature Sig = FunctionCallNode->Signature;
	Sig.Inputs.Empty();
	Sig.Outputs.Empty();

	for (UEdGraphPin* Pin : FunctionCallNode->Pins)
	{
		if (!Pin || IsNiagaraAddPin(Pin))
		{
			continue;
		}
		if (Pin->Direction == EGPD_Input)
		{
			Sig.Inputs.Add(Schema->PinToNiagaraVariable(Pin, true));
		}
		else
		{
			Sig.Outputs.Add(Schema->PinToNiagaraVariable(Pin, false));
		}
	}

	FunctionCallNode->Signature = Sig;
}

bool RenameDynamicPin(
	UNiagaraNode* Node,
	const FName& OldName,
	const FName& NewName,
	FString& OutError)
{
	if (!Node)
	{
		OutError = TEXT("Null node");
		return false;
	}
	if (OldName == NewName)
	{
		return true;
	}

	UEdGraphPin* Target = nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName == OldName && !IsNiagaraAddPin(Pin))
		{
			Target = Pin;
			break;
		}
	}
	if (!Target)
	{
		OutError = FString::Printf(TEXT("Pin '%s' not found on node"), *OldName.ToString());
		return false;
	}

	// Check for name collisions with other pins (excluding Add pins)
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin != Target && Pin && Pin->PinName == NewName && !IsNiagaraAddPin(Pin))
		{
			OutError = FString::Printf(TEXT("Pin name '%s' already exists"), *NewName.ToString());
			return false;
		}
	}

	Node->Modify();
	Target->Modify();
	const FString OldNameStr = OldName.ToString();
	Target->PinName = NewName;

	// CRITICAL ORDERING: rebuild the function signature from the new pin list
	// BEFORE touching CustomHlsl. PostEditChangeProperty on the CustomHlsl field
	// triggers RefreshFromExternalChanges, which re-reads pin state from the
	// signature — if the signature still has the old pin name, the refresh
	// rolls the rename back silently. Syncing the signature first avoids that.
	if (UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node))
	{
		RebuildSignatureFromPins(FuncCall);
	}

	// For custom HLSL, rewrite references to the old pin name inside the HLSL source
	// so the shader code keeps compiling. The Niagara translator accepts either
	// bare identifiers or {BracedForm} for pin references; we substitute the
	// braced form via whole-word replacement using direct string reflection
	// (GetCustomHlsl is not NIAGARAEDITOR_API exported).
	if (Node->IsA<UNiagaraNodeCustomHlsl>())
	{
		if (FProperty* HlslProp = UNiagaraNodeCustomHlsl::StaticClass()->FindPropertyByName(TEXT("CustomHlsl")))
		{
			if (FStrProperty* StrProp = CastField<FStrProperty>(HlslProp))
			{
				FString CurrentHlsl = StrProp->GetPropertyValue_InContainer(Node);
				if (!CurrentHlsl.IsEmpty())
				{
					const FString OldBraced = FString::Printf(TEXT("{%s}"), *OldNameStr);
					const FString NewBraced = FString::Printf(TEXT("{%s}"), *NewName.ToString());
					CurrentHlsl.ReplaceInline(*OldBraced, *NewBraced, ESearchCase::CaseSensitive);
					SetCustomHlslViaReflection(Node, CurrentHlsl);
				}
			}
		}
	}

	Node->MarkNodeRequiresSynchronization(__FUNCTION__, true);
	if (UEdGraph* Graph = Node->GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
	return true;
}

bool RemoveDynamicPinByName(
	UNiagaraNode* Node,
	const FName& PinName,
	FString& OutError)
{
	if (!Node)
	{
		OutError = TEXT("Null node");
		return false;
	}

	UEdGraphPin* Target = nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName == PinName && !IsNiagaraAddPin(Pin))
		{
			Target = Pin;
			break;
		}
	}
	if (!Target)
	{
		OutError = FString::Printf(TEXT("Pin '%s' not found on node"), *PinName.ToString());
		return false;
	}

	Node->Modify();
	Target->Modify();
	Target->BreakAllPinLinks();
	Node->RemovePin(Target);

	if (UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node))
	{
		RebuildSignatureFromPins(FuncCall);
	}

	Node->MarkNodeRequiresSynchronization(__FUNCTION__, true);
	if (UEdGraph* Graph = Node->GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
	return true;
}

bool SetCustomHlslViaReflection(UNiagaraNode* HlslNode, const FString& HlslCode)
{
	if (!HlslNode || !HlslNode->IsA<UNiagaraNodeCustomHlsl>())
	{
		return false;
	}

	FProperty* HlslProp = UNiagaraNodeCustomHlsl::StaticClass()->FindPropertyByName(TEXT("CustomHlsl"));
	FStrProperty* StrProp = CastField<FStrProperty>(HlslProp);
	if (!StrProp)
	{
		return false;
	}

	HlslNode->Modify();
	StrProp->SetPropertyValue_InContainer(HlslNode, HlslCode);

	// Mimic PostEditChangeProperty on the CustomHlsl field — the override in
	// UNiagaraNodeCustomHlsl calls RefreshFromExternalChanges + NotifyGraphNeedsRecompile.
	FPropertyChangedEvent ChangedEvent(HlslProp, EPropertyChangeType::ValueSet);
	HlslNode->PostEditChangeProperty(ChangedEvent);

	HlslNode->MarkNodeRequiresSynchronization(__FUNCTION__, true);
	return true;
}

#else  // !WITH_EDITORONLY_DATA

bool IsAddPin(const UEdGraphPin*) { return false; }
UEdGraphPin* AddTypedPin(UNiagaraNode*, EEdGraphPinDirection, const FNiagaraTypeDefinition&, const FName&, bool) { return nullptr; }
void RebuildSignatureFromPins(UNiagaraNodeFunctionCall*) {}
bool RenameDynamicPin(UNiagaraNode*, const FName&, const FName&, FString& OutError) { OutError = TEXT("Editor-only"); return false; }
bool RemoveDynamicPinByName(UNiagaraNode*, const FName&, FString& OutError) { OutError = TEXT("Editor-only"); return false; }
bool SetCustomHlslViaReflection(UNiagaraNode*, const FString&) { return false; }

#endif

} // namespace NiagaraTypeHelpers
