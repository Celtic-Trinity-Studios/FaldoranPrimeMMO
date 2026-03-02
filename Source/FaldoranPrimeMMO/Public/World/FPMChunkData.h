// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMChunkData.generated.h"

/**
 * EFPMBiome
 *
 * 16 biome types based on real-world biome classification.
 * Selected via continuous Temperature × Moisture × Height fields.
 *
 * Climate grid (low/med elevation):
 *   HOT+DRY=Desert, HOT+MED=Savanna, HOT+WET=Jungle
 *   WARM+DRY=Plains, WARM+MED=Meadows, WARM+WET=Forest
 *   COLD+DRY=Tundra, COLD+MED=Taiga, COLD+WET=BorealForest
 *   Any+VERYWET+LOW=Swamp
 *
 * Elevation overrides:
 *   HIGH+MODERATE=Alpine, HIGH+STEEP=Mountain, HIGH+COLD=Snow
 *
 * Water:
 *   Ocean, Coast, Beach
 */
UENUM(BlueprintType)
enum class EFPMBiome : uint8 {
  // --- Climate-driven (Temperature × Moisture) ---
  Meadows = 0 UMETA(DisplayName = "Meadows"),        // Warm, medium moisture
  Forest UMETA(DisplayName = "Forest"),              // Warm, high moisture
  Plains UMETA(DisplayName = "Plains"),              // Warm, dry
  Savanna UMETA(DisplayName = "Savanna"),            // Hot, medium moisture
  Jungle UMETA(DisplayName = "Jungle"),              // Hot, high moisture
  Desert UMETA(DisplayName = "Desert"),              // Hot, dry
  Taiga UMETA(DisplayName = "Taiga"),                // Cold, medium moisture
  BorealForest UMETA(DisplayName = "Boreal Forest"), // Cold, high moisture
  Tundra UMETA(DisplayName = "Tundra"),              // Cold, dry
  Swamp UMETA(DisplayName = "Swamp"), // Any temp, very wet, low elevation

  // --- Elevation-driven ---
  Alpine UMETA(DisplayName = "Alpine"),     // High elevation, moderate temp
  Mountain UMETA(DisplayName = "Mountain"), // High elevation, rocky/steep
  Snow UMETA(DisplayName = "Snow"),         // High elevation, cold

  // --- Water features ---
  River UMETA(DisplayName = "River"), // Carved river channel
  Coast UMETA(DisplayName = "Coast"), // Island edge transition
  Beach UMETA(DisplayName = "Beach"), // Sandy shoreline (warm coast)
  Ocean UMETA(DisplayName = "Ocean"), // Deep water

  MAX UMETA(Hidden)
};

// =====================================================================
//  Hexagonal Chunk Coordinate System (Axial / "Trapezoidal")
//
//  Uses FLAT-TOP hexagons with axial coordinates (Q, R).
//  The third cube coordinate S is implicit: S = -Q - R.
//
//  Flat-top hex layout (looking from above):
//
//        ____
//       /    \
//      /      \
//      \      /
//       \____/
//
//  6 neighbors at directions: E, NE, NW, W, SW, SE
//
//  Reference: https://www.redblobgames.com/grids/hexagons/
// =====================================================================

/**
 * FFPMChunkCoord
 *
 * Axial hex coordinate identifying a chunk in the world grid.
 * Q = column axis, R = row axis (flat-top orientation).
 * (0,0) is the origin chunk; negative coords are valid for infinite worlds.
 */
USTRUCT(BlueprintType)
struct FFPMChunkCoord {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  int32 Q = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  int32 R = 0;

  FFPMChunkCoord() = default;
  FFPMChunkCoord(int32 InQ, int32 InR) : Q(InQ), R(InR) {}

  /** Cube coordinate S (implicit) */
  int32 S() const { return -Q - R; }

  bool operator==(const FFPMChunkCoord &Other) const {
    return Q == Other.Q && R == Other.R;
  }

