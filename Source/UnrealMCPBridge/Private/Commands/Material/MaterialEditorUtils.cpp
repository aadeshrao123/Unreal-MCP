#include "MaterialEditorUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
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
