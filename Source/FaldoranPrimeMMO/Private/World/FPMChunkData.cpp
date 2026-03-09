// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"

// ===================================================================
//  Geodetic Coordinate Helpers
// ===================================================================

double FFPMGeoCoord::GreatCircleDistanceCm(const FFPMGeoCoord &Other) const {
  // Haversine formula
  const double DLat = Other.Latitude - Latitude;
  const double DLon = Other.Longitude - Longitude;
  const double A = FMath::Sin(DLat * 0.5) * FMath::Sin(DLat * 0.5) +
                   FMath::Cos(Latitude) * FMath::Cos(Other.Latitude) *
                       FMath::Sin(DLon * 0.5) * FMath::Sin(DLon * 0.5);
  const double C = 2.0 * FMath::Atan2(FMath::Sqrt(A), FMath::Sqrt(1.0 - A));
  return C * FPMChunkConstants::PlanetRadiusCm;
}

// ===================================================================
//  Coordinate Conversions
// ===================================================================

int32 FFPMChunkCoord::WrappedHexDistance(const FFPMChunkCoord &A,
                                         const FFPMChunkCoord &B) {
  // For spherical grid: R=LatBand uses simple delta (clamped, not wrapped),
  // Q=LonCell wraps per latitude band.
  const int32 DR = FMath::Abs(A.R - B.R);
  // For Q, use the equatorial wrap as a conservative estimate.
  // Full per-band accuracy would require knowing which band we're in.
  const int32 DQ = FMath::Abs(FPMChunkConstants::WrappedChunkDelta(A.Q, B.Q));
  return FMath::Max(DQ, DR);
}

FVector FPMChunkGenerator::ChunkToWorldCenter(const FFPMChunkCoord &Coord) {
  // Map chunk grid to local tangent-plane coordinates.
  // Q and R directly index a flat grid at ChunkWorldSize spacing.
  // The WorldChunkManager re-bases these relative to the player.
  const float CS = FPMChunkConstants::ChunkWorldSize;
  return FVector(static_cast<float>(Coord.Q) * CS + CS * 0.5f,
                 static_cast<float>(Coord.R) * CS + CS * 0.5f, 0.0f);
}

FVector FPMChunkGenerator::ChunkToWorldOrigin(const FFPMChunkCoord &Coord) {
  const float CS = FPMChunkConstants::ChunkWorldSize;
  return FVector(static_cast<float>(Coord.Q) * CS,
                 static_cast<float>(Coord.R) * CS, 0.0f);
}

FFPMChunkCoord FPMChunkGenerator::WorldToChunkCoord(const FVector &WorldPos) {
  const float CS = FPMChunkConstants::ChunkWorldSize;
  return FFPMChunkCoord(FMath::FloorToInt(WorldPos.X / CS),
                        FMath::FloorToInt(WorldPos.Y / CS));
}

void FPMChunkGenerator::WorldToIslandNorm(const FVector &WorldPos,
                                          float &OutNormX, float &OutNormY) {
  const float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  OutNormX =
      (WorldPos.X + HalfIsland) / FPMChunkConstants::StarterIslandWorldSize;
  OutNormY =
      (WorldPos.Y + HalfIsland) / FPMChunkConstants::StarterIslandWorldSize;
}

// ===================================================================
//  Geodetic ↔ Chunk Coord Conversions
// ===================================================================

FFPMChunkCoord FPMChunkGenerator::GeoToChunkCoord(const FFPMGeoCoord &Geo) {
  // LatBand: 0 at south pole (-PI/2), LatitudeBandCount-1 at north pole
  const double LatNorm = (Geo.Latitude + PI * 0.5) / PI; // 0 to 1
  const int32 LatBand = FMath::Clamp(
      static_cast<int32>(LatNorm * FPMChunkConstants::LatitudeBandCount), 0,
      FPMChunkConstants::LatitudeBandCount - 1);

  // LonCell: number of cells at this latitude
  const int32 LonCells = FPMChunkConstants::LonCellsAtBand(LatBand);
  const double LonNorm = (Geo.Longitude + PI) / (2.0 * PI); // 0 to 1
  int32 LonCell = static_cast<int32>(LonNorm * LonCells);
  if (LonCell >= LonCells)
    LonCell = 0; // wrap

  return FFPMChunkCoord(LonCell, LatBand);
}