  bool operator!=(const FFPMChunkCoord &Other) const {
    return !(*this == Other);
  }

  friend uint32 GetTypeHash(const FFPMChunkCoord &Coord) {
    return HashCombine(GetTypeHash(Coord.Q), GetTypeHash(Coord.R));
  }

  FString ToString() const { return FString::Printf(TEXT("(%d,%d)"), Q, R); }

  /** Grid distance (Chebyshev on square coords) */
  static int32 HexDistance(const FFPMChunkCoord &A, const FFPMChunkCoord &B) {
    return FMath::Max(FMath::Abs(A.Q - B.Q), FMath::Abs(A.R - B.R));
  }

  /** Number of neighbor directions */
  static constexpr int32 NumDirections = 8;

  /** Get the 8 square neighbor offsets */
  static const FFPMChunkCoord *HexDirections() {
    static const FFPMChunkCoord Dirs[8] = {
        {0, -1},  // N
        {+1, -1}, // NE
        {+1, 0},  // E
        {+1, +1}, // SE
        {0, +1},  // S
        {-1, +1}, // SW
        {-1, 0},  // W
        {-1, -1}, // NW
    };
    return Dirs;
  }

  /** Get neighbor at direction index (0-7) */
  FFPMChunkCoord Neighbor(int32 Dir) const {
    const FFPMChunkCoord *Dirs = HexDirections();
    return FFPMChunkCoord(Q + Dirs[Dir].Q, R + Dirs[Dir].R);
  }
};

/**
 * EFPMChunkLOD
 *
 * Level-of-detail for chunk rendering.
 */
enum class EFPMChunkLOD : uint8 {
  /** Full detail: heightmap, biome data, foliage, collision */
  Full = 0,

  /** Medium: heightmap + biome coloring, no foliage/collision */
  Medium,

  /** Low: simplified heightmap silhouette + biome tint only */
  Low,

  /** Unloaded: not rendered */
  Unloaded
};

/**
 * FPMChunkConstants
 *
 * All chunk-system constants in one place.
 * Now uses hexagonal geometry (flat-top orientation).
 */
namespace FPMChunkConstants {

// --- Hex Geometry ---
// World-simulation scale with 1.28km chunks
// HexOuterRadius = distance from center to any vertex
// HexInnerRadius = distance from center to mid-edge = OuterRadius * sqrt(3)/2

/** Outer radius of each hex chunk (center-to-vertex) in cm */
constexpr float HexOuterRadius = 64000.0f; // 640m

/** Inner radius (center-to-edge, "apothem") */
constexpr float HexInnerRadius = 55425.63f; // 64000 * sqrt(3)/2

/** Hex width (flat-top) = 2 * OuterRadius */
constexpr float HexWidth = 2.0f * HexOuterRadius; // 128000cm = 1.28km

/** Legacy Alias: ChunkWorldSize remains for backward compatibility */
constexpr float ChunkWorldSize = HexWidth;

/** Hex bounding box height = 2 * InnerRadius */
constexpr float HexHeight = 2.0f * HexInnerRadius; // 110851.26cm

/** Hex horizontal spacing between centers (3/4 of width) */
constexpr float HexSpacingX = HexWidth * 0.75f; // 96000cm

/** Total heightmap vertices per chunk (square grid covering hex bbox) */
constexpr int32 ChunkResolution = 33; // Keep memory low
constexpr int32 ChunkVertexCount = ChunkResolution * ChunkResolution;

/** Starter island radius in hex rings from center.
 *  At 1.28km per chunk, 400 rings = 512km radius = 1024km diameter. */
constexpr int32 StarterIslandRings = 400;

/** Maximum number of chunks per axis (diameter in rings) */
constexpr int32 StarterIslandChunksPerAxis = StarterIslandRings * 2 + 1;

/** Approximate starter island world size for island mask etc. */
constexpr float StarterIslandWorldSize = StarterIslandChunksPerAxis * HexWidth;

/** View distance rings (in hex distance from player chunk).
 *  Actors are half-scale, so terrain visually reads as larger — we can
 *  load fewer chunks and still feel like a big world.
 *  Full:   2 rings ≈ 2.6km  — collision + foliage
 *  Medium: 5 rings ≈ 6.4km  — mid-res mesh
 *  Low:    9 rings ≈ 11.5km — low-res silhouettes */
inline int32 FullDetailRange = 2;
inline int32 MediumDetailRange = 5;
inline int32 LowDetailRange = 9;

/** Island radius as a fraction of the island grid.
 *  Override in Config/WorldGen.ini [Terrain] with IslandRadiusFraction. */
inline float IslandRadiusFraction = 0.55f;

// --- Vertical Scale (World Simulation Scale) ---
// Sea Level is at Z=0.
// Depth: -11,000m (Mariana Trench)
// Height: +9,000m (Mount Everest)
constexpr float MinWorldZ = -1100000.0f;                  // -11km
constexpr float MaxWorldZ = 900000.0f;                    // +9km
constexpr float WorldHeightRange = MaxWorldZ - MinWorldZ; // 20km total
} // namespace FPMChunkConstants

