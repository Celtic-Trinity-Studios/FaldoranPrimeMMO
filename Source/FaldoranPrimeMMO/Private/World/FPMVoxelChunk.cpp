// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMVoxelChunk.h"
#include "MCTables.h"
#include "Misc/ConfigCacheIni.h"
#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"

using namespace FPMVoxelConstants;

// ===================================================================
//  INI-loaded terrain settings (cached on first use)
// ===================================================================

namespace {
struct FTerrainSettings {
  // [Terrain]
  // Higher scale for more rugged mountains across 1.28km chunks
  float NoiseScale = 6.0f;

  // Sea level at Z=0.
  // Normalized 0.0 = -11km (MinWorldZ)
  // Normalized 1.0 = +9km (MaxWorldZ)
  // Sea level (0m) = 0.55 normalized elevation.
  float HeightBase = FPMChunkConstants::MinWorldZ;
  float HeightScale = FPMChunkConstants::WorldHeightRange;
  float MaxHeight = 1.0f; // Allow the full 9km height

  float MeadowFlattenTarget = 0.57f; // Just above sea level
  float MeadowFlattenStrength = 0.40f;
  float MountainPeakIntensity = 0.15f;

  // [Ocean]
  // Lowest point at -11km ( normalized 0.0 )
  float OceanFloorDepth = 0.0f;

  // [Rivers]
  bool bEnableRivers = true;
  float RiverBedHeight = 0.55f;  // Sea level
  float RiverCarveDepth = 0.02f; // ~400m carving depth
  float RiverCarveStrength = 0.85f;
  float RiverMaskThreshold = 0.15f;

  // [Lakes]
  bool bEnableLakes = true;
  float LakeBedHeight = 0.555f; // Slightly above sea level
  float LakeThreshold = 0.58f;

  bool bLoaded = false;
};

static FTerrainSettings GTerrainSettings;

void LoadTerrainSettings() {
  static bool bSettingsLoaded = false;
  if (bSettingsLoaded)
    return;
  bSettingsLoaded = true;

  FString IniPath = FPaths::ConvertRelativePathToFull(
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
  GConfig->GetFloat(TEXT("Rivers"), TEXT("RiverCarveDepth"),
                    GTerrainSettings.RiverCarveDepth, IniPath);
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

  // [Ocean]
  GConfig->GetFloat(TEXT("Ocean"), TEXT("OceanFloorDepth"),
                    GTerrainSettings.OceanFloorDepth, IniPath);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM Voxel: Terrain settings loaded -- Rivers=%d, Lakes=%d, "
              "NoiseScale=%.1f, HeightScale=%.0f, OceanFloor=%.2f, "
              "RiverCarveDepth=%.4f, RiverCarveStr=%.2f"),
         GTerrainSettings.bEnableRivers, GTerrainSettings.bEnableLakes,
         GTerrainSettings.NoiseScale, GTerrainSettings.HeightScale,
         GTerrainSettings.OceanFloorDepth, GTerrainSettings.RiverCarveDepth,
         GTerrainSettings.RiverCarveStrength);
}

/** Hermite smooth-step: S-curve from 0->1 between Edge0 and Edge1 */
float SmoothStep(float Edge0, float Edge1, float X) {
  const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
  return T * T * (3.0f - 2.0f * T);
}

} // anonymous namespace

// ===================================================================
//  Terrain Surface Z � noise + rivers + lakes
// ===================================================================

// ===================================================================
//  Terrain Surface Z - unified FPMNoise pipeline
// ===================================================================

