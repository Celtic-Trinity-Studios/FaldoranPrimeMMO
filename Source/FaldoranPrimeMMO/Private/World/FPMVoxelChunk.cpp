// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMVoxelChunk.h"
#include "MCTables.h"
#include "Misc/ConfigCacheIni.h"

using namespace FPMVoxelConstants;

// ===================================================================
//  INI-loaded terrain settings (cached on first use)
// ===================================================================

namespace {
struct FTerrainSettings {
  // [Terrain]
  float NoiseScale = 6.0f;
  float HeightBase = -400.0f;
  float HeightScale = 5000.0f;
  float MaxHeight = 0.55f;
  float MeadowFlattenTarget = 0.06f;
  float MeadowFlattenStrength = 0.80f;
  float MountainPeakIntensity = 0.12f;

  // [Rivers]
  bool bEnableRivers = true;
  float RiverBedHeight = 0.04f;
  float RiverCarveStrength = 0.85f;
  float RiverMaskThreshold = 0.15f;

  // [Lakes]
  bool bEnableLakes = true;
  float LakeBedHeight = 0.03f;
  float LakeThreshold = 0.38f;

  bool bLoaded = false;
};

static FTerrainSettings GTerrainSettings;

void LoadTerrainSettings() {
  if (GTerrainSettings.bLoaded)
    return;
  GTerrainSettings.bLoaded = true;

  FString IniPath =
      FPaths::ConvertRelativePathToFull(
          FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("WorldGen.ini")));
  FConfigCacheIni::NormalizeConfigIniPath(IniPath);
  if (!FPaths::FileExists(IniPath))
    return;

  // [Terrain]
  GConfig->GetFloat(TEXT("Terrain"), TEXT("NoiseScale"),
                    GTerrainSettings.NoiseScale, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("HeightBase"),
                    GTerrainSettings.HeightBase, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("HeightScale"),
                    GTerrainSettings.HeightScale, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("MaxHeight"),
                    GTerrainSettings.MaxHeight, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("MeadowFlattenTarget"),
                    GTerrainSettings.MeadowFlattenTarget, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("MeadowFlattenStrength"),
                    GTerrainSettings.MeadowFlattenStrength, IniPath);
  GConfig->GetFloat(TEXT("Terrain"), TEXT("MountainPeakIntensity"),
                    GTerrainSettings.MountainPeakIntensity, IniPath);

  // [Rivers]
  GConfig->GetBool(TEXT("Rivers"), TEXT("bEnableRivers"),
                   GTerrainSettings.bEnableRivers, IniPath);
  GConfig->GetFloat(TEXT("Rivers"), TEXT("RiverBedHeight"),
                    GTerrainSettings.RiverBedHeight, IniPath);
  GConfig->GetFloat(TEXT("Rivers"), TEXT("RiverCarveStrength"),
                    GTerrainSettings.RiverCarveStrength, IniPath);
  GConfig->GetFloat(TEXT("Rivers"), TEXT("RiverMaskThreshold"),
                    GTerrainSettings.RiverMaskThreshold, IniPath);

  // [Lakes]
  GConfig->GetBool(TEXT("Lakes"), TEXT("bEnableLakes"),
                   GTerrainSettings.bEnableLakes, IniPath);
  GConfig->GetFloat(TEXT("Lakes"), TEXT("LakeBedHeight"),
                    GTerrainSettings.LakeBedHeight, IniPath);
  GConfig->GetFloat(TEXT("Lakes"), TEXT("LakeThreshold"),
                    GTerrainSettings.LakeThreshold, IniPath);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM Voxel: Terrain settings loaded — Rivers=%d, Lakes=%d, "
              "NoiseScale=%.1f, HeightScale=%.0f"),
         GTerrainSettings.bEnableRivers, GTerrainSettings.bEnableLakes,
         GTerrainSettings.NoiseScale, GTerrainSettings.HeightScale);
}

/** Hermite smooth-step: S-curve from 0->1 between Edge0 and Edge1 */
float SmoothStep(float Edge0, float Edge1, float X) {
  const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
  return T * T * (3.0f - 2.0f * T);
}

} // anonymous namespace

// ===================================================================
//  Terrain Surface Z — noise + rivers + lakes
// ===================================================================