FFPMGeoCoord FPMChunkGenerator::ChunkCoordToGeo(const FFPMChunkCoord &Coord) {
  // Center latitude of this band
  const double Lat = -PI * 0.5 + (static_cast<double>(Coord.R) + 0.5) *
                                     FPMChunkConstants::ChunkAngularSize;

  // Center longitude of this cell
  const int32 LonCells = FPMChunkConstants::LonCellsAtBand(Coord.R);
  const double LonStep = 2.0 * PI / static_cast<double>(LonCells);
  const double Lon = -PI + (static_cast<double>(Coord.Q) + 0.5) * LonStep;

  return FFPMGeoCoord(Lat, Lon, 0.0);
}

FVector FPMChunkGenerator::GeoToLocal(const FFPMGeoCoord &Reference,
                                      const FFPMGeoCoord &Target) {
  // Local tangent plane: X=East, Y=North, Z=Up
  // Small-angle approximation valid within ~100 km of reference
  const double DLon = Target.Longitude - Reference.Longitude;
  // Wrap DLon to [-PI, PI]
  double WDLon = DLon;
  while (WDLon > PI)
    WDLon -= 2.0 * PI;
  while (WDLon <= -PI)
    WDLon += 2.0 * PI;

  const double DLat = Target.Latitude - Reference.Latitude;
  const double R = FPMChunkConstants::PlanetRadiusCm;
  const double CosRefLat = FMath::Cos(Reference.Latitude);

  const float X = static_cast<float>(WDLon * R * CosRefLat); // East
  const float Y = static_cast<float>(DLat * R);              // North
  const float Z = static_cast<float>(Target.Altitude - Reference.Altitude);

  return FVector(X, Y, Z);
}

FFPMGeoCoord FPMChunkGenerator::LocalToGeo(const FFPMGeoCoord &Reference,
                                           const FVector &LocalOffset) {
  const double R = FPMChunkConstants::PlanetRadiusCm;
  const double CosRefLat = FMath::Cos(Reference.Latitude);
  const double SafeCosLat =
      FMath::Max(CosRefLat, 0.001); // avoid div/0 at poles

  FFPMGeoCoord Result;
  Result.Latitude = Reference.Latitude + static_cast<double>(LocalOffset.Y) / R;
  Result.Longitude = Reference.Longitude +
                     static_cast<double>(LocalOffset.X) / (R * SafeCosLat);
  Result.Altitude = Reference.Altitude + static_cast<double>(LocalOffset.Z);
  Result.Normalize();
  return Result;
}

FFPMGeoCoord FPMChunkGenerator::FlatWorldToGeo(const FVector &FlatPos) {
  // Legacy flat-world: X and Y are cm from origin (0,0).
  // Map to geodetic: X → longitude arc, Y → latitude arc.
  const double R = FPMChunkConstants::PlanetRadiusCm;
  FFPMGeoCoord Geo;
  Geo.Latitude = static_cast<double>(FlatPos.Y) / R;
  Geo.Longitude = static_cast<double>(FlatPos.X) / R;
  Geo.Altitude = static_cast<double>(FlatPos.Z);
  Geo.Normalize();
  return Geo;
}

FVector3d FPMChunkGenerator::GeoToNoiseCoord(const FFPMGeoCoord &Geo,
                                             double NoiseScale) {
  // Project lat/lon to 3D unit sphere, then scale.
  // This gives seamless noise sampling — no seams at date line or poles.
  FVector3d P = Geo.ToUnitSphere();
  return P * (FPMChunkConstants::PlanetRadiusCm * NoiseScale);
}

bool FPMChunkGenerator::IsInsideHex(float LocalX, float LocalY) {
  const float AX = FMath::Abs(LocalX);
  const float AY = FMath::Abs(LocalY);
  const float R = FPMChunkConstants::HexOuterRadius;
  const float IR = FPMChunkConstants::HexInnerRadius;
  if (AX > R || AY > IR)
    return false;
  return (AX * IR + AY * (R * 0.5f)) <= (R * IR);
}

