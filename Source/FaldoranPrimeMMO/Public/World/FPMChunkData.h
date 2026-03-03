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
//  Geodetic Coordinate System — True Spherical Planet
//
//  The world is a sphere with Earth radius (6,371 km).
//  All global positions are stored as (Latitude, Longitude, Altitude)
//  in double precision.  Rendering uses a local tangent plane centered
//  on the player — all chunk meshes are positioned relative to the
//  player to avoid floating-point precision loss.
//
//  See Documents/Technical/SphericalPlanet_Migration.md for full design.
// =====================================================================

/**
 * FFPMGeoCoord
 *
 * Double-precision geodetic coordinate on the spherical planet.
 * Latitude:  -PI/2 (south pole) to +PI/2 (north pole)
 * Longitude: -PI to +PI (wraps seamlessly)
 * Altitude:  centimeters above sea-level sphere (negative = below sea level)
 */
USTRUCT(BlueprintType)
struct FFPMGeoCoord {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  double Latitude = 0.0;  // radians

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  double Longitude = 0.0; // radians

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  double Altitude = 0.0;  // cm above sea level sphere

  FFPMGeoCoord() = default;
  FFPMGeoCoord(double InLat, double InLon, double InAlt = 0.0)
      : Latitude(InLat), Longitude(InLon), Altitude(InAlt) {}

  /** Wrap longitude to [-PI, PI) and clamp latitude to [-PI/2, PI/2]. */
  void Normalize() {
    // Clamp latitude
    Latitude = FMath::Clamp(Latitude, -PI * 0.5, PI * 0.5);
    // Wrap longitude
    while (Longitude > PI)
      Longitude -= 2.0 * PI;
    while (Longitude <= -PI)
      Longitude += 2.0 * PI;
  }

  /** Convert to a 3D unit-sphere point (for seamless noise sampling). */
  FVector3d ToUnitSphere() const {
    const double CosLat = FMath::Cos(Latitude);
    return FVector3d(CosLat * FMath::Cos(Longitude),
                     CosLat * FMath::Sin(Longitude),
                     FMath::Sin(Latitude));
  }

  /** Great-circle surface distance to another point (cm). */
  double GreatCircleDistanceCm(const FFPMGeoCoord &Other) const;

  /** Latitude in degrees (convenience for debug display). */
  double LatDeg() const { return FMath::RadiansToDegrees(Latitude); }
  /** Longitude in degrees. */
  double LonDeg() const { return FMath::RadiansToDegrees(Longitude); }

  FString ToString() const {
    return FString::Printf(TEXT("(%.4f°, %.4f°, alt=%.0f)"),
                           LatDeg(), LonDeg(), Altitude);
  }

  bool operator==(const FFPMGeoCoord &Other) const {
    return FMath::IsNearlyEqual(Latitude, Other.Latitude, 1e-12) &&
           FMath::IsNearlyEqual(Longitude, Other.Longitude, 1e-12);
  }
};

// =====================================================================
//  Chunk Coordinate System — Equirectangular Grid on Sphere
//
//  Chunks tile the sphere using latitude bands and longitude cells.
//  Q = LonCell (column), R = LatBand (row).
//  Longitude cells per band adapt to cos(latitude) to keep chunk
//  ground-size roughly constant (~1.28 km).
//
//  See Documents/Technical/SphericalPlanet_Migration.md
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

  /** Grid distance (Chebyshev on square coords).
   *  For toroidal wrap-aware distance, callers should use
   *  FPMChunkConstants::WrappedChunkDelta before calling this. */
  static int32 HexDistance(const FFPMChunkCoord &A, const FFPMChunkCoord &B) {
    return FMath::Max(FMath::Abs(A.Q - B.Q), FMath::Abs(A.R - B.R));
  }

  /** Grid distance accounting for toroidal wrap.
   *  Requires FPMChunkConstants to be defined (call sites in .cpp files). */
  static int32 WrappedHexDistance(const FFPMChunkCoord &A,
                                  const FFPMChunkCoord &B);

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
 * Spherical planet with Earth-scale dimensions.
 *
 * The rendering plane is always flat and centered on the player.
 * These constants define the sphere geometry, chunk sizing on the
 * sphere surface, and the local tangent-plane parameters.
 */
