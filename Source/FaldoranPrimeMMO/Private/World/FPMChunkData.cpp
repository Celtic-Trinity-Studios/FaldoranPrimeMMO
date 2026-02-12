// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkData.h"

// ===================================================================
//  Coordinate Conversions
// ===================================================================

FVector FPMChunkGenerator::ChunkToWorldOrigin(const FFPMChunkCoord &Coord) {
  // Chunks are centered on the world origin for the starter island.
  // The island grid goes from chunk (0,0) to (15,15) for a 16x16 grid.
  // We center it so chunk (8,8) is roughly at world origin.
  const float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float WorldX = Coord.X * FPMChunkConstants::ChunkWorldSize - HalfIsland;
  const float WorldY = Coord.Y * FPMChunkConstants::ChunkWorldSize - HalfIsland;
  return FVector(WorldX, WorldY, 0.0f);
}

FFPMChunkCoord FPMChunkGenerator::WorldToChunkCoord(const FVector &WorldPos) {
  const float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const int32 ChunkX = FMath::FloorToInt((WorldPos.X + HalfIsland) /
                                         FPMChunkConstants::ChunkWorldSize);
  const int32 ChunkY = FMath::FloorToInt((WorldPos.Y + HalfIsland) /
                                         FPMChunkConstants::ChunkWorldSize);
  return FFPMChunkCoord(ChunkX, ChunkY);
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
//  Noise Functions (ported from original FPMTerrainGenerator)
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

  // Smoothstep
  FX = FX * FX * (3.0f - 2.0f * FX);
  FY = FY * FY * (3.0f - 2.0f * FY);

  const float N00 = Hash(IX, IY, Seed);
  const float N10 = Hash(IX + 1, IY, Seed);
  const float N01 = Hash(IX, IY + 1, Seed);
  const float N11 = Hash(IX + 1, IY + 1, Seed);

  const float NX0 = FMath::Lerp(N00, N10, FX);
  const float NX1 = FMath::Lerp(N01, N11, FX);
  return FMath::Lerp(NX0, NX1, FY);
}

float FPMChunkGenerator::FractalNoise(float X, float Y, int32 Seed,
                                      int32 Octaves) {
  float Result = 0.0f;
  float Amplitude = 1.0f;
  float Frequency = 1.0f;
  float TotalAmplitude = 0.0f;

  for (int32 i = 0; i < Octaves; ++i) {
    Result +=
        ValueNoise2D(X * Frequency, Y * Frequency, Seed + i * 7919) * Amplitude;
    TotalAmplitude += Amplitude;
    Amplitude *= 0.5f;
    Frequency *= 2.0f;
  }

  return Result / TotalAmplitude;
}

// ===================================================================
//  Island Shape Functions
// ===================================================================

float FPMChunkGenerator::IslandMask(float NormX, float NormY) {
  const float DX = NormX - 0.5f;
  const float DY = NormY - 0.5f;
  const float Dist =
      FMath::Sqrt(DX * DX + DY * DY) / FPMChunkConstants::IslandRadiusFraction;

  if (Dist >= 1.0f) {
    return 0.0f;
  }

  // Use a gradual slope that starts ramping down earlier.
  // The "shore zone" begins at 40% of the island radius and slopes
  // gently to zero, creating a natural beach-like transition.
  constexpr float ShoreStart = 0.40f; // Start sloping at 40% of radius
  if (Dist > ShoreStart) {
    // Remap [ShoreStart, 1.0] to [1.0, 0.0]
    const float T = (1.0f - Dist) / (1.0f - ShoreStart);
    // Gentle power curve for smooth coastal ramp
    return T * T; // T^2 — very gradual near the shore
  }

  // Interior: full height (slightly below 1.0 to avoid a flat plateau)
  // Smoothly blend from 1.0 at center to 1.0 at ShoreStart
  return 1.0f;
}

float FPMChunkGenerator::RiverFactor(float NormX, float NormY, int32 Seed) {
  const float BaseX = 0.45f + NormY * 0.12f;
  const float CurveNoise = ValueNoise2D(0.0f, NormY * 8.0f, Seed + 5000);
  const float RiverCenterX = BaseX + (CurveNoise - 0.5f) * 0.12f;
  const float Dist = FMath::Abs(NormX - RiverCenterX);
  const float RiverWidth = 0.005f + NormY * 0.005f;

  if (Dist > RiverWidth) {
    return 0.0f;
  }

  const float T = Dist / RiverWidth;
  return (1.0f - T * T) * 0.15f;
}