float FPMVoxelGenerator::TerrainSurfaceZ(float WorldX, float WorldY,
                                         int32 WorldSeed) {
  LoadTerrainSettings();
  const FTerrainSettings &S = GTerrainSettings;

  // Convert world XY to normalized island-space (0-1)
  const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float ISZ = FPMChunkConstants::StarterIslandWorldSize;
  const float NormX = (WorldX + HI) / ISZ;
  const float NormY = (WorldY + HI) / ISZ;

  // Island mask
  const float Mask = FPMChunkGenerator::IslandMask(NormX, NormY);

  // Biome (needed for vertex colors and flattening)
  EFPMBiome Biome =
      FPMChunkGenerator::AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

  // Compute continuous BiomeValue with domain warping (matches
  // AssignBiomeFromNoise)
  constexpr float BiomeScale1 = 3.5f;
  constexpr float BiomeScale2 = 5.0f;
  constexpr float BiomeScale3 = 12.0f;
  constexpr int32 BiomeSeedOffset = 99999;
  constexpr float WarpScale = 4.0f;
  constexpr float WarpStrength = 0.08f;
  const float WarpX =
      FPMChunkGenerator::FractalNoise(NormX * WarpScale, NormY * WarpScale,
                                      WorldSeed + 55555, 3) *
      WarpStrength;
  const float WarpY = FPMChunkGenerator::FractalNoise(
                          NormX * WarpScale + 100.0f,
                          NormY * WarpScale + 100.0f, WorldSeed + 66666, 3) *
                      WarpStrength;
  const float WarpedX = NormX + WarpX;
  const float WarpedY = NormY + WarpY;
  const float BN1 = FPMChunkGenerator::FractalNoise(
      WarpedX * BiomeScale1, WarpedY * BiomeScale1, WorldSeed + BiomeSeedOffset,
      3);
  const float BN2 = FPMChunkGenerator::FractalNoise(
      WarpedX * BiomeScale2, WarpedY * BiomeScale2,
      WorldSeed + BiomeSeedOffset + 7777, 2);
  const float BN3 = FPMChunkGenerator::FractalNoise(
      WarpedX * BiomeScale3, WarpedY * BiomeScale3,
      WorldSeed + BiomeSeedOffset + 33333, 2);
  const float BiomeValue = BN1 * 0.50f + BN2 * 0.30f + BN3 * 0.20f;

  // Smooth noise blend: fractal (plains) ? ridge (mountains)
  const float RidgeBlend =
      FMath::Clamp((BiomeValue - 0.30f) / 0.40f, 0.0f, 1.0f);
  const float FlatNoise = FPMChunkGenerator::FractalNoise(
      NormX * S.NoiseScale, NormY * S.NoiseScale, WorldSeed, 5);
  const float MtNoise = FPMChunkGenerator::RidgeNoise(
      NormX * S.NoiseScale, NormY * S.NoiseScale, WorldSeed, 6);
  const float Noise = FMath::Lerp(FlatNoise, MtNoise, RidgeBlend);

  // Continuous elevation bias (smooth curve, no jumps)
  const float BiomeBias =
      FPMChunkGenerator::ContinuousElevationBias(BiomeValue);
  float LandHeight = (Noise * 0.5f + BiomeBias * 0.5f) * Mask;

  // Meadows flattening
  if (Biome == EFPMBiome::Meadows && LandHeight > 0.02f) {
    LandHeight =
        FMath::Lerp(LandHeight, S.MeadowFlattenTarget, S.MeadowFlattenStrength);
  }

  // Mountain peaks
  if (Biome == EFPMBiome::Mountain) {
    const float PeakNoise = FPMChunkGenerator::FractalNoise(
        NormX * S.NoiseScale * 2.0f, NormY * S.NoiseScale * 2.0f,
        WorldSeed + 12345, 4);
    LandHeight += PeakNoise * S.MountainPeakIntensity * Mask;
  }

  // --- Rivers (smooth banks) ---
  if (S.bEnableRivers) {
    const float River = FPMChunkGenerator::RiverFactor(NormX, NormY, WorldSeed);
    if (River > 0.005f && Mask > S.RiverMaskThreshold) {
      // Smooth S-curve: gentle slope from land into river bed
      // SmoothStep creates a gradual bank instead of a cliff
      const float BankFactor = SmoothStep(0.005f, 0.10f, River);
      LandHeight = FMath::Lerp(LandHeight, S.RiverBedHeight,
                               BankFactor * S.RiverCarveStrength);
    }
  }

  // --- Lakes ---
  if (S.bEnableLakes) {
    const float LakeNoise = FPMChunkGenerator::FractalNoise(
        NormX * 4.0f, NormY * 4.0f, WorldSeed + 77777, 3);
    if (LakeNoise < S.LakeThreshold && Mask > 0.25f &&
        Biome != EFPMBiome::Mountain && Biome != EFPMBiome::Snow) {
      // Smooth lake edge transition
      const float LakeEdge =
          SmoothStep(S.LakeThreshold, S.LakeThreshold * 0.5f, LakeNoise);
      LandHeight = FMath::Lerp(LandHeight, S.LakeBedHeight,
                               FMath::Clamp(LakeEdge, 0.0f, 0.9f));
    }
  }

  LandHeight = FMath::Clamp(LandHeight, 0.0f, S.MaxHeight);

  // Convert normalized height to world Z
  return S.HeightBase + LandHeight * S.HeightScale;
}