// ===================================================================
//  Legacy Noise Functions (kept for snow/coast overlays in ChunkActor)
// ===================================================================

float FPMChunkGenerator::Hash(int32 X, int32 Y, int32 Seed) {
  int32 H = X * 374761393 + Y * 668265263 + Seed * 1274126177;
  H = (H ^ (H >> 13)) * 1103515245;
  H = H ^ (H >> 16);
  return FMath::Abs(static_cast<float>(H) / 2147483647.0f);
}

float FPMChunkGenerator::ValueNoise2D(float X, float Y, int32 Seed) {
  const int32 IX = FMath::FloorToInt(X);
  const int32 IY = FMath::FloorToInt(Y);
  float FX = X - static_cast<float>(IX);
  float FY = Y - static_cast<float>(IY);
  FX = FX * FX * FX * (FX * (FX * 6.0f - 15.0f) + 10.0f);
  FY = FY * FY * FY * (FY * (FY * 6.0f - 15.0f) + 10.0f);
  const float N00 = Hash(IX, IY, Seed);
  const float N10 = Hash(IX + 1, IY, Seed);
  const float N01 = Hash(IX, IY + 1, Seed);
  const float N11 = Hash(IX + 1, IY + 1, Seed);
  return FMath::Lerp(FMath::Lerp(N00, N10, FX), FMath::Lerp(N01, N11, FX), FY);
}

float FPMChunkGenerator::FractalNoise(float X, float Y, int32 Seed,
                                      int32 Octaves) {
  float Result = 0, Amp = 1, Freq = 1, Total = 0;
  for (int32 i = 0; i < Octaves; ++i) {
    Result += ValueNoise2D(X * Freq, Y * Freq, Seed + i * 7919) * Amp;
    Total += Amp;
    Amp *= 0.5f;
    Freq *= 2.0f;
  }
  return Result / Total;
}

float FPMChunkGenerator::RidgeNoise(float X, float Y, int32 Seed,
                                    int32 Octaves) {
  float Result = 0, Amp = 1, Freq = 1, Total = 0;
  for (int32 i = 0; i < Octaves; ++i) {
    float V = ValueNoise2D(X * Freq, Y * Freq, Seed + i * 7919);
    V = 1.0f - FMath::Abs(2.0f * V - 1.0f);
    V = V * V;
    Result += V * Amp;
    Total += Amp;
    Amp *= 0.5f;
    Freq *= 2.0f;
  }
  return Result / Total;
}

// ===================================================================
//  Island Shape (delegates to FPMNoise)
// ===================================================================

float FPMChunkGenerator::IslandMask(float NormX, float NormY) {
  // Convert norm coords to world coords and delegate
  const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float WX = NormX * FPMChunkConstants::StarterIslandWorldSize - HI;
  const float WY = NormY * FPMChunkConstants::StarterIslandWorldSize - HI;
  return FPMNoise::IslandMask(WX, WY, 0);
}

float FPMChunkGenerator::RiverFactor(float NormX, float NormY, int32 Seed) {
  // Use ridged noise to create natural-looking river networks.
  // Ridge noise = 1 - abs(noise) produces sharp valleys = rivers.
  // Domain warping creates organic meanders.

  // Scale to world-like coords for noise sampling
  // Higher scale = more rivers but each one thinner
  float WX = NormX * 10.0f;
  float WY = NormY * 10.0f;

  // Domain warp for organic meanders
  float WarpX = ValueNoise2D(WX * 0.7f, WY * 0.7f, Seed + 8000) - 0.5f;
  float WarpY =
      ValueNoise2D(WX * 0.7f + 100.0f, WY * 0.7f + 100.0f, Seed + 8500) - 0.5f;
  WX += WarpX * 1.2f;
  WY += WarpY * 1.2f;

  // Ridge noise: sharp valleys where noise crosses zero
  float N1 = ValueNoise2D(WX, WY, Seed + 9000);
  float Ridge1 =
      1.0f - FMath::Abs(N1 * 2.0f - 1.0f); // 0..1, peaks at noise=0.5
  // Pow(6.0) for very narrow, well-defined river channels
  Ridge1 = FMath::Pow(Ridge1, 6.0f);

  // Second octave at different scale for tributaries
  float N2 = ValueNoise2D(WX * 2.3f + 50.0f, WY * 2.3f + 50.0f, Seed + 9500);
  float Ridge2 = 1.0f - FMath::Abs(N2 * 2.0f - 1.0f);
  Ridge2 = FMath::Pow(Ridge2, 6.0f); // Narrow tributaries

  // Combine: main rivers + thin tributaries
  float River = FMath::Max(Ridge1, Ridge2 * 0.20f);

  // High threshold = only the sharpest ridge peaks pass
  const float RiverThreshold = 0.55f;
  if (River < RiverThreshold)
    return 0.0f;

  // Remap from [threshold, 1] to [0, 1]
  return (River - RiverThreshold) / (1.0f - RiverThreshold);
}