// ===================================================================
//  Biome Assignment
// ===================================================================

EFPMBiome FPMChunkGenerator::AssignBiomeFromNoise(float NormX, float NormY,
                                                  int32 Seed,
                                                  float IslandMaskValue) {
  // Ocean: outside the island entirely
  if (IslandMaskValue <= 0.01f) {
    return EFPMBiome::Ocean;
  }

  // Coast: near the island edge
  if (IslandMaskValue < 0.20f) {
    return EFPMBiome::Coast;
  }

  // Noise-based biome assignment for interior
  constexpr float BiomeScale1 = 3.5f;
  constexpr float BiomeScale2 = 5.0f;
  constexpr int32 BiomeSeedOffset = 99999;

  const float N1 = FractalNoise(NormX * BiomeScale1, NormY * BiomeScale1,
                                Seed + BiomeSeedOffset, 3);
  const float N2 = FractalNoise(NormX * BiomeScale2, NormY * BiomeScale2,
                                Seed + BiomeSeedOffset + 7777, 2);

  const float BiomeValue = N1 * 0.6f + N2 * 0.4f;

  if (BiomeValue < 0.40f) {
    return EFPMBiome::Meadows;
  }
  if (BiomeValue < 0.75f) {
    return EFPMBiome::Forest;
  }
  return EFPMBiome::Mountain;
}

float FPMChunkGenerator::BiomeElevationBias(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Mountain:
    return 0.10f; // was 0.18 — gentler hills
  case EFPMBiome::Forest:
    return 0.05f; // was 0.08
  case EFPMBiome::Meadows:
    return 0.03f;
  case EFPMBiome::Coast:
    return 0.01f; // was -0.01 — slight positive so coast isn't below water
  case EFPMBiome::Swamp:
    return 0.02f; // was 0.00
  case EFPMBiome::Snow:
    return 0.15f; // was 0.25
  case EFPMBiome::Ocean:
    return -0.03f; // was -0.10 — less extreme ocean drop
  default:
    return 0.04f; // was 0.05
  }
}

// ===================================================================
//  Main Chunk Generation
// ===================================================================

