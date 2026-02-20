// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkData.h"

// ===================================================================
//  Hexagonal Coordinate Conversions (Flat-Top Orientation)
//
//  Flat-top hex center in world space:
//    X = OuterRadius * 1.5 * Q
//    Y = InnerRadius * 2.0 * (R + Q * 0.5)
//
//  Ref: https://www.redblobgames.com/grids/hexagons/#coordinates-offset
// ===================================================================

FVector FPMChunkGenerator::ChunkToWorldCenter(const FFPMChunkCoord &Coord) {
  // Square grid: center = (Q * ChunkSize + half, R * ChunkSize + half)
  const float CS = FPMChunkConstants::ChunkWorldSize;
  const float X = static_cast<float>(Coord.Q) * CS + CS * 0.5f;
  const float Y = static_cast<float>(Coord.R) * CS + CS * 0.5f;
  return FVector(X, Y, 0.0f);
}

FVector FPMChunkGenerator::ChunkToWorldOrigin(const FFPMChunkCoord &Coord) {
  // Square grid: origin = (Q * ChunkSize, R * ChunkSize)
  const float CS = FPMChunkConstants::ChunkWorldSize;
  return FVector(static_cast<float>(Coord.Q) * CS,
                 static_cast<float>(Coord.R) * CS, 0.0f);
}

FFPMChunkCoord FPMChunkGenerator::WorldToChunkCoord(const FVector &WorldPos) {
  // Square grid: simple floor division
  const float CS = FPMChunkConstants::ChunkWorldSize;
  const int32 Q = FMath::FloorToInt(WorldPos.X / CS);
  const int32 R = FMath::FloorToInt(WorldPos.Y / CS);
  return FFPMChunkCoord(Q, R);
}

void FPMChunkGenerator::WorldToIslandNorm(const FVector &WorldPos,
                                          float &OutNormX, float &OutNormY) {
  // Normalize relative to island center (world origin)
  // Range is roughly -IslandWorldSize/2 to +IslandWorldSize/2
  const float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  OutNormX =
      (WorldPos.X + HalfIsland) / FPMChunkConstants::StarterIslandWorldSize;
  OutNormY =
      (WorldPos.Y + HalfIsland) / FPMChunkConstants::StarterIslandWorldSize;
}

