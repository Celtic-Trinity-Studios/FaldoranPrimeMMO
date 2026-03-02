// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/FPMChunkData.h" // EFPMBiome

#include "FPMBiomePCGConfig.generated.h"

// =====================================================================
//  FFPMBiomeTreeLayer — one type of tree (canopy + optional trunk)
// =====================================================================

/**
 * Describes a single tree variant: canopy mesh, optional trunk mesh,
 * and per-variant density weight (higher = more frequent).
 */
USTRUCT(BlueprintType)
struct FFPMBiomeTreeLayer {
  GENERATED_BODY()

  /** Canopy / foliage mesh for this tree variant. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
  TSoftObjectPtr<UStaticMesh> CanopyMesh;

  /**
   * Optional trunk mesh spawned at the same position as the canopy.
   * Leave null to skip trunk spawning (e.g. for bushes/shrubs).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
  TSoftObjectPtr<UStaticMesh> TrunkMesh;

  /**
   * Relative frequency weight for this variant.
   * Weight 2.0 appears twice as often as weight 1.0.
   * All weights are normalized across variants at runtime.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree",
            meta = (ClampMin = "0.01", ClampMax = "10.0"))
  float Weight = 1.0f;
};

// =====================================================================
//  FFPMBiomeRockLayer — one type of rock/boulder
// =====================================================================

USTRUCT(BlueprintType)
struct FFPMBiomeRockLayer {
  GENERATED_BODY()

  /** Rock / boulder mesh for this variant. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rock")
  TSoftObjectPtr<UStaticMesh> RockMesh;

  /** Relative frequency weight. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rock",
            meta = (ClampMin = "0.01", ClampMax = "10.0"))
  float Weight = 1.0f;
};

// =====================================================================
//  FFPMBiomeGroundCoverLayer — grass, flowers, snow tufts, etc.
// =====================================================================

USTRUCT(BlueprintType)
struct FFPMBiomeGroundCoverLayer {
  GENERATED_BODY()

  /** Ground cover mesh (grass tuft, flower patch, snow clump, etc.). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover")
  TSoftObjectPtr<UStaticMesh> Mesh;

  /** Relative frequency weight. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover",
            meta = (ClampMin = "0.01", ClampMax = "10.0"))
  float Weight = 1.0f;
};

// =====================================================================
//  FFPMBiomeSpawnParams — per-biome settings block
// =====================================================================

/**
 * All spawning parameters for one biome.
 * Add one entry per biome to UFPMBiomePCGConfig::BiomeParams.
 *
 * Trees, Rocks, and GroundCover are independent layers — each
 * generates its own scatter pass with its own density and slope filter.
 * Any layer with Density == 0 or empty mesh list is skipped.
 */
USTRUCT(BlueprintType)
struct FFPMBiomeSpawnParams {
  GENERATED_BODY()

  // ------------------------------------------------------------------
  //  General
  // ------------------------------------------------------------------

  /**
   * Enable / disable spawning for this biome entirely.
   * Faster than setting all densities to 0 during testing.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
  bool bEnabled = true;

  // ------------------------------------------------------------------
  //  Trees
  // ------------------------------------------------------------------

  /**
   * Tree variants for this biome.
   * Each entry has its own canopy mesh, optional trunk, and weight.
   * Leave empty to skip tree spawning.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees")
  TArray<FFPMBiomeTreeLayer> Trees;

  /**
   * Number of tree placement attempts per chunk.
   * Actual spawned count will be lower due to biome/slope rejection.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees",
            meta = (ClampMin = "0", ClampMax = "500"))
  int32 TreeDensity = 0;

  /** Uniform scale range for trees [min, max]. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees")
  FVector2D TreeScaleRange = FVector2D(0.8f, 1.3f);

  /**
   * Z offset applied after mesh-snapping.
   * Negative values sink the mesh into the ground (compensates for
   * pivot offset). Scaled by instance Z-scale.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees",
            meta = (ClampMin = "-500.0", ClampMax = "500.0"))
  float TreeZOffset = -95.0f;

  /**
   * Maximum terrain slope angle for trees (degrees).
   * Points steeper than this are rejected. 90 = no filter.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees",
            meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0",
                    UIMax = "90.0"))
  float TreeMaxSlopeDegrees = 30.0f;

  /**
   * If true, tree HISM uses QueryOnly collision (allows foliage
   * hit-testing). If false (default), NoCollision — much cheaper
   * at high instance counts.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trees")
  bool bTreeCollision = false;

  // ------------------------------------------------------------------
  //  Rocks
  // ------------------------------------------------------------------

  /**
   * Rock/boulder variants for this biome.
   * Rocks always get QueryOnly collision regardless of bTreeCollision.
   * Leave empty to skip rock spawning.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocks")
  TArray<FFPMBiomeRockLayer> Rocks;

  /** Number of rock placement attempts per chunk. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocks",
            meta = (ClampMin = "0", ClampMax = "300"))
  int32 RockDensity = 0;

  /** Uniform scale range for rocks [min, max]. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocks")
  FVector2D RockScaleRange = FVector2D(0.4f, 1.8f);

  /** Z offset after mesh-snapping (negative = sink into ground). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocks",
            meta = (ClampMin = "-500.0", ClampMax = "500.0"))
  float RockZOffset = -20.0f;

  /**
   * Maximum terrain slope for rocks (degrees).
   * Rocks look natural on steeper terrain — default is 55°.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocks",
            meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0",
                    UIMax = "90.0"))
  float RockMaxSlopeDegrees = 55.0f;

  // ------------------------------------------------------------------
  //  Ground Cover (grass, flowers, snow tufts, etc.)
  // ------------------------------------------------------------------

  /**
   * Ground cover mesh variants (grass tufts, flowers, ferns, etc.).
   * These are purely visual — they always use NoCollision.
   * Leave empty to skip ground cover spawning.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover")
  TArray<FFPMBiomeGroundCoverLayer> GroundCover;

  /** Number of ground cover placement attempts per chunk. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover",
            meta = (ClampMin = "0", ClampMax = "2000"))
  int32 GroundCoverDensity = 0;

  /** Uniform scale range for ground cover [min, max]. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover")
  FVector2D GroundCoverScaleRange = FVector2D(0.7f, 1.2f);

  /** Z offset for ground cover after mesh-snapping. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover",
            meta = (ClampMin = "-200.0", ClampMax = "200.0"))
  float GroundCoverZOffset = 0.0f;

  /**
   * Maximum terrain slope for ground cover (degrees).
   * Grass / flowers don't look right on steep slopes — default 20°.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundCover",
            meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0",
                    UIMax = "90.0"))
  float GroundCoverMaxSlopeDegrees = 20.0f;
};

// =====================================================================
//  UFPMBiomePCGConfig — the main data asset
// =====================================================================

/**
 * UFPMBiomePCGConfig
 *
 * Data Asset that drives the PCG biome spawner.
 * One entry per EFPMBiome in BiomeParams — the spawner reads only the
 * entry matching the biome at each scatter point.
 *
 * HOW TO SET UP (Editor):
 *   1. Content Browser → Add → Miscellaneous → Data Asset
 *   2. Pick UFPMBiomePCGConfig
 *   3. In Details, expand BiomeParams → Add entries for each biome
 *   4. Key = EFPMBiome (e.g. Forest, Meadows…)
 *   5. Fill in Trees / Rocks / GroundCover arrays and densities
 *   6. Assign this asset to the WorldChunkManager's BiomePCGConfig slot
 *
 * Biomes with no entry (or bEnabled=false) are silently skipped.
 *
 * See Documents/Technical/Guides/PCG_Biome_Population.md for full setup.
 */
UCLASS(BlueprintType)
class FALDORANPRIMEMMO_API UFPMBiomePCGConfig : public UPrimaryDataAsset {
  GENERATED_BODY()

public:
  // ------------------------------------------------------------------
  //  Per-Biome Settings
  // ------------------------------------------------------------------