// ===================================================================
//  Biome from Climate Fields (replaces threshold-based assignment)
// ===================================================================

static float Smoothstep(float Edge0, float Edge1, float X) {
  float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
  return T * T * (3.0f - 2.0f * T);
}

EFPMBiome FPMChunkGenerator::AssignBiomeFromNoise(float NormX, float NormY,
                                                  int32 Seed,
                                                  float IslandMaskValue) {
  // World coords for climate lookup
  const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float WX = NormX * FPMChunkConstants::StarterIslandWorldSize - HI;
  const float WY = NormY * FPMChunkConstants::StarterIslandWorldSize - HI;

  const float H = FPMNoise::TerrainHeight(WX, WY, Seed);
  const float Temp = FPMNoise::Temperature(WX, WY, Seed);
  const float Moist = FPMNoise::Moisture(WX, WY, Seed);

  // Ocean: terrain below sea level
  constexpr float SeaLevel = FPMChunkConstants::SeaLevelNormalized;
  if (H < SeaLevel - 0.02f)
    return EFPMBiome::Ocean;
  if (H < SeaLevel + 0.005f)
    return (Temp > 0.50f) ? EFPMBiome::Beach : EFPMBiome::Coast;

  float TBias, MBias, EdgeBlend;
  FPMNoise::BiomeRegion(WX, WY, Seed, TBias, MBias, EdgeBlend);

  return AssignBiomeWeighted(Temp, Moist, H, IslandMaskValue, EdgeBlend, Seed);
}

// ===================================================================
//  Biome Ideal Centers in (Temperature, Moisture) climate space
//
//  Each biome has an ideal (T, M) point and a falloff radius.
//  Weight = exp(-dist² / (2 * radius²))  (Gaussian in climate space)
//
//  This produces SOFT MEMBERSHIP: nearby biomes share weight
//  at boundaries, creating smooth transitions instead of hard edges.
// ===================================================================