float FPMVoxelGenerator::TerrainSurfaceZ(float WorldX, float WorldY,
                                         int32 WorldSeed) {
  LoadTerrainSettings();
  const FTerrainSettings &S = GTerrainSettings;

  float LandHeight = FPMNoise::TerrainHeight(WorldX, WorldY, WorldSeed);

  // Precompute normalized coords and island mask (shared by rivers + lakes)
  const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const float ISZ = FPMChunkConstants::StarterIslandWorldSize;
  const float NormX = (WorldX + HI) / ISZ;
  const float NormY = (WorldY + HI) / ISZ;
  const float Mask = FPMNoise::IslandMask(WorldX, WorldY, WorldSeed);

  // --- River carving ---
  if (S.bEnableRivers) {
    const float River = FPMChunkGenerator::RiverFactor(NormX, NormY, WorldSeed);
    if (River > 0.01f && Mask > S.RiverMaskThreshold) {
      const float CarveAmount =
          S.RiverCarveDepth * River * S.RiverCarveStrength;
      LandHeight -= CarveAmount;
    }
  }

  // --- Lake carving ---
  // Lakes form in low-lying, high-moisture areas.
  if (S.bEnableLakes && Mask > 0.1f) {
    const float Moist = FPMNoise::Moisture(WorldX, WorldY, WorldSeed);
    // Sea level is at normalized 0.55 (maps to Z=0)
    const float SeaLevelNorm = 0.55f;

    // Lake activation: high moisture + terrain near or below sea level
    if (Moist > S.LakeThreshold && LandHeight < SeaLevelNorm + 0.05f) {
      // Smooth blend into lake bed based on moisture intensity
      const float LakeFactor =
          SmoothStep(S.LakeThreshold, S.LakeThreshold + 0.12f, Moist);
      LandHeight = FMath::Lerp(LandHeight, S.LakeBedHeight, LakeFactor * 0.85f);
    }
  }

  // Allow terrain below sea level (ocean floor).
  LandHeight = FMath::Clamp(LandHeight, S.OceanFloorDepth, S.MaxHeight);
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
  const float Mask = FPMNoise::IslandMask(WorldX, WorldY, WorldSeed);

  EFPMBiome Biome =
      FPMChunkGenerator::AssignBiomeFromNoise(NormX, NormY, WorldSeed, Mask);

  if (GTerrainSettings.bEnableRivers) {
    const float River = FPMChunkGenerator::RiverFactor(NormX, NormY, WorldSeed);
    if (River > 0.3f && Mask > GTerrainSettings.RiverMaskThreshold) {
      Biome = EFPMBiome::River;
    }
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
    return FColor(60, 90, 80, 0); // Dark brown-green riverbed/banks
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
//  Smooth Normals � average normals across shared vertex positions
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
//  Vertex color smoothing � averages colors at shared positions for
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

  // --- 1. Build density grid + climate grids ---
  TArray<float> Density;
  Density.SetNumUninitialized(TotalCorners);

  // Climate grids: Temperature and Moisture sampled at each XY column
  // These will be diffusion-smoothed before biome assignment.
  TArray<float> TempGrid, MoistGrid, HeightGrid, MaskGrid;
  const int32 ClimateSize = GridX * GridY;
  TempGrid.SetNumUninitialized(ClimateSize);
  MoistGrid.SetNumUninitialized(ClimateSize);
  HeightGrid.SetNumUninitialized(ClimateSize);
  MaskGrid.SetNumUninitialized(ClimateSize);

  // Sample terrain height, density, AND climate fields per column
  for (int32 Y = 0; Y < GridY; ++Y) {
    const float WorldY = ChunkOrigin.Y - OverlapOff + Y * VS;
    for (int32 X = 0; X < GridX; ++X) {
      const float WorldX = ChunkOrigin.X - OverlapOff + X * VS;
      const float SurfaceZ = TerrainSurfaceZ(WorldX, WorldY, WorldSeed);

      // Climate fields (will be smoothed in step 1b)
      const int32 CIdx = Y * GridX + X;
      TempGrid[CIdx] = FPMNoise::Temperature(WorldX, WorldY, WorldSeed);
      MoistGrid[CIdx] = FPMNoise::Moisture(WorldX, WorldY, WorldSeed);
      HeightGrid[CIdx] = FPMNoise::TerrainHeight(WorldX, WorldY, WorldSeed);
      MaskGrid[CIdx] = FPMNoise::IslandMask(WorldX, WorldY, WorldSeed);

      // Fill density column
      for (int32 Z = 0; Z < GridZ; ++Z) {
        const float WorldZ = WorldZBase + Z * VS;
        const int32 Idx = Z * GridX * GridY + Y * GridX + X;

        float D = SurfaceZ - WorldZ;
        if (Z >= GridZ - 2) {
          D = -1.0f;
        }
        Density[Idx] = D;
      }
    }
  }

  // --- 1b. Diffusion-smooth climate grids ---
  // 3 passes at strength 0.4: enough to prevent speckling, but preserves
  // the medium-frequency climate variation for within-region variety.
  FPMNoise::DiffusionSmooth(TempGrid, GridX, GridY, 3, 0.4f);
  FPMNoise::DiffusionSmooth(MoistGrid, GridX, GridY, 3, 0.4f);

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

          CornerPos[C] = FVector(CX * VS - OverlapOff, CY * VS - OverlapOff,
                                 WorldZBase + CZ * VS);
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

          // UVs + vertex colors: computed per-vertex for smooth
          // interpolation (avoids per-triangle centroid striations).
          // Bilinear-interpolate the diffusion-smoothed climate grids
          // at each vertex position individually.
          auto BilerpGrid = [&](const TArray<float>& Grid, float LX, float LY) -> float {
            const float GFX = (LX + OverlapOff) / VS;
            const float GFY = (LY + OverlapOff) / VS;
            const int32 GX0 = FMath::Clamp(FMath::FloorToInt(GFX), 0, GridX - 2);
            const int32 GY0 = FMath::Clamp(FMath::FloorToInt(GFY), 0, GridY - 2);
            const float FX = FMath::Clamp(GFX - GX0, 0.0f, 1.0f);
            const float FY = FMath::Clamp(GFY - GY0, 0.0f, 1.0f);
            const float V00 = Grid[GY0 * GridX + GX0];
            const float V10 = Grid[GY0 * GridX + GX0 + 1];
            const float V01 = Grid[(GY0 + 1) * GridX + GX0];
            const float V11 = Grid[(GY0 + 1) * GridX + GX0 + 1];
            return FMath::Lerp(
                FMath::Lerp(V00, V10, FX),
                FMath::Lerp(V01, V11, FX),
                FY);
          };

          for (int32 VI = 0; VI < 3; ++VI) {
            const FVector &VP = OutMesh.Vertices[BaseIdx + VI];
            OutMesh.UVs.Emplace((ChunkOrigin.X + VP.X) / 100.0f,
                                (ChunkOrigin.Y + VP.Y) / 100.0f);

            const float VTemp = BilerpGrid(TempGrid, VP.X, VP.Y);
            const float VMoist = BilerpGrid(MoistGrid, VP.X, VP.Y);
            const float VHeight = BilerpGrid(HeightGrid, VP.X, VP.Y);
            const float VMask = BilerpGrid(MaskGrid, VP.X, VP.Y);

            FColor VColor;
            if (GTerrainSettings.bEnableRivers) {
              const float WorldVX = ChunkOrigin.X + VP.X;
              const float WorldVY = ChunkOrigin.Y + VP.Y;
              const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
              const float ISZ = FPMChunkConstants::StarterIslandWorldSize;
              const float RNormX = (WorldVX + HI) / ISZ;
              const float RNormY = (WorldVY + HI) / ISZ;
              const float River = FPMChunkGenerator::RiverFactor(RNormX, RNormY, WorldSeed);
              if (River > 0.3f && VMask > GTerrainSettings.RiverMaskThreshold) {
                VColor = FPMVoxelGenerator::BiomeToVertexColor(EFPMBiome::River);
              } else {
                VColor = FPMChunkGenerator::BlendedBiomeColor(
                    VTemp, VMoist, VHeight, VMask);
              }
            } else {
              VColor = FPMChunkGenerator::BlendedBiomeColor(
                  VTemp, VMoist, VHeight, VMask);
            }
            OutMesh.Colors.Add(VColor);
          }
        }
      }
    }
  }

  // --- 3. Smooth normals (average across shared vertices) ---
  SmoothNormals(OutMesh);

  // --- 4. Vertex colors are now computed per-vertex (no post-smooth needed) ---

  UE_LOG(LogTemp, Verbose, TEXT("FPM Voxel: Chunk %s -> %d verts, %d tris"),
         *Coord.ToString(), OutMesh.Vertices.Num(),
         OutMesh.Triangles.Num() / 3);
}