// ===================================================================
//  Biome at world XY
// ===================================================================

EFPMBiome FPMVoxelGenerator::BiomeAtWorldXY(float WorldX, float WorldY,
                                            int32 WorldSeed,
                                            float NormalizedHeight) {
  const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float ISZ = FPMChunkConstants::StarterIslandWorldSize;
  const float NormX = (WorldX + HI) / ISZ;
  const float NormY = (WorldY + HI) / ISZ;
  const float Mask = FPMChunkGenerator::IslandMask(NormX, NormY);

  EFPMBiome Biome =
      FPMChunkGenerator::AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

  // River override: paint as Coast biome
  if (GTerrainSettings.bEnableRivers) {
    const float River = FPMChunkGenerator::RiverFactor(NormX, NormY, WorldSeed);
    if (River > 0.02f && Mask > GTerrainSettings.RiverMaskThreshold) {
      Biome = EFPMBiome::Coast;
    }
  }

  // Lake override: paint as Coast biome
  if (GTerrainSettings.bEnableLakes) {
    const float LakeNoise = FPMChunkGenerator::FractalNoise(
        NormX * 4.0f, NormY * 4.0f, WorldSeed + 77777, 3);
    if (LakeNoise < GTerrainSettings.LakeThreshold && Mask > 0.25f &&
        Biome != EFPMBiome::Mountain && Biome != EFPMBiome::Snow) {
      Biome = EFPMBiome::Coast;
    }
  }

  // Elevation overrides
  if (Biome == EFPMBiome::Mountain && NormalizedHeight > 0.40f) {
    Biome = EFPMBiome::Snow;
  } else if (NormalizedHeight > 0.0f && NormalizedHeight < 0.05f &&
             Biome != EFPMBiome::Coast && Biome != EFPMBiome::Ocean) {
    Biome = EFPMBiome::Swamp;
  }
  return Biome;
}

// ===================================================================
//  Helpers
// ===================================================================

FVector FPMVoxelGenerator::InterpolateEdge(const FVector &P1, const FVector &P2,
                                           float D1, float D2) {
  if (FMath::Abs(D1) < 0.00001f)
    return P1;
  if (FMath::Abs(D2) < 0.00001f)
    return P2;
  if (FMath::Abs(D1 - D2) < 0.00001f)
    return P1;
  const float T = D1 / (D1 - D2);
  return P1 + T * (P2 - P1);
}

FColor FPMVoxelGenerator::BiomeToVertexColor(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Meadows:
    return FColor(255, 0, 0, 0);
  case EFPMBiome::Forest:
    return FColor(0, 255, 0, 0);
  case EFPMBiome::Mountain:
    return FColor(0, 0, 255, 0);
  case EFPMBiome::Coast:
    return FColor(51, 0, 0, 0);
  case EFPMBiome::Swamp:
    return FColor(128, 0, 128, 0);
  case EFPMBiome::Snow:
    return FColor(0, 0, 0, 255);
  case EFPMBiome::Ocean:
    return FColor(0, 0, 0, 0);
  default:
    return FColor(128, 128, 128, 0);
  }
}

// ===================================================================
//  Smooth Normals — average normals across shared vertex positions
// ===================================================================