namespace {

struct FBiomeCenter {
  EFPMBiome Biome;
  float IdealTemp;
  float IdealMoist;
  float Radius; // Falloff radius in climate space (larger = more spread)
};

// 9 climate-grid biomes with ideal centers and falloff radii
static const FBiomeCenter GBiomeCenters[] = {
    // HOT row (Temp ~0.80)
    {EFPMBiome::Desert, 0.80f, 0.20f, 0.22f},
    {EFPMBiome::Savanna, 0.80f, 0.50f, 0.22f},
    {EFPMBiome::Jungle, 0.80f, 0.65f, 0.25f},
    // WARM row (Temp ~0.50)
    {EFPMBiome::Plains, 0.50f, 0.20f, 0.22f},
    {EFPMBiome::Meadows, 0.50f, 0.45f, 0.22f},
    {EFPMBiome::Forest, 0.50f, 0.65f, 0.25f},
    // COLD row (Temp ~0.20)
    {EFPMBiome::Tundra, 0.20f, 0.20f, 0.22f},
    {EFPMBiome::Taiga, 0.20f, 0.45f, 0.22f},
    {EFPMBiome::BorealForest, 0.20f, 0.65f, 0.25f},
};
static constexpr int32 NumClimateBiomes = 9;

/** Compute Gaussian weight for a biome at given climate values. */
float BiomeGaussianWeight(float Temp, float Moist, const FBiomeCenter &Center) {
  const float DT = Temp - Center.IdealTemp;
  const float DM = Moist - Center.IdealMoist;
  const float Dist2 = DT * DT + DM * DM;
  const float R2 = Center.Radius * Center.Radius;
  return FMath::Exp(-Dist2 / (2.0f * R2));
}

/** Per-biome vertex colors (must match FPMVoxelGenerator::BiomeToVertexColor)
 */
FColor BiomeColor(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Meadows:
    return FColor(200, 220, 50, 0);
  case EFPMBiome::Forest:
    return FColor(30, 140, 30, 0);
  case EFPMBiome::Plains:
    return FColor(180, 170, 80, 0);
  case EFPMBiome::Savanna:
    return FColor(200, 160, 60, 0);
  case EFPMBiome::Jungle:
    return FColor(10, 100, 20, 0);
  case EFPMBiome::Desert:
    return FColor(220, 190, 120, 0);
  case EFPMBiome::Taiga:
    return FColor(60, 100, 60, 0);
  case EFPMBiome::BorealForest:
    return FColor(40, 80, 50, 0);
  case EFPMBiome::Tundra:
    return FColor(150, 160, 170, 0);
  case EFPMBiome::Swamp:
    return FColor(80, 100, 40, 0);
  case EFPMBiome::Alpine:
    return FColor(140, 140, 120, 80);
  case EFPMBiome::Mountain:
    return FColor(120, 110, 100, 0);
  case EFPMBiome::Snow:
    return FColor(240, 245, 255, 255);
  case EFPMBiome::River:
    return FColor(60, 90, 80, 0);
  case EFPMBiome::Coast:
    return FColor(120, 150, 140, 0);
  case EFPMBiome::Beach:
    return FColor(230, 210, 160, 0);
  case EFPMBiome::Ocean:
    return FColor(30, 80, 120, 0);
  default:
    return FColor(128, 128, 128, 0);
  }
}

} // anonymous namespace

EFPMBiome FPMChunkGenerator::AssignBiomeWeighted(float Temp, float Moist,
                                                 float Height,
                                                 float IslandMaskValue,
                                                 float EdgeBlend, int32 Seed) {
  if (IslandMaskValue <= 0.01f)
    return EFPMBiome::Ocean;

  // --- Water biomes from elevation (planet mode) ---
  constexpr float SeaLevel = FPMChunkConstants::SeaLevelNormalized;
  if (Height < SeaLevel - 0.02f)
    return EFPMBiome::Ocean;
  if (Height < SeaLevel + 0.005f)
    return (Temp > 0.50f) ? EFPMBiome::Beach : EFPMBiome::Coast;

  // --- Elevation-driven overrides (altitude takes priority) ---
  const float SnowLineBase = 0.74f;
  const float TempShift = (Temp - 0.50f) * 0.10f;
  const float SnowLine = SnowLineBase + TempShift;
  const float AlpineLine = SnowLine - 0.03f;
  const float MountainLine = 0.68f;

  if (Height > SnowLine)
    return EFPMBiome::Snow;
  if (Height > AlpineLine)
    return EFPMBiome::Alpine;
  if (Height > MountainLine)
    return EFPMBiome::Mountain;

  // --- Swamp: low elevation + wet + inland ---
  const float SwampW = (1.0f - Smoothstep(0.56f, 0.58f, Height)) *
                       Smoothstep(0.50f, 0.65f, Moist) *
                       (IslandMaskValue > 0.25f ? 1.0f : 0.0f);
  if (SwampW > 0.5f)
    return EFPMBiome::Swamp;

  // ================================================================
  //  SOFT MEMBERSHIP: Gaussian weights in (T, M) climate space
  //
  //  Instead of hard thresholds, each biome has a Gaussian attractor.
  //  At any point, all 9 climate biomes have a weight.
  //  The winner is the one with the highest weight.
  //
  //  HYSTERESIS: When near a boundary (winner confidence is low AND
  //  we're near a Voronoi region edge), we bias toward the region's
  //  dominant biome to prevent peppering.
  // ================================================================

  float MaxWeight = -1.0f;
  int32 WinnerIdx = 4; // Default: Meadows

  for (int32 I = 0; I < NumClimateBiomes; ++I) {
    const float W = BiomeGaussianWeight(Temp, Moist, GBiomeCenters[I]);
    if (W > MaxWeight) {
      MaxWeight = W;
      WinnerIdx = I;
    }
  }

  return GBiomeCenters[WinnerIdx].Biome;
}