  // ------------------------------------------------------------------
  //  Default / Fallback Settings
  // ------------------------------------------------------------------

  /**
   * Default spawn parameters applied to ANY biome NOT listed in BiomeParams.
   *
   * Set your tree and rock meshes here once — every land biome will use
   * them unless you add a specific override entry to BiomeParams or exclude
   * the biome in BiomeExclusions.
   *
   * Typical workflow:
   *   1. Set DefaultBiomeParams.Trees with your common tree variants.
   *   2. Add Ocean / River / Coast / Beach / Desert to BiomeExclusions.
   *   3. Add biome-specific entries to BiomeParams only where you want
   *      different meshes, density, or slope limits.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes|Default")
  FFPMBiomeSpawnParams DefaultBiomeParams;

  /**
   * Biomes that receive NO spawning at all — not even the default.
   * Add water biomes (Ocean, River, Coast, Beach) and any others
   * you want completely empty.
   *
   * Default exclusions you'll typically want:
   *   Ocean, River, Coast, Beach, Desert, Tundra, Snow
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes|Default")
  TSet<EFPMBiome> BiomeExclusions;

  // ------------------------------------------------------------------
  //  Per-Biome Overrides
  // ------------------------------------------------------------------

  /**
   * Per-biome spawn parameter overrides.
   * If a biome is listed here, its entry is used INSTEAD of DefaultBiomeParams.
   * If a biome is NOT listed here AND NOT in BiomeExclusions,
   * DefaultBiomeParams is used.
   *
   * Key   = EFPMBiome enum value
   * Value = FFPMBiomeSpawnParams
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biomes|Overrides")
  TMap<EFPMBiome, FFPMBiomeSpawnParams> BiomeParams;

  // ------------------------------------------------------------------
  //  Global Overrides
  // ------------------------------------------------------------------

  /**
   * Global cull distances [start, end] in cm applied to all HISM
   * components. Instances fade out between StartCull and EndCull.
   * Default: 35000 / 40000 (350 m / 400 m).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance",
            meta = (ClampMin = "0.0"))
  FVector2D CullDistances = FVector2D(35000.f, 40000.f);

  /**
   * If true, ALL ground cover layers cast shadows.
   * Off by default — per-instance shadows on dense foliage are very
   * expensive.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  bool bGroundCoverCastsShadow = false;

  /**
   * If true, ALL tree layers cast shadows.
   * Usually off for performance; enable only if artistically required.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
  bool bTreeCastsShadow = false;

  // ------------------------------------------------------------------
  //  PCG Graph Override (advanced)
  // ------------------------------------------------------------------

  /**
   * Optional PCG Graph asset.
   * If assigned, the spawner SKIPS its internal HISM logic and
   * defers to this graph instead. Leave null to use the built-in
   * HISM spawn system (recommended for current workflow).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
  TSoftObjectPtr<class UPCGGraphInterface> PCGGraphOverride;

  // ------------------------------------------------------------------
  //  UPrimaryDataAsset
  // ------------------------------------------------------------------

  virtual FPrimaryAssetId GetPrimaryAssetId() const override {
    return FPrimaryAssetId(TEXT("BiomePCGConfig"), GetFName());
  }
};
