// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FPMBiomePCGConfig.generated.h"

/**
 * UFPMBiomePCGConfig
 *
 * Data Asset that drives the PCG biome spawner.
 * Assign static meshes for trees, rocks, and grass here.
 * The PCG system reads this to know *what* to spawn and *how dense*.
 *
 * Create one in Editor: Content Browser -> Add -> Data Asset ->
 * FPMBiomePCGConfig
 */
UCLASS(BlueprintType)
class FALDORANPRIMEMMO_API UFPMBiomePCGConfig : public UPrimaryDataAsset {
  GENERATED_BODY()

public:
  // ---- Forest ----

  /** Static meshes used for forest tree canopy. Randomly selected per instance.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
  TArray<TSoftObjectPtr<UStaticMesh>> ForestTreeMeshes;

  /** Static meshes used for forest tree trunks (spawned at same position as
   * canopy). If empty, only canopy is spawned. Should have same count as
   * ForestTreeMeshes for 1:1 pairing, or 1 entry to share across all. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
  TArray<TSoftObjectPtr<UStaticMesh>> ForestTrunkMeshes;

  /** Number of trees to attempt per chunk in Forest biome */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest",
            meta = (ClampMin = "0", ClampMax = "500"))
  int32 ForestTreeDensity = 60;

  /** Min/Max random scale for forest trees */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
  FVector2D ForestTreeScaleRange = FVector2D(0.8f, 1.4f);

  /** Z offset applied to forest trees after placement.
   *  Negative values sink the mesh into the ground.
   *  Defaulted to -95 based on testing (compensates for pivot offset). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest",
            meta = (ClampMin = "-500", ClampMax = "500"))
  float ForestTreeZOffset = -95.0f;

  // ---- Meadow ----

  /** Static meshes for meadow trees (scattered, fewer than forest) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meadow")
  TArray<TSoftObjectPtr<UStaticMesh>> MeadowTreeMeshes;

  /** Trunk meshes for meadow trees (same pairing rules as forest) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meadow")
  TArray<TSoftObjectPtr<UStaticMesh>> MeadowTrunkMeshes;

  /** Number of trees per chunk in Meadow biome */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meadow",
            meta = (ClampMin = "0", ClampMax = "200"))
  int32 MeadowTreeDensity = 30;

  /** Z offset for meadow trees (negative = sink into ground) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meadow",
            meta = (ClampMin = "-500", ClampMax = "500"))
  float MeadowTreeZOffset = -95.0f;

  // ---- Mountain / Rocks ----

  /** Rock meshes for mountain biome */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain")
  TArray<TSoftObjectPtr<UStaticMesh>> MountainRockMeshes;

  /** Number of rocks per chunk in Mountain biome */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain",
            meta = (ClampMin = "0", ClampMax = "200"))
  int32 MountainRockDensity = 40;

  /** Min/Max random scale for mountain rocks */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain")
  FVector2D MountainRockScaleRange = FVector2D(0.5f, 2.0f);

  /** Z offset for mountain rocks (negative = sink into ground) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain",
            meta = (ClampMin = "-500", ClampMax = "500"))
  float MountainRockZOffset = -30.0f;

  // ---- Scatter Rocks (all biomes except Ocean/Coast) ----

  /** Small rock/boulder meshes that appear everywhere */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
  TArray<TSoftObjectPtr<UStaticMesh>> ScatterRockMeshes;

  /** Scatter rock density per chunk */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter",
            meta = (ClampMin = "0", ClampMax = "100"))
  int32 ScatterRockDensity = 15;

  /** Z offset for scatter rocks (negative = sink into ground) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter",
            meta = (ClampMin = "-500", ClampMax = "500"))
  float ScatterRockZOffset = -15.0f;

  // ---- PCG Graph ----

  /** The PCG Graph asset to use for biome spawning.
   *  If null, the system creates a runtime graph automatically. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TSoftObjectPtr<class UPCGGraphInterface> PCGGraphOverride;
};