/**
 * FFPMChunkHeightmapData
 *
 * Raw heightmap + biome data for a single chunk.
 * This is pure data — no rendering, no actors. Can be generated
 * deterministically from (ChunkCoord, WorldSeed) at any time.
 *
 * The heightmap is stored as a square grid covering the hex bounding box.
 * Vertices outside the hex boundary are masked during mesh construction.
 */
USTRUCT()
struct FFPMChunkHeightmapData {
  GENERATED_BODY()

  /** Heightmap vertices. Indexed [Y * ChunkResolution + X]. Values are 0.0-1.0
   * normalized. */
  TArray<float> HeightValues;

  /** Biome at each vertex. Indexed same as HeightValues. */
  TArray<EFPMBiome> BiomeValues;

  /** Continuous biome noise value (0-1) per vertex for smooth color blending.
   *  This drives gradient vertex colors so biome transitions are organic
   *  rather than hard-edged. Indexed same as HeightValues. */
  TArray<float> BiomeNoiseValues;

  /** The chunk coordinate this data belongs to */
  FFPMChunkCoord Coord;

  /** Whether this data has been generated */
  bool bIsValid = false;

  void Reset() {
    HeightValues.Empty();
    BiomeValues.Empty();
    BiomeNoiseValues.Empty();
    bIsValid = false;
  }

  void Allocate() {
    HeightValues.SetNumZeroed(FPMChunkConstants::ChunkVertexCount);
    BiomeValues.SetNum(FPMChunkConstants::ChunkVertexCount);
    FMemory::Memset(BiomeValues.GetData(), 0,
                    BiomeValues.Num() * sizeof(EFPMBiome));
    BiomeNoiseValues.SetNumZeroed(FPMChunkConstants::ChunkVertexCount);
  }
};

/**
 * FPMChunkGenerator
 *
 * Stateless utility class: generates chunk data from coordinates + seed.
 * Now works with hexagonal axial coordinates.
 */
class FALDORANPRIMEMMO_API FPMChunkGenerator {
public:
  /**
   * Generate complete heightmap + biome data for a single hex chunk.
   * @param Coord     Hex axial coordinate (Q, R)
   * @param WorldSeed Global world seed
   * @param OutData   Output chunk data (allocated + filled)
   */
  static void GenerateChunk(const FFPMChunkCoord &Coord, int32 WorldSeed,
                            FFPMChunkHeightmapData &OutData);