static void SmoothNormals(FFPMVoxelMeshData &Mesh) {
  if (Mesh.Vertices.Num() == 0)
    return;

  // Quantize vertex positions to 0.1mm precision for matching
  // (avoids floating point comparison issues)
  auto Quantize = [](const FVector &V) -> FIntVector {
    return FIntVector(FMath::RoundToInt(V.X * 10.0f),
                      FMath::RoundToInt(V.Y * 10.0f),
                      FMath::RoundToInt(V.Z * 10.0f));
  };

  // First pass: accumulate normals at each unique position
  TMap<FIntVector, FVector> NormalAccum;
  NormalAccum.Reserve(Mesh.Vertices.Num() / 3); // rough estimate

  for (int32 I = 0; I < Mesh.Vertices.Num(); ++I) {
    const FIntVector Key = Quantize(Mesh.Vertices[I]);
    if (FVector *Existing = NormalAccum.Find(Key)) {
      *Existing += Mesh.Normals[I];
    } else {
      NormalAccum.Add(Key, Mesh.Normals[I]);
    }
  }

  // Second pass: replace each normal with the averaged version
  for (int32 I = 0; I < Mesh.Vertices.Num(); ++I) {
    const FIntVector Key = Quantize(Mesh.Vertices[I]);
    const FVector AvgNormal = NormalAccum[Key].GetSafeNormal();
    Mesh.Normals[I] = AvgNormal;
  }
}

// ===================================================================
//  Vertex color smoothing — averages colors at shared positions for
//  smooth biome transitions.  Uses the same position-quantization
//  approach as SmoothNormals.
// ===================================================================

static void SmoothVertexColors(FFPMVoxelMeshData &Mesh) {
  if (Mesh.Vertices.Num() == 0 || Mesh.Colors.Num() != Mesh.Vertices.Num())
    return;

  // Quantize to 0.1 mm precision for matching shared verts
  auto Quantize = [](const FVector &V) -> FIntVector {
    return FIntVector(FMath::RoundToInt(V.X * 10.0f),
                      FMath::RoundToInt(V.Y * 10.0f),
                      FMath::RoundToInt(V.Z * 10.0f));
  };

  // Accumulate per unique position: sum of RGBA and count
  struct FColorAccum {
    int32 R = 0;
    int32 G = 0;
    int32 B = 0;
    int32 A = 0;
    int32 Count = 0;
  };

  TMap<FIntVector, FColorAccum> Accum;
  Accum.Reserve(Mesh.Vertices.Num() / 3);

  // Pass 1: accumulate
  for (int32 I = 0; I < Mesh.Vertices.Num(); ++I) {
    const FIntVector Key = Quantize(Mesh.Vertices[I]);
    FColorAccum &A = Accum.FindOrAdd(Key);
    A.R += Mesh.Colors[I].R;
    A.G += Mesh.Colors[I].G;
    A.B += Mesh.Colors[I].B;
    A.A += Mesh.Colors[I].A;
    A.Count++;
  }

  // Pass 2: write averaged colors back
  for (int32 I = 0; I < Mesh.Vertices.Num(); ++I) {
    const FIntVector Key = Quantize(Mesh.Vertices[I]);
    const FColorAccum &A = Accum[Key];
    Mesh.Colors[I] = FColor(
        static_cast<uint8>(A.R / A.Count), static_cast<uint8>(A.G / A.Count),
        static_cast<uint8>(A.B / A.Count), static_cast<uint8>(A.A / A.Count));
  }
}

// ===================================================================
//  Main: Generate density field + Marching Cubes mesh extraction
// ===================================================================