// ===================================================================
//  Soft-Blended Biome Vertex Color
//
//  Instead of picking ONE biome's color, blends all biome colors
//  weighted by their Gaussian membership. This produces smooth
//  color gradients at biome boundaries — no hard edges.
// ===================================================================

FColor FPMChunkGenerator::BlendedBiomeColor(float Temp, float Moist,
                                            float Height,
                                            float IslandMaskValue) {
  // Elevation-driven overrides: use solid colors (no blending needed)
  const float SnowLineBase = 0.74f;
  const float TempShift = (Temp - 0.50f) * 0.10f;
  const float SnowLine = SnowLineBase + TempShift;
  const float AlpineLine = SnowLine - 0.03f;
  const float MountainLine = 0.68f;

  if (IslandMaskValue <= 0.01f)
    return BiomeColor(EFPMBiome::Ocean);
  if (IslandMaskValue < 0.10f)
    return BiomeColor(Temp > 0.50f ? EFPMBiome::Beach : EFPMBiome::Coast);

  // For altitude biomes, blend between them smoothly at transition zones
  if (Height > SnowLine + 0.01f)
    return BiomeColor(EFPMBiome::Snow);
  if (Height > AlpineLine + 0.01f && Height < SnowLine + 0.01f) {
    // Blend Alpine ↔ Snow at the snow line
    float T = Smoothstep(AlpineLine, SnowLine, Height);
    FColor CA = BiomeColor(EFPMBiome::Alpine);
    FColor CS = BiomeColor(EFPMBiome::Snow);
    return FColor(static_cast<uint8>(CA.R + (CS.R - CA.R) * T),
                  static_cast<uint8>(CA.G + (CS.G - CA.G) * T),
                  static_cast<uint8>(CA.B + (CS.B - CA.B) * T),
                  static_cast<uint8>(CA.A + (CS.A - CA.A) * T));
  }
  if (Height > MountainLine + 0.01f) {
    float T = Smoothstep(MountainLine, AlpineLine, Height);
    FColor CM = BiomeColor(EFPMBiome::Mountain);
    FColor CA = BiomeColor(EFPMBiome::Alpine);
    return FColor(static_cast<uint8>(CM.R + (CA.R - CM.R) * T),
                  static_cast<uint8>(CM.G + (CA.G - CM.G) * T),
                  static_cast<uint8>(CM.B + (CA.B - CM.B) * T),
                  static_cast<uint8>(CM.A + (CA.A - CM.A) * T));
  }

  // Swamp
  const float SwampW = (1.0f - Smoothstep(0.56f, 0.58f, Height)) *
                       Smoothstep(0.50f, 0.65f, Moist) *
                       (IslandMaskValue > 0.25f ? 1.0f : 0.0f);
  if (SwampW > 0.8f)
    return BiomeColor(EFPMBiome::Swamp);

  // --- Soft blend of all 9 climate biomes ---
  float Weights[NumClimateBiomes];
  float TotalWeight = 0;

  for (int32 I = 0; I < NumClimateBiomes; ++I) {
    Weights[I] = BiomeGaussianWeight(Temp, Moist, GBiomeCenters[I]);
    TotalWeight += Weights[I];
  }

  if (TotalWeight < 0.0001f)
    return BiomeColor(EFPMBiome::Meadows);

  // Accumulate weighted color
  float R = 0, G = 0, B = 0, A = 0;
  for (int32 I = 0; I < NumClimateBiomes; ++I) {
    const float NW = Weights[I] / TotalWeight;
    if (NW < 0.01f)
      continue; // Skip negligible contributors
    const FColor C = BiomeColor(GBiomeCenters[I].Biome);
    R += C.R * NW;
    G += C.G * NW;
    B += C.B * NW;
    A += C.A * NW;
  }

  // If near the mountain line, blend toward mountain color
  if (Height > MountainLine - 0.03f) {
    float MtnT = Smoothstep(MountainLine - 0.03f, MountainLine, Height);
    FColor CM = BiomeColor(EFPMBiome::Mountain);
    R = FMath::Lerp(R, static_cast<float>(CM.R), MtnT);
    G = FMath::Lerp(G, static_cast<float>(CM.G), MtnT);
    B = FMath::Lerp(B, static_cast<float>(CM.B), MtnT);
    A = FMath::Lerp(A, static_cast<float>(CM.A), MtnT);
  }

  // Partial swamp blend
  if (SwampW > 0.1f) {
    FColor CSw = BiomeColor(EFPMBiome::Swamp);
    R = FMath::Lerp(R, static_cast<float>(CSw.R), SwampW);
    G = FMath::Lerp(G, static_cast<float>(CSw.G), SwampW);
    B = FMath::Lerp(B, static_cast<float>(CSw.B), SwampW);
    A = FMath::Lerp(A, static_cast<float>(CSw.A), SwampW);
  }

  return FColor(static_cast<uint8>(FMath::Clamp(R, 0.0f, 255.0f)),
                static_cast<uint8>(FMath::Clamp(G, 0.0f, 255.0f)),
                static_cast<uint8>(FMath::Clamp(B, 0.0f, 255.0f)),
                static_cast<uint8>(FMath::Clamp(A, 0.0f, 255.0f)));
}