void FPMChunkGenerator::GenerateChunk(const FFPMChunkCoord &Coord,
                                      int32 WorldSeed,
                                      FFPMChunkHeightmapData &OutData) {
  OutData.Coord = Coord;
  OutData.Allocate();

  constexpr int32 Res = FPMChunkConstants::ChunkResolution;
  constexpr float NoiseScale = 6.0f;

  // Pre-compute the world origin of this chunk
  const FVector ChunkOrigin = ChunkToWorldOrigin(Coord);

  for (int32 LocalY = 0; LocalY < Res; ++LocalY) {
    for (int32 LocalX = 0; LocalX < Res; ++LocalX) {
      const int32 Idx = LocalY * Res + LocalX;

      // World position of this vertex
      const float VertexWorldX =
          ChunkOrigin.X + (static_cast<float>(LocalX) / (Res - 1)) *
                              FPMChunkConstants::ChunkWorldSize;
      const float VertexWorldY =
          ChunkOrigin.Y + (static_cast<float>(LocalY) / (Res - 1)) *
                              FPMChunkConstants::ChunkWorldSize;

      // Convert to normalized island-space (0-1)
      float NormX, NormY;
      WorldToIslandNorm(FVector(VertexWorldX, VertexWorldY, 0.0f), NormX,
                        NormY);

      // --- Island mask ---
      const float Mask = IslandMask(NormX, NormY);

      // --- Biome assignment (noise-based, NOT elevation-locked) ---
      EFPMBiome Biome = AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

      // --- Heightmap generation ---
      const float Noise =
          FractalNoise(NormX * NoiseScale, NormY * NoiseScale, WorldSeed, 6);
      const float BiomeBias = BiomeElevationBias(Biome);

      float LandHeight = (Noise * 0.3f + BiomeBias * 0.5f) * Mask;

      // Meadows flattening
      if (Biome == EFPMBiome::Meadows && LandHeight > 0.02f) {
        constexpr float MeadowTarget = 0.06f;
        LandHeight = FMath::Lerp(LandHeight, MeadowTarget, 0.80f);
      }

      // Mountain peak detail (subtle, not extreme)
      if (Biome == EFPMBiome::Mountain) {
        const float PeakNoise =
            FractalNoise(NormX * NoiseScale * 2.0f, NormY * NoiseScale * 2.0f,
                         WorldSeed + 12345, 4);
        LandHeight += PeakNoise * 0.12f * Mask; // Was 0.3 — much gentler peaks
      }

      // --- River: paint at surface level, don't carve a valley ---
      const float River = RiverFactor(NormX, NormY, WorldSeed);
      if (River > 0.02f && Mask > 0.15f) {
        // Override biome to Coast — renders as water-colored terrain
        Biome = EFPMBiome::Coast;
        // Gently flatten the river bed so it's a smooth level strip
        constexpr float RiverBedHeight = 0.05f;
        LandHeight = FMath::Lerp(LandHeight, RiverBedHeight,
                                 FMath::Clamp(River * 8.0f, 0.0f, 0.85f));
      }

      // Clamp to prevent extreme spikes regardless of seed
      LandHeight = FMath::Clamp(LandHeight, 0.0f, 0.55f);

      // --- Elevation-based biome overrides ---
      // Snow: mountain terrain above snow line
      if (Biome == EFPMBiome::Mountain && LandHeight > 0.40f) {
        Biome = EFPMBiome::Snow;
      }
      // Swamp: low elevation interior (not on rivers or coast)
      else if (LandHeight > 0.0f && LandHeight < 0.05f &&
               Biome != EFPMBiome::Coast && Biome != EFPMBiome::Ocean) {
        Biome = EFPMBiome::Swamp;
      }

      // Store results
      OutData.HeightValues[Idx] = LandHeight;
      OutData.BiomeValues[Idx] = Biome;
    }
  }

  // =================================================================
  //  Post-Generation: Heightmap smoothing (eliminates stair-step cliffs)
  //
  //  Average each vertex with its neighbors to prevent steep height jumps.
  //  Multiple passes spread the effect, turning cliffs into gentle slopes.
  //
  //  IMPORTANT: Do NOT smooth edge vertices (first/last row & column).
  //  Edge vertices must keep their original generated heights so they
  //  match the neighboring chunk's shared edge exactly (no tearing).
  // =================================================================
  constexpr int32 SmoothPasses = 6;
  constexpr float SmoothStrength = 0.6f;

  const int32 SmoothRes = FPMChunkConstants::ChunkResolution;
  TArray<float> Smoothed;
  Smoothed.SetNumUninitialized(OutData.HeightValues.Num());

  for (int32 Pass = 0; Pass < SmoothPasses; ++Pass) {
    // Copy all values first (edges will stay unchanged)
    FMemory::Memcpy(Smoothed.GetData(), OutData.HeightValues.GetData(),
                    Smoothed.Num() * sizeof(float));

    // Only smooth interior vertices — skip edges to prevent chunk seams
    for (int32 Y = 1; Y < SmoothRes - 1; ++Y) {
      for (int32 X = 1; X < SmoothRes - 1; ++X) {
        const int32 Idx = Y * SmoothRes + X;
        const float Center = OutData.HeightValues[Idx];

        // 4-neighbor average (all neighbors guaranteed to exist for interior)
        const float NeighborAvg =
            (OutData.HeightValues[Idx - 1] + OutData.HeightValues[Idx + 1] +
             OutData.HeightValues[Idx - SmoothRes] +
             OutData.HeightValues[Idx + SmoothRes]) *
            0.25f;

        Smoothed[Idx] = FMath::Lerp(Center, NeighborAvg, SmoothStrength);
      }
    }
    // Copy smoothed back for next pass
    FMemory::Memcpy(OutData.HeightValues.GetData(), Smoothed.GetData(),
                    Smoothed.Num() * sizeof(float));
  }

  OutData.bIsValid = true;

  UE_LOG(LogTemp, Verbose, TEXT("FPM: Generated chunk %s"), *Coord.ToString());
}