void FPMVoxelGenerator::GenerateAndMesh(const FFPMChunkCoord &Coord,
                                        int32 WorldSeed,
                                        FFPMVoxelMeshData &OutMesh) {
  OutMesh.Reset();
  LoadTerrainSettings();

  // Chunk world origin (bottom-left corner)
  const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
  const float VS = VoxelSizeCm;
  // Overlap margin: grid starts OverlapMargin voxels BEFORE the chunk origin
  const float OverlapOff = OverlapOffsetCm;

  // --- 1. Build density grid ---
  TArray<float> Density;
  Density.SetNumUninitialized(TotalCorners);

  for (int32 Z = 0; Z < GridZ; ++Z) {
    const float WorldZ = WorldZBase + Z * VS;
    for (int32 Y = 0; Y < GridY; ++Y) {
      const float WorldY = ChunkOrigin.Y - OverlapOff + Y * VS;
      for (int32 X = 0; X < GridX; ++X) {
        const float WorldX = ChunkOrigin.X - OverlapOff + X * VS;
        const int32 Idx = Z * GridX * GridY + Y * GridX + X;

        // Density > 0 = solid (below surface), < 0 = air (above surface)
        const float SurfaceZ = TerrainSurfaceZ(WorldX, WorldY, WorldSeed);
        float D = SurfaceZ - WorldZ;

        // CRITICAL: Force the top 2 layers to be air (density < 0).
        // This prevents Marching Cubes from generating chaotic cap
        // geometry when the terrain approaches the volume ceiling.
        if (Z >= GridZ - 2) {
          D = -1.0f;
        }

        Density[Idx] = D;
      }
    }
  }

  // --- 2. Marching Cubes ---
  for (int32 Z = 0; Z < ChunkVoxelsZ; ++Z) {
    for (int32 Y = 0; Y < ChunkVoxelsXY_Total; ++Y) {
      for (int32 X = 0; X < ChunkVoxelsXY_Total; ++X) {
        // 8 corner densities and positions
        float CornerDensity[8];
        FVector CornerPos[8];

        for (int32 C = 0; C < 8; ++C) {
          const int32 CX = X + MCCornerOffset[C][0];
          const int32 CY = Y + MCCornerOffset[C][1];
          const int32 CZ = Z + MCCornerOffset[C][2];
          const int32 Idx = CZ * GridX * GridY + CY * GridX + CX;
          CornerDensity[C] = Density[Idx];

          CornerPos[C] = FVector(CX * VS - OverlapOff, CY * VS - OverlapOff, WorldZBase + CZ * VS);
        }

        // Determine cube configuration index
        int32 CubeIndex = 0;
        for (int32 C = 0; C < 8; ++C) {
          if (CornerDensity[C] > 0.0f) {
            CubeIndex |= (1 << C);
          }
        }

        if (MCEdgeTable[CubeIndex] == 0)
          continue;

        // Interpolate vertices along intersected edges
        FVector EdgeVerts[12];
        for (int32 E = 0; E < 12; ++E) {
          if (MCEdgeTable[CubeIndex] & (1 << E)) {
            const int32 C0 = MCEdgeCorners[E][0];
            const int32 C1 = MCEdgeCorners[E][1];
            EdgeVerts[E] =
                InterpolateEdge(CornerPos[C0], CornerPos[C1], CornerDensity[C0],
                                CornerDensity[C1]);
          }
        }

        // Emit triangles
        for (int32 T = 0; MCTriTable[CubeIndex][T] != -1; T += 3) {
          const FVector &V0 = EdgeVerts[MCTriTable[CubeIndex][T]];
          const FVector &V1 = EdgeVerts[MCTriTable[CubeIndex][T + 1]];
          const FVector &V2 = EdgeVerts[MCTriTable[CubeIndex][T + 2]];

          // Face normal (initial — will be smoothed in post-pass)
          const FVector Edge1 = V1 - V0;
          const FVector Edge2 = V2 - V0;
          FVector FaceNormal =
              -FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

          const int32 BaseIdx = OutMesh.Vertices.Num();

          OutMesh.Vertices.Add(V0);
          OutMesh.Vertices.Add(V1);
          OutMesh.Vertices.Add(V2);

          OutMesh.Normals.Add(FaceNormal);
          OutMesh.Normals.Add(FaceNormal);
          OutMesh.Normals.Add(FaceNormal);

          OutMesh.Triangles.Add(BaseIdx);
          OutMesh.Triangles.Add(BaseIdx + 1);
          OutMesh.Triangles.Add(BaseIdx + 2);

          // UVs: world-space XY projected
          for (int32 VI = 0; VI < 3; ++VI) {
            const FVector &VP = OutMesh.Vertices[BaseIdx + VI];
            OutMesh.UVs.Emplace((ChunkOrigin.X + VP.X) / 100.0f,
                                (ChunkOrigin.Y + VP.Y) / 100.0f);
          }

          // Vertex color: biome at triangle center
          const FVector Center = (V0 + V1 + V2) / 3.0f;
          const float CenterWorldX = ChunkOrigin.X + Center.X;
          const float CenterWorldY = ChunkOrigin.Y + Center.Y;
          const float NormH = (Center.Z - GTerrainSettings.HeightBase) /
                              GTerrainSettings.HeightScale;
          const EFPMBiome Biome =
              BiomeAtWorldXY(CenterWorldX, CenterWorldY, WorldSeed, NormH);
          const FColor BColor = BiomeToVertexColor(Biome);
          OutMesh.Colors.Add(BColor);
          OutMesh.Colors.Add(BColor);
          OutMesh.Colors.Add(BColor);
        }
      }
    }
  }

  // --- 3. Smooth normals (average across shared vertices) ---
  SmoothNormals(OutMesh);

  // --- 4. Smooth vertex colors (biome blending at boundaries) ---
  SmoothVertexColors(OutMesh);

  UE_LOG(LogTemp, Verbose, TEXT("FPM Voxel: Chunk %s -> %d verts, %d tris"),
         *Coord.ToString(), OutMesh.Vertices.Num(),
         OutMesh.Triangles.Num() / 3);
}
