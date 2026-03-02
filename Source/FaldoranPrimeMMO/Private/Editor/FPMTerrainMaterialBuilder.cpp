// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
// Editor-only helper that auto-creates /Game/Materials/M_TerrainBiome
// the first time the editor loads if it doesn't already exist.
// The material reads vertex colours → BaseColor so all biome colours show.

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace {

static const TCHAR *kAssetPath = TEXT("/Game/Materials/M_TerrainBiome");
static const TCHAR *kPkgPath = TEXT("/Game/Materials");
static const TCHAR *kAssetName = TEXT("M_TerrainBiome");

void EnsureTerrainBiomeMaterial() {
  // Check on disk first — the asset registry isn't fully populated yet
  // when this runs at startup, so querying it always returns "not found".
  const FString PkgFile = FPackageName::LongPackageNameToFilename(
      kAssetPath, FPackageName::GetAssetPackageExtension());
  if (FPaths::FileExists(PkgFile)) {
    return; // Already exists — nothing to do
  }

  UE_LOG(LogTemp, Log, TEXT("FPM: Creating /Game/Materials/M_TerrainBiome …"));

  // Create the material via AssetTools
  IAssetTools &AT =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  auto *Factory = NewObject<UMaterialFactoryNew>();
  UMaterial *Mat = Cast<UMaterial>(
      AT.CreateAsset(kAssetName, kPkgPath, UMaterial::StaticClass(), Factory));
  if (!Mat) {
    UE_LOG(LogTemp, Error, TEXT("FPM: Failed to create M_TerrainBiome"));
    return;
  }

  // ── VertexColor → BaseColor ───────────────────────────────────────
  UMaterialExpressionVertexColor *VC =
      NewObject<UMaterialExpressionVertexColor>(Mat);
  VC->MaterialExpressionEditorX = -350;
  VC->MaterialExpressionEditorY = 0;
  Mat->GetExpressionCollection().AddExpression(VC);
  Mat->GetEditorOnlyData()->BaseColor.Expression = VC;

  // ── Constant roughness 0.75 ───────────────────────────────────────
  UMaterialExpressionConstant *Rough =
      NewObject<UMaterialExpressionConstant>(Mat);
  Rough->R = 0.75f;
  Rough->MaterialExpressionEditorX = -350;
  Rough->MaterialExpressionEditorY = 120;
  Mat->GetExpressionCollection().AddExpression(Rough);
  Mat->GetEditorOnlyData()->Roughness.Expression = Rough;

  // ── Metallic = 0 ─────────────────────────────────────────────────
  UMaterialExpressionConstant *Metal =
      NewObject<UMaterialExpressionConstant>(Mat);
  Metal->R = 0.0f;
  Metal->MaterialExpressionEditorX = -350;
  Metal->MaterialExpressionEditorY = 200;
  Mat->GetExpressionCollection().AddExpression(Metal);
  Mat->GetEditorOnlyData()->Metallic.Expression = Metal;

  // ── Compile & save ────────────────────────────────────────────────
  Mat->PreEditChange(nullptr);
  Mat->PostEditChange();
  Mat->MarkPackageDirty();

  FSavePackageArgs SaveArgs;
  SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
  const FString SaveFile = FPackageName::LongPackageNameToFilename(
      kAssetPath, FPackageName::GetAssetPackageExtension());
  UPackage::SavePackage(Mat->GetOutermost(), Mat, *SaveFile, SaveArgs);

  // Notify the asset registry that a new asset was created
  FAssetRegistryModule::AssetCreated(Mat);

  UE_LOG(
      LogTemp, Log,
      TEXT("FPM: M_TerrainBiome created successfully. "
           "Assign it to WorldChunkManager > FPM|World > Terrain Material."));
}

// Register callback: runs once when the editor finishes loading
struct FTerrainMaterialAutoCreate {
  FTerrainMaterialAutoCreate() {
    if (GEditor) {
      // Editor already loaded (hot-reload path)
      EnsureTerrainBiomeMaterial();
    } else {
      // Normal startup: wait for editor to finish init
      FEditorDelegates::OnEditorInitialized.AddLambda(
          [](double) { EnsureTerrainBiomeMaterial(); });
    }
  }
} GTerrainMaterialAutoCreate;

} // anonymous namespace

#endif // WITH_EDITOR