namespace FPMChunkConstants {

// --- Spherical Planet Geometry (Earth-scale) ---

/** Planet radius in centimeters (Earth = 6,371 km). */
constexpr double PlanetRadiusCm = 637100000.0;

/** Planet radius in km (convenience). */
constexpr double PlanetRadiusKm = 6371.0;

/** Planet circumference in cm (2 * PI * R). */
constexpr double PlanetCircumferenceCm_D = 2.0 * 3.14159265358979323846 * PlanetRadiusCm;

/** Float versions for existing code compatibility. */
constexpr float PlanetCircumferenceCm = 4007500000.0f; // ~40,075 km
constexpr float HalfCircumferenceCm = PlanetCircumferenceCm * 0.5f;
constexpr float PlanetCircumferenceKm = 40075.0f;

// --- Chunk Geometry ---
// Each chunk covers a fixed angular size on the sphere surface.
// At the equator this maps to ~1.28 km ground size.

/** Ground-space size of a chunk at the equator (cm). */
constexpr float ChunkWorldSize = 128000.0f; // 1.28 km

/** Angular size of one chunk in radians (at equator). */
constexpr double ChunkAngularSize =
    static_cast<double>(ChunkWorldSize) / PlanetRadiusCm; // ~0.000201 rad ≈ 0.01152°

/** Number of latitude bands covering pole-to-pole. */
constexpr int32 LatitudeBandCount =
    static_cast<int32>(3.14159265358979323846 / ChunkAngularSize); // ~15,654

/** Maximum longitude cells at equator. */
constexpr int32 MaxLonCellsAtEquator =
    static_cast<int32>(2.0 * 3.14159265358979323846 / ChunkAngularSize); // ~31,309

/** Legacy hex geometry aliases (for PMC mesh building — still uses flat chunks) */
constexpr float HexOuterRadius = 64000.0f;
constexpr float HexInnerRadius = 55425.63f;
constexpr float HexWidth = ChunkWorldSize;
constexpr float HexHeight = 2.0f * HexInnerRadius;
constexpr float HexSpacingX = HexWidth * 0.75f;

/** Total heightmap vertices per chunk (square grid covering chunk bbox) */
constexpr int32 ChunkResolution = 33;
constexpr int32 ChunkVertexCount = ChunkResolution * ChunkResolution;

/** Legacy aliases for backward compat with existing biome/climate code.
 *  StarterIslandWorldSize = full planet circumference (the "island" is the
 *  whole planet now — there are no edges). */
constexpr int32 PlanetChunksPerAxis = MaxLonCellsAtEquator; // ~31,309
constexpr int32 StarterIslandRings = PlanetChunksPerAxis / 2;
constexpr int32 StarterIslandChunksPerAxis = PlanetChunksPerAxis;
constexpr float StarterIslandWorldSize = PlanetCircumferenceCm;

/** Normalized sea level (0.55 = Z=0 in world space). */
constexpr float SeaLevelNormalized = 0.55f;

/** View distance rings (in grid distance from player chunk).
 *  Full:   2 rings ≈ 2.6km  — collision + foliage
 *  Medium: 5 rings ≈ 6.4km  — mid-res mesh
 *  Low:    9 rings ≈ 11.5km — low-res silhouettes */
inline int32 FullDetailRange = 2;
inline int32 MediumDetailRange = 5;
inline int32 LowDetailRange = 9;

/** Island radius fraction — legacy, effectively 1.0 (whole planet). */
inline float IslandRadiusFraction = 1.0f;

// --- Vertical Scale ---
// Sea Level is at Z=0 (and at PlanetRadiusCm from center).
// Depth: -10,994m (Mariana Trench, rounded to -11km)
// Height: +8,849m (Everest, rounded to +9km)
constexpr float MinWorldZ = -1100000.0f;                  // -11 km
constexpr float MaxWorldZ = 900000.0f;                    // +9 km
constexpr float WorldHeightRange = MaxWorldZ - MinWorldZ; // 20 km total

// --- Coordinate Helpers ---
// These still work in local tangent-plane space (cm) for existing code.
// The WorldChunkManager converts geo→local before chunk operations.

/** Get the number of longitude cells at a given latitude band. */
inline int32 LonCellsAtBand(int32 LatBand) {
  // Band center latitude in radians
  const double BandLat = -3.14159265358979323846 * 0.5 +
      (static_cast<double>(LatBand) + 0.5) * ChunkAngularSize;
  const double CosLat = FMath::Abs(FMath::Cos(BandLat));
  const int32 Cells = FMath::Max(1, static_cast<int32>(
      2.0 * 3.14159265358979323846 * CosLat / ChunkAngularSize));
  return Cells;
}

/** Wrap a longitude cell index for a given latitude band. */
inline int32 WrapLonCell(int32 LonCell, int32 LatBand) {
  const int32 Count = LonCellsAtBand(LatBand);
  LonCell = LonCell % Count;
  if (LonCell < 0)
    LonCell += Count;
  return LonCell;
}

/** Clamp a latitude band to valid range [0, LatitudeBandCount). */
inline int32 ClampLatBand(int32 LatBand) {
  return FMath::Clamp(LatBand, 0, LatitudeBandCount - 1);
}

/** Wrap a world-space X or Y coordinate into [0, PlanetCircumferenceCm).
 *  Legacy — used by tangent-plane local coordinates. */
inline float WrapWorldCoord(float V) {
  V = FMath::Fmod(V, PlanetCircumferenceCm);
  if (V < 0.0f)
    V += PlanetCircumferenceCm;
  return V;
}

/** Legacy chunk coord wrap (equatorial count). */
inline int32 WrapChunkCoord(int32 C) {
  C = C % PlanetChunksPerAxis;
  if (C < 0)
    C += PlanetChunksPerAxis;
  return C;
}

/** Shortest signed distance between two local coords on one axis. */
inline float WrappedDelta(float A, float B) {
  float D = B - A;
  if (D > HalfCircumferenceCm)
    D -= PlanetCircumferenceCm;
  else if (D < -HalfCircumferenceCm)
    D += PlanetCircumferenceCm;
  return D;
}

/** Shortest signed distance between two chunk coords on one axis. */
inline int32 WrappedChunkDelta(int32 A, int32 B) {
  const int32 Half = PlanetChunksPerAxis / 2;
  int32 D = B - A;
  if (D > Half)
    D -= PlanetChunksPerAxis;
  else if (D < -Half)
    D += PlanetChunksPerAxis;
  return D;
}

/** Shortest wrapped world-space distance between two 2D positions. */
inline float WrappedDistance2D(const FVector &A, const FVector &B) {
  const float DX = WrappedDelta(A.X, B.X);
  const float DY = WrappedDelta(A.Y, B.Y);
  return FMath::Sqrt(DX * DX + DY * DY);
}

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