bool FPMChunkGenerator::IsInsideHex(float LocalX, float LocalY) {
  // Flat-top hex test (point relative to hex CENTER).
  // A flat-top regular hexagon with outer radius R has vertices at:
  //   (R, 0), (R/2, R*sqrt3/2), (-R/2, R*sqrt3/2),
  //   (-R, 0), (-R/2, -R*sqrt3/2), (R/2, -R*sqrt3/2)
  //
  // The hex is bounded by 3 constraint pairs:
  //   |X| <= R
  //   |Y| <= InnerRadius
  //   |X| * InnerRadius + |Y| * R/2 <= R * InnerRadius  (the angled edges)
  //
  // Simplified: test each of the 3 axis pairs for a regular hexagon.

  const float AX = FMath::Abs(LocalX);
  const float AY = FMath::Abs(LocalY);
  const float R = FPMChunkConstants::HexOuterRadius;
  const float IR = FPMChunkConstants::HexInnerRadius;

  // Quick reject
  if (AX > R || AY > IR) {
    return false;
  }

  // Angled edge test: the slanted sides
  // For flat-top hex, the slanted edges satisfy:
  //   AX * IR + AY * (R * 0.5) <= R * IR
  return (AX * IR + AY * (R * 0.5f)) <= (R * IR);
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

float FPMChunkGenerator::RidgeNoise(float X, float Y, int32 Seed,
                                    int32 Octaves) {
  float Result = 0.0f;
  float Amplitude = 1.0f;
  float Frequency = 1.0f;
  float TotalAmplitude = 0.0f;

  for (int32 i = 0; i < Octaves; ++i) {
    float V = ValueNoise2D(X * Frequency, Y * Frequency, Seed + i * 7919);
    V = 1.0f - FMath::Abs(2.0f * V - 1.0f);
    V = V * V;

    Result += V * Amplitude;
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

  // Smooth shore transition: terrain is full-strength in the inner 30%,
  // then falls off smoothly using quintic smoothstep to the edge.
  constexpr float ShoreStart = 0.30f;
  if (Dist > ShoreStart) {
    const float T = (1.0f - Dist) / (1.0f - ShoreStart);
    // Quintic smoothstep: 6t^5 - 15t^4 + 10t^3  (smoother than cubic)
    return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
  }

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
  if (IslandMaskValue <= 0.01f) {
    return EFPMBiome::Ocean;
  }

  if (IslandMaskValue < 0.20f) {
    return EFPMBiome::Coast;
  }

  constexpr float BiomeScale1 = 3.5f;
  constexpr float BiomeScale2 = 5.0f;
  constexpr float BiomeScale3 = 12.0f; // high-freq detail for jagged edges
  constexpr int32 BiomeSeedOffset = 99999;

  // Domain warping: distort coordinates to break up smooth round boundaries
  constexpr float WarpScale = 4.0f;
  constexpr float WarpStrength = 0.08f; // how far to push (in normalized space)
  const float WarpX =
      FractalNoise(NormX * WarpScale, NormY * WarpScale, Seed + 55555, 3) *
      WarpStrength;
  const float WarpY =
      FractalNoise(NormX * WarpScale + 100.0f, NormY * WarpScale + 100.0f,
                   Seed + 66666, 3) *
      WarpStrength;
  const float WarpedX = NormX + WarpX;
  const float WarpedY = NormY + WarpY;

  const float N1 = FractalNoise(WarpedX * BiomeScale1, WarpedY * BiomeScale1,
                                Seed + BiomeSeedOffset, 3);
  const float N2 = FractalNoise(WarpedX * BiomeScale2, WarpedY * BiomeScale2,
                                Seed + BiomeSeedOffset + 7777, 2);
  // High-freq edge noise — adds fine-scale irregularity
  const float N3 = FractalNoise(WarpedX * BiomeScale3, WarpedY * BiomeScale3,
                                Seed + BiomeSeedOffset + 33333, 2);

  const float BiomeValue = N1 * 0.50f + N2 * 0.30f + N3 * 0.20f;

  // Swamp: low-lying interior areas (low noise, deep inside the island)
  if (BiomeValue < 0.25f && IslandMaskValue > 0.40f) {
    return EFPMBiome::Swamp;
  }
  if (BiomeValue < 0.38f) {
    return EFPMBiome::Meadows;
  }
  if (BiomeValue < 0.58f) {
    return EFPMBiome::Forest;
  }
  // Snow: highest peaks (very high noise, well inside the island)
  if (BiomeValue > 0.72f && IslandMaskValue > 0.50f) {
    return EFPMBiome::Snow;
  }
  return EFPMBiome::Mountain;
}

float FPMChunkGenerator::BiomeElevationBias(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Mountain:
    return 0.28f; // Prominent peaks
  case EFPMBiome::Forest:
    return 0.12f; // Rolling hills
  case EFPMBiome::Meadows:
    return 0.06f; // Gentle undulation
  case EFPMBiome::Coast:
    return 0.02f; // Near sea level
  case EFPMBiome::Swamp:
    return 0.04f; // Low-lying
  case EFPMBiome::Snow:
    return 0.38f; // Highest peaks
  case EFPMBiome::Ocean:
    return -0.05f; // Below sea level
  default:
    return 0.08f;
  }
}

// ===================================================================
//  Continuous elevation bias — smooth curve from biome noise value.
//  Avoids discrete jumps at biome boundaries.
// ===================================================================

float FPMChunkGenerator::ContinuousElevationBias(float BiomeNoiseValue) {
  const float T = FMath::Clamp(BiomeNoiseValue, 0.0f, 1.0f);
  // Quadratic ramp: gentle at low values, steeper at high
  // T=0.0 -> 0.02  (coast level)
  // T=0.4 -> 0.08  (meadow)
  // T=0.6 -> 0.16  (foothills)
  // T=0.8 -> 0.28  (mountain)
  // T=1.0 -> 0.40  (snow peak)
  constexpr float MinBias = 0.02f;
  constexpr float MaxBias = 0.40f;
  return FMath::Lerp(MinBias, MaxBias, T * T);
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
  constexpr float NoiseScale = 2.5f;

  // Get the world-space center of this hex chunk
  const FVector ChunkCenter = ChunkToWorldCenter(Coord);

  TArray<float> MaxSlopes;
  MaxSlopes.SetNumZeroed(OutData.HeightValues.Num());

  // The heightmap covers the hex's bounding box.
  // We generate a square grid and mark vertices outside the hex as Ocean.
  const float BBoxHalfW = FPMChunkConstants::ChunkWorldSize * 0.5f;
  const float BBoxHalfH = FPMChunkConstants::ChunkWorldSize * 0.5f;

  for (int32 LocalY = 0; LocalY < Res; ++LocalY) {
    for (int32 LocalX = 0; LocalX < Res; ++LocalX) {
      const int32 Idx = LocalY * Res + LocalX;

      // UV within the bounding box (0 to 1)
      const float U = static_cast<float>(LocalX) / (Res - 1);
      const float V = static_cast<float>(LocalY) / (Res - 1);

      // Local position relative to hex center
      const float HexLocalX = (U * 2.0f - 1.0f) * BBoxHalfW;
      const float HexLocalY = (V * 2.0f - 1.0f) * BBoxHalfH;

      // World position of this vertex
      const float VertexWorldX = ChunkCenter.X + HexLocalX;
      const float VertexWorldY = ChunkCenter.Y + HexLocalY;

      // Square grid: no hex boundary rejection needed

      // Convert to normalized island-space (0-1)
      float NormX, NormY;
      WorldToIslandNorm(FVector(VertexWorldX, VertexWorldY, 0.0f), NormX,
                        NormY);

      // --- Island mask ---
      const float Mask = IslandMask(NormX, NormY);

      // --- Biome assignment ---
      EFPMBiome Biome = AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

      // --- Heightmap generation (continuous, no hard boundaries) ---
      // Compute the same warped BiomeValue used for biome assignment so we can
      // smoothly drive elevation bias and noise type blending.
      constexpr float BiomeScale1 = 3.5f;
      constexpr float BiomeScale2 = 5.0f;
      constexpr float BiomeScale3 = 12.0f;
      constexpr int32 BiomeSeedOffset = 99999;
      constexpr float WarpScale = 4.0f;
      constexpr float WarpStrength = 0.08f;
      const float WarpX = FractalNoise(NormX * WarpScale, NormY * WarpScale,
                                       WorldSeed + 55555, 3) *
                          WarpStrength;
      const float WarpY =
          FractalNoise(NormX * WarpScale + 100.0f, NormY * WarpScale + 100.0f,
                       WorldSeed + 66666, 3) *
          WarpStrength;
      const float WarpedX = NormX + WarpX;
      const float WarpedY = NormY + WarpY;
      const float BN1 =
          FractalNoise(WarpedX * BiomeScale1, WarpedY * BiomeScale1,
                       WorldSeed + BiomeSeedOffset, 3);
      const float BN2 =
          FractalNoise(WarpedX * BiomeScale2, WarpedY * BiomeScale2,
                       WorldSeed + BiomeSeedOffset + 7777, 2);
      const float BN3 =
          FractalNoise(WarpedX * BiomeScale3, WarpedY * BiomeScale3,
                       WorldSeed + BiomeSeedOffset + 33333, 2);
      const float BiomeValue = BN1 * 0.50f + BN2 * 0.30f + BN3 * 0.20f;

      // Smooth noise blend: fractal (plains) → ridge (mountains)
      // Blend factor rises smoothly from 0 at BiomeValue=0.3 to 1 at 0.7
      const float RidgeBlend =
          FMath::Clamp((BiomeValue - 0.30f) / 0.40f, 0.0f, 1.0f);

      const float FlatNoise =
          FractalNoise(NormX * NoiseScale, NormY * NoiseScale, WorldSeed, 5);
      const float MtNoise =
          RidgeNoise(NormX * NoiseScale, NormY * NoiseScale, WorldSeed, 6);
      float LandHeight = FMath::Lerp(FlatNoise, MtNoise, RidgeBlend);

      // Continuous elevation bias (smooth curve, no jumps)
      const float BiomeBias = ContinuousElevationBias(BiomeValue);
      LandHeight = (LandHeight * 0.6f + BiomeBias * 0.4f);
      LandHeight *= Mask;

      if (Biome == EFPMBiome::Ocean || Mask <= 0.05f) {
        LandHeight = FMath::Lerp(LandHeight, 0.0f, 0.9f);
      }

      if (Biome == EFPMBiome::Meadows || Biome == EFPMBiome::Swamp) {
        LandHeight = FMath::Lerp(LandHeight, 0.1f, 0.3f);
      }

      OutData.HeightValues[Idx] = FMath::Clamp(LandHeight, 0.0f, 1.0f);
      OutData.BiomeValues[Idx] = Biome;
    }
  }

  // === Post-Generation: Smoothing ===
  constexpr int32 SmoothPasses = 4;
  constexpr float SmoothStrength = 0.4f;
  const int32 SmoothRes = FPMChunkConstants::ChunkResolution;
  TArray<float> Smoothed;
  Smoothed.SetNumUninitialized(OutData.HeightValues.Num());

  for (int32 Pass = 0; Pass < SmoothPasses; ++Pass) {
    FMemory::Memcpy(Smoothed.GetData(), OutData.HeightValues.GetData(),
                    Smoothed.Num() * sizeof(float));

    for (int32 Y = 1; Y < SmoothRes - 1; ++Y) {
      for (int32 X = 1; X < SmoothRes - 1; ++X) {
        const int32 Idx = Y * SmoothRes + X;
        const float Center = OutData.HeightValues[Idx];
        const float NeighborAvg =
            (OutData.HeightValues[Idx - 1] + OutData.HeightValues[Idx + 1] +
             OutData.HeightValues[Idx - SmoothRes] +
             OutData.HeightValues[Idx + SmoothRes]) *
            0.25f;
        Smoothed[Idx] = FMath::Lerp(Center, NeighborAvg, SmoothStrength);
      }
    }
    FMemory::Memcpy(OutData.HeightValues.GetData(), Smoothed.GetData(),
                    Smoothed.Num() * sizeof(float));
  }

  // === Post-Generation: Biome Override ===
  constexpr float SnowThreshold = 0.714f;
  constexpr float RockSlopeThreshold = 0.0002f;

  for (int32 Y = 1; Y < SmoothRes - 1; ++Y) {
    for (int32 X = 1; X < SmoothRes - 1; ++X) {
      const int32 Idx = Y * SmoothRes + X;
      const float H = OutData.HeightValues[Idx];

      float MaxDiff = 0.0f;
      MaxDiff =
          FMath::Max(MaxDiff, FMath::Abs(H - OutData.HeightValues[Idx - 1]));
      MaxDiff =
          FMath::Max(MaxDiff, FMath::Abs(H - OutData.HeightValues[Idx + 1]));
      MaxDiff = FMath::Max(
          MaxDiff, FMath::Abs(H - OutData.HeightValues[Idx - SmoothRes]));
      MaxDiff = FMath::Max(
          MaxDiff, FMath::Abs(H - OutData.HeightValues[Idx + SmoothRes]));

      EFPMBiome &CurrentBiome = OutData.BiomeValues[Idx];

      if (H > SnowThreshold) {
        CurrentBiome = EFPMBiome::Snow;
      }

      if (MaxDiff > RockSlopeThreshold) {
        CurrentBiome = EFPMBiome::Mountain;
      }
    }
  }

  // === Post-Generation: Slope Limiter ===
  constexpr int32 SlopePasses = 4;
  constexpr float MaxSlope = 0.02f;

  for (int32 Pass = 0; Pass < SlopePasses; ++Pass) {
    for (int32 Y = 0; Y < SmoothRes; ++Y) {
      for (int32 X = 0; X < SmoothRes; ++X) {
        const int32 Idx = Y * SmoothRes + X;
        float H = OutData.HeightValues[Idx];

        auto ClampToNeighbor = [&](int32 NIdx) {
          const float NH = OutData.HeightValues[NIdx];
          if (H - NH > MaxSlope) {
            H = NH + MaxSlope;
          } else if (NH - H > MaxSlope) {
            H = NH - MaxSlope;
          }
        };

        if (X > 0)
          ClampToNeighbor(Idx - 1);
        if (X < SmoothRes - 1)
          ClampToNeighbor(Idx + 1);
        if (Y > 0)
          ClampToNeighbor(Idx - SmoothRes);
        if (Y < SmoothRes - 1)
          ClampToNeighbor(Idx + SmoothRes);

        OutData.HeightValues[Idx] = H;
      }
    }
  }

  OutData.bIsValid = true;

  UE_LOG(LogTemp, Verbose, TEXT("FPM: Generated hex chunk %s"),
         *Coord.ToString());
}
