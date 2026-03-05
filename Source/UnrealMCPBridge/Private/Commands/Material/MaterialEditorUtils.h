#pragma once

#include "CoreMinimal.h"

class UMaterial;
class IMaterialEditor;

/**
 * Internal helper functions for interacting with the Material Editor.
 *
 * The material editor works on a transient UPreviewMaterial copy, not the
 * original asset. These utilities locate the editor's working copy and
 * sync graph changes back to the visual UI.
 */

/**
 * Get the IMaterialEditor and its preview material for a given original asset.
 *
 * Uses UAssetEditorSubsystem::FindEditorForAsset() and the IMaterialEditor
 * interface — matching the pattern Epic uses in FMaterialEditor itself
 * (MaterialEditor.cpp line 8268).
 */
bool GetMaterialEditorContext(UMaterial* OriginalMaterial,
                              IMaterialEditor*& OutEditor,
                              UMaterial*& OutPreviewMaterial);

/** Resolve a material to its editor preview copy (if the editor is open). */
UMaterial* ResolveWorkingMaterial(UMaterial* OriginalMaterial);

/**
 * Refresh the material editor graph UI so changes appear in real time.
 *
 * For each expression without a graph node, calls AddExpression() to create
 * the visual UMaterialGraphNode wrapper. Then calls LinkGraphNodesFromMaterial()
 * to sync pin connections, and UpdateMaterialAfterGraphChange() for recompilation
 * and visual redraw.
 */
void NotifyMaterialEditorRefresh(UMaterial* OriginalMaterial);

/**
 * Full rebuild of the material editor graph.
 *
 * Used after deletions or bulk graph builds where the entire node set
 * may have changed.
 */
void RebuildMaterialEditorGraph(UMaterial* OriginalMaterial);
