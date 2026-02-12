// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMChunkData.generated.h"

/**
 * EFPMBiome
 *
 * Biome types used across the world. Assigned procedurally via noise.
 * Snow requires high elevation, Swamp requires low elevation near water.
 * All other biomes are placed by noise cells and are NOT elevation-locked.
 */
UENUM(BlueprintType)
enum class EFPMBiome : uint8 {
  Meadows = 0 UMETA(DisplayName = "Meadows"),
  Forest UMETA(DisplayName = "Forest"),
  Mountain UMETA(DisplayName = "Mountain"),
  Coast UMETA(DisplayName = "Coast"),
  Swamp UMETA(DisplayName = "Swamp"),
  Snow UMETA(DisplayName = "Snow"),
  Ocean UMETA(DisplayName = "Ocean"),
  MAX UMETA(Hidden)
};

/**
 * FPMChunkCoord
 *
 * Integer coordinate identifying a chunk in the world grid.
 * (0,0) is the origin chunk; negative coords are valid for infinite worlds.
 */
USTRUCT(BlueprintType)
struct FFPMChunkCoord {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  int32 X = 0;

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  int32 Y = 0;

  FFPMChunkCoord() = default;
  FFPMChunkCoord(int32 InX, int32 InY) : X(InX), Y(InY) {}

  bool operator==(const FFPMChunkCoord &Other) const {
    return X == Other.X && Y == Other.Y;
  }

  bool operator!=(const FFPMChunkCoord &Other) const {
    return !(*this == Other);
  }

  friend uint32 GetTypeHash(const FFPMChunkCoord &Coord) {
    return HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
  }

  FString ToString() const { return FString::Printf(TEXT("(%d,%d)"), X, Y); }
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
 */
namespace FPMChunkConstants {
/** Size of a single chunk in Unreal units (cm). 64m = 6400cm */
constexpr float ChunkWorldSize = 6400.0f;

/** Number of heightmap vertices per chunk edge (65 = 64 quads + 1 for
 * stitching) */
constexpr int32 ChunkResolution = 65;

/** Total heightmap vertices per chunk */
constexpr int32 ChunkVertexCount = ChunkResolution * ChunkResolution;

/** Starter island size in chunks per axis */
constexpr int32 StarterIslandChunksPerAxis = 16;

/** Starter island total size in world units (1024m = 102400cm) */
constexpr float StarterIslandWorldSize =
    ChunkWorldSize * StarterIslandChunksPerAxis;

/** View distance rings (in chunks from player) */
constexpr int32 FullDetailRange = 4;   // ~256m radius
constexpr int32 MediumDetailRange = 8; // ~512m radius
constexpr int32 LowDetailRange = 14;   // ~896m radius
// Beyond LowDetailRange = unloaded

/** Island radius as a fraction of the island grid (for the circular mask) */
constexpr float IslandRadiusFraction = 0.42f;
} // namespace FPMChunkConstants

/**
 * FFPMChunkHeightmapData
 *
 * Raw heightmap + biome data for a single chunk.
 * This is pure data — no rendering, no actors. Can be generated
 * deterministically from (ChunkCoord, WorldSeed) at any time.
 */
USTRUCT()
struct FFPMChunkHeightmapData {
  GENERATED_BODY()

  /** Heightmap vertices. Indexed [Y * ChunkResolution + X]. Values are 0.0-1.0
   * normalized. */
  TArray<float> HeightValues;

  /** Biome at each vertex. Indexed same as HeightValues. */
  TArray<EFPMBiome> BiomeValues;

  /** The chunk coordinate this data belongs to */
  FFPMChunkCoord Coord;

  /** Whether this data has been generated */
  bool bIsValid = false;

  void Reset() {
    HeightValues.Empty();
    BiomeValues.Empty();
    bIsValid = false;
  }

  void Allocate() {
    HeightValues.SetNumZeroed(FPMChunkConstants::ChunkVertexCount);
    BiomeValues.SetNum(FPMChunkConstants::ChunkVertexCount);
    FMemory::Memset(BiomeValues.GetData(), 0,
                    BiomeValues.Num() * sizeof(EFPMBiome));
  }
};

/**
 * FPMChunkGenerator
 *
 * Stateless utility class: generates chunk data from coordinates + seed.
 * No side effects, no stored state. The same inputs ALWAYS produce
 * the same output (deterministic).
 *
 * This replaces the old monolithic FPMTerrainGenerator for heightmap/biome
 * generation. The old class's noise functions live here now.
 */
class FALDORANPRIMEMMO_API FPMChunkGenerator {
public:
  /**
   * Generate complete heightmap + biome data for a single chunk.
   * @param Coord     Chunk grid coordinate
   * @param WorldSeed Global world seed
   * @param OutData   Output chunk data (allocated + filled)
   */
  static void GenerateChunk(const FFPMChunkCoord &Coord, int32 WorldSeed,
                            FFPMChunkHeightmapData &OutData);

  /**
   * Get the world-space origin (bottom-left corner) of a chunk.
   * @param Coord Chunk grid coordinate
   * @return World position in cm
   */
  static FVector ChunkToWorldOrigin(const FFPMChunkCoord &Coord);

  /**
   * Convert a world position to the chunk coordinate it falls in.
   * @param WorldPos Position in world space (cm)
   * @return Chunk grid coordinate
   */
  static FFPMChunkCoord WorldToChunkCoord(const FVector &WorldPos);

  /**
   * Convert a world position to normalized island-space (0-1) coordinates.
   * Based on the StarterIsland grid (16x16 chunks centered at origin).
   * @param WorldPos Position in world space
   * @param OutNormX Output normalized X (0-1)
   * @param OutNormY Output normalized Y (0-1)
   */
  static void WorldToIslandNorm(const FVector &WorldPos, float &OutNormX,
                                float &OutNormY);

private:
  // --- Noise Functions (same algorithms as original FPMTerrainGenerator) ---

  /** Hash function for noise generation. Returns 0-1. */
  static float Hash(int32 X, int32 Y, int32 Seed);

  /** 2D value noise with smooth interpolation. Returns 0-1. */
  static float ValueNoise2D(float X, float Y, int32 Seed);

  /** Fractal noise — multiple octaves layered for natural detail. */
  static float FractalNoise(float X, float Y, int32 Seed, int32 Octaves);

  // --- Island Shape ---

  /** Circular island falloff mask. 1.0 at center, 0.0 at edges. */
  static float IslandMask(float NormX, float NormY);

  /** River carve factor. Returns depth to subtract. */
  static float RiverFactor(float NormX, float NormY, int32 Seed);

  // --- Biome Assignment ---

  /** Assign biome from noise at a given normalized position. */
  static EFPMBiome AssignBiomeFromNoise(float NormX, float NormY, int32 Seed,
                                        float IslandMaskValue);

  /** Get elevation bias for a biome type. */
  static float BiomeElevationBias(EFPMBiome Biome);
};