float FPMChunkGenerator::BiomeElevationBias(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Snow:
    return 0.75f;
  case EFPMBiome::Alpine:
    return 0.55f;
  case EFPMBiome::Mountain:
    return 0.50f;
  case EFPMBiome::Forest:
    return 0.20f;
  case EFPMBiome::BorealForest:
    return 0.18f;
  case EFPMBiome::Taiga:
    return 0.15f;
  case EFPMBiome::Jungle:
    return 0.12f;
  case EFPMBiome::Meadows:
    return 0.10f;
  case EFPMBiome::Savanna:
    return 0.10f;
  case EFPMBiome::Plains:
    return 0.08f;
  case EFPMBiome::Tundra:
    return 0.07f;
  case EFPMBiome::Desert:
    return 0.06f;
  case EFPMBiome::Swamp:
    return 0.04f;
  case EFPMBiome::Beach:
    return 0.03f;
  case EFPMBiome::River:
    return 0.05f;
  case EFPMBiome::Coast:
    return 0.03f;
  case EFPMBiome::Ocean:
    return -0.05f;
  default:
    return 0.15f;
  }
}

float FPMChunkGenerator::ContinuousElevationBias(float BiomeNoiseValue) {
  const float T = FMath::Clamp(BiomeNoiseValue, 0.0f, 1.0f);
  return FMath::Lerp(0.02f, 0.80f, T * T * T);
}

// ===================================================================
//  Main Chunk Generation (uses FPMNoise pipeline)
// ===================================================================