  // --- Geodetic Conversions ---

  /** Convert a geodetic coordinate to the chunk's (LatBand, LonCell). */
  static FFPMChunkCoord GeoToChunkCoord(const FFPMGeoCoord &Geo);

  /** Get the geodetic center of a chunk. */
  static FFPMGeoCoord ChunkCoordToGeo(const FFPMChunkCoord &Coord);

  /** Convert a geodetic point to local tangent-plane offset (cm) relative
   *  to a reference geo point.  X = East, Y = North, Z = Up. */
  static FVector GeoToLocal(const FFPMGeoCoord &Reference,
                            const FFPMGeoCoord &Target);

  /** Convert local tangent-plane offset back to geodetic. */
  static FFPMGeoCoord LocalToGeo(const FFPMGeoCoord &Reference,
                                 const FVector &LocalOffset);

  /** Convert a legacy flat-world position (cm) to geodetic.
   *  Used for database migration of saved spawn positions.
   *  Treats X as eastward arc distance, Y as northward arc distance
   *  from the (0,0) origin which maps to Lat=0, Lon=0. */
  static FFPMGeoCoord FlatWorldToGeo(const FVector &FlatPos);

  /** Project geodetic coords to 3D unit-sphere point scaled for noise.
   *  Multiplies by NoiseScale so noise wavelengths correspond to surface km. */
  static FVector3d GeoToNoiseCoord(const FFPMGeoCoord &Geo,
                                   double NoiseScale = 1.0);

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
