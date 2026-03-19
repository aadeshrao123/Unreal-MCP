#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunction.h"
#include "IMaterialEditor.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Subsystems/AssetEditorSubsystem.h"

// ---------------------------------------------------------------------------

bool GetMaterialEditorContext(UMaterial* OriginalMaterial,
                              IMaterialEditor*& OutEditor,
                              UMaterial*& OutPreviewMaterial)
{
	OutEditor = nullptr;
	OutPreviewMaterial = nullptr;

	if (!OriginalMaterial || !GEditor)
	{
		return false;
	}

	UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!Sub)
	{
		return false;
	}

	IAssetEditorInstance* EditorInstance = Sub->FindEditorForAsset(OriginalMaterial, /*bFocusIfOpen=*/false);
	if (!EditorInstance)
	{
		return false;
	}
	if (EditorInstance->GetEditorName() != FName("MaterialEditor"))
	{
		return false;
	}

	// Safe cast — same pattern as MaterialEditor.cpp line 8268:
	// "We've ensured that this is a valid cast by checking GetEditorName()"
	OutEditor = static_cast<IMaterialEditor*>(EditorInstance);
	OutPreviewMaterial = Cast<UMaterial>(OutEditor->GetMaterialInterface());

	return OutPreviewMaterial != nullptr && OutPreviewMaterial->MaterialGraph != nullptr;
}

// ---------------------------------------------------------------------------

UMaterial* ResolveWorkingMaterial(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
	{
		return PreviewMat;
	}
	return OriginalMaterial;
}

// ---------------------------------------------------------------------------

void NotifyMaterialEditorRefresh(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (!GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
	{
		return; // Editor not open — nothing to refresh
	}

	UMaterialGraph* Graph = PreviewMat->MaterialGraph;

	// Create graph nodes for any expressions that lack one (newly added via library)
	for (UMaterialExpression* Expr : PreviewMat->GetExpressions())
	{
		if (Expr && !Expr->GraphNode)
		{
			Graph->AddExpression(Expr, /*bUserInvoked=*/false);
		}
	}

	// Sync all pin connections from expression data
	Graph->LinkGraphNodesFromMaterial();

	// Recompile preview, refresh expression previews, mark dirty, update visuals
	Editor->UpdateMaterialAfterGraphChange();
}

// ---------------------------------------------------------------------------

void RebuildMaterialEditorGraph(UMaterial* OriginalMaterial)
{
	IMaterialEditor* Editor = nullptr;
	UMaterial* PreviewMat = nullptr;
	if (!GetMaterialEditorContext(OriginalMaterial, Editor, PreviewMat))
	{
		return;
	}

	PreviewMat->MaterialGraph->RebuildGraph();
	Editor->UpdateMaterialAfterGraphChange();
}

// ---------------------------------------------------------------------------
// Material Function Editor helpers
// ---------------------------------------------------------------------------

bool GetMaterialFunctionEditorContext(UMaterialFunction* OriginalFunction,
                                      IMaterialEditor*& OutEditor,
                                      UMaterialFunction*& OutPreviewFunction)
{
	OutEditor = nullptr;
	OutPreviewFunction = nullptr;

	if (!OriginalFunction || !GEditor)
	{
		return false;
	}

	UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!Sub)
	{
		return false;
	}

	IAssetEditorInstance* EditorInstance = Sub->FindEditorForAsset(OriginalFunction, /*bFocusIfOpen=*/false);
	if (!EditorInstance)
	{
		return false;
	}

	if (EditorInstance->GetEditorName() != FName("MaterialEditor"))
	{
		return false;
	}

	OutEditor = static_cast<IMaterialEditor*>(EditorInstance);

	// The editor's preview material's MaterialGraph has a pointer to the transient MF copy.
	// The transient copy has ParentFunction == OriginalFunction.
	UMaterialInterface* MatInterface = OutEditor->GetMaterialInterface();
	UMaterial* PreviewMat = Cast<UMaterial>(MatInterface);
	if (PreviewMat && PreviewMat->MaterialGraph)
	{
		OutPreviewFunction = PreviewMat->MaterialGraph->MaterialFunction;
	}

	return OutPreviewFunction != nullptr;
}

/** Resolve a MF to its editor transient copy (if the editor is open). */
UMaterialFunction* ResolveWorkingMaterialFunction(UMaterialFunction* OriginalFunction)
{
	IMaterialEditor* Editor = nullptr;
	UMaterialFunction* PreviewFunc = nullptr;
	if (GetMaterialFunctionEditorContext(OriginalFunction, Editor, PreviewFunc))
	{
		return PreviewFunc;
	}
	return OriginalFunction;
}

void NotifyMaterialFunctionEditorRefresh(UMaterialFunction* OriginalFunction)
{
	IMaterialEditor* Editor = nullptr;
	UMaterialFunction* PreviewFunc = nullptr;
	if (!GetMaterialFunctionEditorContext(OriginalFunction, Editor, PreviewFunc))
	{
		return;
	}

	// RebuildGraph() reads from Material->GetExpressions(), NOT from the MF.
	// So we must sync the MF's expression collection into the preview Material
	// before rebuilding the graph. This mirrors what FMaterialEditor::InitMaterialEditor does.
	UMaterialInterface* MatInterface = Editor->GetMaterialInterface();
	UMaterial* PreviewMat = Cast<UMaterial>(MatInterface);
	if (PreviewMat && PreviewFunc)
	{
		// Sync MF expressions into the Material's expression collection
		FMaterialExpressionCollection& MatCollection = PreviewMat->GetExpressionCollection();
		FMaterialExpressionCollection& MFCollection = PreviewFunc->GetExpressionCollection();

		// Add any MF expressions that aren't already in the Material
		for (UMaterialExpression* Expr : MFCollection.Expressions)
		{
			if (Expr && !MatCollection.Expressions.Contains(Expr))
			{
				MatCollection.Expressions.Add(Expr);
			}
		}

		// Remove any Material expressions that are no longer in the MF
		for (int32 i = MatCollection.Expressions.Num() - 1; i >= 0; --i)
		{
			if (!MFCollection.Expressions.Contains(MatCollection.Expressions[i]))
			{
				MatCollection.Expressions.RemoveAt(i);
			}
		}

		if (PreviewMat->MaterialGraph)
		{
			PreviewMat->MaterialGraph->RebuildGraph();
		}
	}

	Editor->UpdateMaterialAfterGraphChange();
}