  /**
   * Get the world-space center of a hex chunk (flat-top orientation).
   *
   * Flat-top hex center formula:
   *   X = HexWidth * (Q + R * 0.5)  [with row offset]
   *   Y = HexHeight * R * 0.75      [vertical stagger for flat-top]
   *
   * Actually for flat-top:
   *   X = OuterRadius * 1.5 * Q
   *   Y = InnerRadius * 2 * (R + Q * 0.5)
   *
   * @param Coord Hex axial coordinate
   * @return World position of hex center in cm
   */
  static FVector ChunkToWorldCenter(const FFPMChunkCoord &Coord);

  /**
   * Get the world-space origin (bottom-left of bounding box) of a hex chunk.
   * This is the center minus half the bounding box size.
   * Used for placing the chunk actor.
   *
   * @param Coord Hex axial coordinate
   * @return World position of hex bounding-box corner in cm
   */
  static FVector ChunkToWorldOrigin(const FFPMChunkCoord &Coord);

  /**
   * Convert a world position to the hex chunk coordinate it falls in.
   * Uses pixel-to-hex rounding (Red Blob Games algorithm).
   *
   * @param WorldPos Position in world space (cm)
   * @return Hex axial coordinate
   */
  static FFPMChunkCoord WorldToChunkCoord(const FVector &WorldPos);

  /**
   * Convert a world position to normalized island-space (0-1) coordinates.
   * @param WorldPos Position in world space
   * @param OutNormX Output normalized X (0-1)
   * @param OutNormY Output normalized Y (0-1)
   */
  static void WorldToIslandNorm(const FVector &WorldPos, float &OutNormX,
                                float &OutNormY);

  /**
   * Test if a local XY point (relative to hex center) is inside the hex.
   * @param LocalX X relative to hex center
   * @param LocalY Y relative to hex center
   * @return true if inside the flat-top hexagon
   */
  static bool IsInsideHex(float LocalX, float LocalY);

  // --- Noise Functions (public for reuse by voxel generator) ---

  /** Hash function for noise generation. Returns 0-1. */
  static float Hash(int32 X, int32 Y, int32 Seed);

  /** 2D value noise with smooth interpolation. Returns 0-1. */
  static float ValueNoise2D(float X, float Y, int32 Seed);

  /** Fractal noise — multiple octaves layered for natural detail. */
  static float FractalNoise(float X, float Y, int32 Seed, int32 Octaves);

  /** Ridge noise — 1.0 - abs(Perlin/Simplex) for sharp mountain peaks. */
  static float RidgeNoise(float X, float Y, int32 Seed, int32 Octaves);

  // --- Island Shape ---

  /** Circular island falloff mask. 1.0 at center, 0.0 at edges. */
  static float IslandMask(float NormX, float NormY);

  // --- Biome Assignment ---

  /** Assign biome from noise at a given normalized position. */
  static EFPMBiome AssignBiomeFromNoise(float NormX, float NormY, int32 Seed,
                                        float IslandMaskValue);

  /** Assign biome from pre-computed climate values.
   *  Used by the voxel pipeline after climate grid smoothing.
   *  EdgeBlend is from BiomeRegion (0=boundary, 1=deep inside). */
  static EFPMBiome AssignBiomeWeighted(float Temp, float Moist, float Height,
                                       float IslandMaskValue, float EdgeBlend,
                                       int32 Seed);

  /** Compute a soft-blended vertex color from climate values.
   *  Instead of picking one biome's color, blends nearby biome colors
   *  weighted by climate-space proximity. Eliminates hard color boundaries. */
  static FColor BlendedBiomeColor(float Temp, float Moist, float Height,
                                  float IslandMaskValue);

  /** Get elevation bias for a biome type. */
  static float BiomeElevationBias(EFPMBiome Biome);

  /** Returns a smooth elevation bias from the continuous biome noise value
   *  (avoids discrete jumps at biome boundaries). */
  static float ContinuousElevationBias(float BiomeNoiseValue);

  /** River carve factor at normalized position. Returns depth to subtract. */
  static float RiverFactor(float NormX, float NormY, int32 Seed);
};