void FPMChunkGenerator::GenerateChunk(const FFPMChunkCoord &Coord,
                                      int32 WorldSeed,
                                      FFPMChunkHeightmapData &OutData) {
  OutData.Coord = Coord;
  OutData.Allocate();

  constexpr int32 Res = FPMChunkConstants::ChunkResolution;
  const FVector ChunkCenter = ChunkToWorldCenter(Coord);
  const float BBoxHalf = FPMChunkConstants::ChunkWorldSize * 0.5f;
  constexpr float CellSpacing = FPMChunkConstants::ChunkWorldSize / (Res - 1);

  // ================================================================
  //  PASS 1: Sample terrain height from continuous world function
  // ================================================================
  for (int32 LY = 0; LY < Res; ++LY) {
    for (int32 LX = 0; LX < Res; ++LX) {
      const int32 Idx = LY * Res + LX;
      const float U = static_cast<float>(LX) / (Res - 1);
      const float V = static_cast<float>(LY) / (Res - 1);
      const float WX = ChunkCenter.X + (U * 2.0f - 1.0f) * BBoxHalf;
      const float WY = ChunkCenter.Y + (V * 2.0f - 1.0f) * BBoxHalf;

      // Height from the unified noise pipeline
      OutData.HeightValues[Idx] = FPMNoise::TerrainHeight(WX, WY, WorldSeed);

      // Biome from climate fields
      float NormX, NormY;
      WorldToIslandNorm(FVector(WX, WY, 0), NormX, NormY);
      const float Mask = FPMNoise::IslandMask(WX, WY, WorldSeed);
      OutData.BiomeValues[Idx] =
          AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

      // Continuous biome noise for vertex color blending
      // Use temperature as the blend value (organic, smooth)
      float BV = FPMNoise::Temperature(WX, WY, WorldSeed) * 0.5f +
                 FPMNoise::Moisture(WX, WY, WorldSeed) * 0.3f +
                 OutData.HeightValues[Idx] * 0.2f;
      if (Mask <= 0.01f)
        BV = 0.0f;
      else if (Mask < 0.12f)
        BV = FMath::Min(BV, 0.10f);
      OutData.BiomeNoiseValues[Idx] = FMath::Clamp(BV, 0.0f, 1.0f);
    }
  }

  // ================================================================
  //  PASS 2: Talus Erosion (eliminates cliffs in heightmap)
  // ================================================================
  FPMNoise::TalusErosion(OutData.HeightValues, Res, CellSpacing, 4, 0.65f);

  // ================================================================
  //  PASS 3: Light smoothing
  // ================================================================
  constexpr int32 SmoothPasses = 1;
  constexpr float SmoothStr = 0.10f;
  TArray<float> Buf;
  Buf.SetNumUninitialized(OutData.HeightValues.Num());

  for (int32 P = 0; P < SmoothPasses; ++P) {
    FMemory::Memcpy(Buf.GetData(), OutData.HeightValues.GetData(),
                    Buf.Num() * sizeof(float));
    for (int32 Y = 1; Y < Res - 1; ++Y) {
      for (int32 X = 1; X < Res - 1; ++X) {
        const int32 Idx = Y * Res + X;
        const float C = OutData.HeightValues[Idx];
        const float Avg =
            (OutData.HeightValues[Idx - 1] + OutData.HeightValues[Idx + 1] +
             OutData.HeightValues[Idx - Res] +
             OutData.HeightValues[Idx + Res]) *
            0.25f;
        Buf[Idx] = FMath::Lerp(C, Avg, SmoothStr);
      }
    }
    FMemory::Memcpy(OutData.HeightValues.GetData(), Buf.GetData(),
                    Buf.Num() * sizeof(float));
  }

  // ================================================================
  //  PASS 4: Biome Post-Pass (slope-aware corrections)
  // ================================================================
  constexpr float SnowThreshold = 0.50f;
  constexpr float SteepSlopeThreshold = 1.2f;

  for (int32 Y = 1; Y < Res - 1; ++Y) {
    for (int32 X = 1; X < Res - 1; ++X) {
      const int32 Idx = Y * Res + X;
      const float H = OutData.HeightValues[Idx];
      EFPMBiome &CB = OutData.BiomeValues[Idx];

      // Slope calculation
      float MD = 0;
      MD = FMath::Max(MD, FMath::Abs(H - OutData.HeightValues[Idx - 1]));
      MD = FMath::Max(MD, FMath::Abs(H - OutData.HeightValues[Idx + 1]));
      MD = FMath::Max(MD, FMath::Abs(H - OutData.HeightValues[Idx - Res]));
      MD = FMath::Max(MD, FMath::Abs(H - OutData.HeightValues[Idx + Res]));
      const float Slope =
          (MD * FPMChunkConstants::WorldHeightRange) / CellSpacing;

      // Steep slopes → Mountain rock (snow can't stick to cliffs)
      if (Slope > SteepSlopeThreshold && CB == EFPMBiome::Snow) {
        CB = EFPMBiome::Mountain;
      }
    }
  }

  OutData.bIsValid = true;
  UE_LOG(LogTemp, Verbose, TEXT("FPM: Generated chunk %s"), *Coord.ToString());
}
