// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMVoxelChunk.h"
#include "MCTables.h"
#include "Misc/ConfigCacheIni.h"
#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"
#include "World/FPMWorldChunkManager.h"

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
//  Terrain Surface Z ? noise + rivers + lakes
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
//  Smooth Normals ? average normals across shared vertex positions
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
//  Vertex color smoothing ? averages colors at shared positions for
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

  // Cave shell parameters — only evaluate 3D cave noise within this range
  // below the terrain surface. Loaded from INI via FPMNoise::CaveDensity.
  constexpr float CaveShellDepth = 500000.0f; // 5km below surface
  constexpr float CaveCarveThreshold = 0.15f; // How strong cave noise must be
                                              // to fully carve (lower = more
                                              // caves visible)

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

      // Precompute cave shell Z limits for this column
      const float CaveTopZ = SurfaceZ; // Caves can't be above surface
      const float CaveBotZ = SurfaceZ - CaveShellDepth;

      // Fill density column
      for (int32 Z = 0; Z < GridZ; ++Z) {
        const float WorldZ = WorldZBase + Z * VS;
        const int32 Idx = Z * GridX * GridY + Y * GridX + X;

        float D = SurfaceZ - WorldZ;

        // Cap top of volume to air
        if (Z >= GridZ - 2) {
          D = -1.0f;
        }
        // --- Cave carving (only within cave shell) ---
        else if (D > 0.0f && WorldZ >= CaveBotZ && WorldZ <= CaveTopZ) {
          // We're below the surface and within cave-possible range.
          // Sample 3D cave noise to potentially carve out this voxel.
          const float Cave = FPMNoise::CaveDensity(WorldX, WorldY, WorldZ,
                                                   WorldSeed, SurfaceZ);

          if (Cave > CaveCarveThreshold) {
            // Smoothly transition from solid to air based on cave intensity.
            // At CaveCarveThreshold: D stays positive (solid)
            // At 1.0: D becomes strongly negative (open air)
            const float CarveFactor =
                (Cave - CaveCarveThreshold) / (1.0f - CaveCarveThreshold);
            // Push density negative proportional to carve strength.
            // Use a smooth curve so cave walls have nice MC interpolation.
            D = FMath::Lerp(D, -VS * 0.5f, CarveFactor * CarveFactor);
          }
        }

        // --- Apply terraforming voxel overlay ---
        // Look up any player modifications (dig/fill) stored in the
        // WorldChunkManager's VoxelOverlays map.
        {
          // Use the global active manager (same GActiveChunkManager used
          // by console commands). This is safe because GenerateAndMesh
          // runs on worker threads and VoxelOverlays is only written to
          // from the game thread between regeneration calls.
          extern AFPMWorldChunkManager *GActiveChunkManager;
          if (GActiveChunkManager &&
              GActiveChunkManager->HasVoxelDeltas(Coord)) {
            const FIntVector VoxelKey(FMath::FloorToInt(WorldX / VS),
                                      FMath::FloorToInt(WorldY / VS),
                                      FMath::FloorToInt(WorldZ / VS));
            const float Delta =
                GActiveChunkManager->GetVoxelDelta(Coord, VoxelKey);
            if (Delta != 0.0f) {
              D += Delta;
            }
          }
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
          auto BilerpGrid = [&](const TArray<float> &Grid, float LX,
                                float LY) -> float {
            const float GFX = (LX + OverlapOff) / VS;
            const float GFY = (LY + OverlapOff) / VS;
            const int32 GX0 =
                FMath::Clamp(FMath::FloorToInt(GFX), 0, GridX - 2);
            const int32 GY0 =
                FMath::Clamp(FMath::FloorToInt(GFY), 0, GridY - 2);
            const float FX = FMath::Clamp(GFX - GX0, 0.0f, 1.0f);
            const float FY = FMath::Clamp(GFY - GY0, 0.0f, 1.0f);
            const float V00 = Grid[GY0 * GridX + GX0];
            const float V10 = Grid[GY0 * GridX + GX0 + 1];
            const float V01 = Grid[(GY0 + 1) * GridX + GX0];
            const float V11 = Grid[(GY0 + 1) * GridX + GX0 + 1];
            return FMath::Lerp(FMath::Lerp(V00, V10, FX),
                               FMath::Lerp(V01, V11, FX), FY);
          };

          for (int32 VI = 0; VI < 3; ++VI) {
            const FVector &VP = OutMesh.Vertices[BaseIdx + VI];
            OutMesh.UVs.Emplace((ChunkOrigin.X + VP.X) / 100.0f,
                                (ChunkOrigin.Y + VP.Y) / 100.0f);

            const float VTemp = BilerpGrid(TempGrid, VP.X, VP.Y);
            const float VMoist = BilerpGrid(MoistGrid, VP.X, VP.Y);
            const float VHeight = BilerpGrid(HeightGrid, VP.X, VP.Y);
            const float VMask = BilerpGrid(MaskGrid, VP.X, VP.Y);
            const float WorldVX = ChunkOrigin.X + VP.X;
            const float WorldVY = ChunkOrigin.Y + VP.Y;

            FColor VColor;
            if (GTerrainSettings.bEnableRivers) {
              const float HI = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
              const float ISZ = FPMChunkConstants::StarterIslandWorldSize;
              const float RNormX = (WorldVX + HI) / ISZ;
              const float RNormY = (WorldVY + HI) / ISZ;
              const float River =
                  FPMChunkGenerator::RiverFactor(RNormX, RNormY, WorldSeed);
              if (River > 0.3f && VMask > GTerrainSettings.RiverMaskThreshold) {
                VColor =
                    FPMVoxelGenerator::BiomeToVertexColor(EFPMBiome::River);
              } else {
                VColor = FPMChunkGenerator::BlendedBiomeColor(VTemp, VMoist,
                                                              VHeight, VMask);
              }
            } else {
              VColor = FPMChunkGenerator::BlendedBiomeColor(VTemp, VMoist,
                                                            VHeight, VMask);
            }
            // Depth-based strata tint: plains topsoil near surface, rock deeper.
            const float SurfaceAtVertex = TerrainSurfaceZ(WorldVX, WorldVY, WorldSeed);
            const float DepthBelowSurface = FMath::Max(0.0f, SurfaceAtVertex - VP.Z);
            const float RockBlend =
                FMath::Clamp((DepthBelowSurface - 2500.0f) / 9000.0f, 0.0f, 1.0f);
            if (RockBlend > 0.0f) {
              const FColor RockColor(112, 104, 96, VColor.A);
              VColor = FColor(
                  static_cast<uint8>(FMath::Lerp((float)VColor.R, (float)RockColor.R, RockBlend)),
                  static_cast<uint8>(FMath::Lerp((float)VColor.G, (float)RockColor.G, RockBlend)),
                  static_cast<uint8>(FMath::Lerp((float)VColor.B, (float)RockColor.B, RockBlend)),
                  VColor.A);
            }
            OutMesh.Colors.Add(VColor);
          }
        }
      }
    }
  }

  // --- 3. Smooth normals (average across shared vertices) ---
  SmoothNormals(OutMesh);

  // --- 4. Vertex colors are now computed per-vertex (no post-smooth needed)
  // ---

  UE_LOG(LogTemp, Verbose, TEXT("FPM Voxel: Chunk %s -> %d verts, %d tris"),
         *Coord.ToString(), OutMesh.Vertices.Num(),
         OutMesh.Triangles.Num() / 3);
}

// ===================================================================
//  GenerateTerraformTile — Fine-resolution terraform mesh
//  33×33×33 grid at 200cm = 64m tile. Uses same MC algorithm.
// ===================================================================
void FPMVoxelGenerator::GenerateTerraformTile(
    const FVector &TileOrigin, int32 WorldSeed,
    const TMap<FIntVector, float> &FineDeltas, FFPMVoxelMeshData &OutMesh,
    bool bForceBaseSurface) {
  using namespace FPMVoxelConstants;
  OutMesh.Reset();

  const float VS = TerraformVoxelSizeCm; // Fine terraform cell size (cm)
  const int32 GN = TerraformGridN;       // Tile grid corners
  // Build a local active-cube mask so terraform tiles only generate
  // geometry near edited voxels (prevents full-tile visual overlays).
  const FIntVector TileBaseKey(FMath::FloorToInt(TileOrigin.X / VS),
                               FMath::FloorToInt(TileOrigin.Y / VS),
                               FMath::FloorToInt(TileOrigin.Z / VS));
  TSet<FIntVector> ActiveCubes;
  ActiveCubes.Reserve(FineDeltas.Num() * 27);
  for (const TPair<FIntVector, float> &KV : FineDeltas) {
    const FIntVector Local = KV.Key - TileBaseKey;
    for (int32 DZ = -1; DZ <= 1; ++DZ) {
      for (int32 DY = -1; DY <= 1; ++DY) {
        for (int32 DX = -1; DX <= 1; ++DX) {
          const FIntVector Cube(Local.X + DX, Local.Y + DY, Local.Z + DZ);
          if (Cube.X >= 0 && Cube.X < GN - 1 && Cube.Y >= 0 && Cube.Y < GN - 1 &&
              Cube.Z >= 0 && Cube.Z < GN - 1) {
            ActiveCubes.Add(Cube);
          }
        }
      }
    }
  }


  if (bForceBaseSurface) {
    // No edits in this tile: seed active cubes around the procedural surface
    // so we can render fine replacement terrain in the player bubble.
    for (int32 Y = 0; Y < GN - 1; ++Y) {
      const float WorldY = TileOrigin.Y + (Y + 0.5f) * VS;
      for (int32 X = 0; X < GN - 1; ++X) {
        const float WorldX = TileOrigin.X + (X + 0.5f) * VS;
        const float SurfaceZ = TerrainSurfaceZ(WorldX, WorldY, WorldSeed);
        const int32 SurfaceCubeZ =
            FMath::FloorToInt((SurfaceZ - TileOrigin.Z) / VS);

        for (int32 DZ = -2; DZ <= 1; ++DZ) {
          const int32 CZ = SurfaceCubeZ + DZ;
          if (CZ >= 0 && CZ < GN - 1) {
            ActiveCubes.Add(FIntVector(X, Y, CZ));
          }
        }
      }
    }
  }

  if (ActiveCubes.Num() == 0) {
    return;
  }

  int32 MinCubeX = GN, MinCubeY = GN, MinCubeZ = GN;
  int32 MaxCubeX = -1, MaxCubeY = -1, MaxCubeZ = -1;
  for (const FIntVector &Cube : ActiveCubes) {
    MinCubeX = FMath::Min(MinCubeX, Cube.X);
    MinCubeY = FMath::Min(MinCubeY, Cube.Y);
    MinCubeZ = FMath::Min(MinCubeZ, Cube.Z);
    MaxCubeX = FMath::Max(MaxCubeX, Cube.X);
    MaxCubeY = FMath::Max(MaxCubeY, Cube.Y);
    MaxCubeZ = FMath::Max(MaxCubeZ, Cube.Z);
  }

  const int32 MinDX = FMath::Clamp(MinCubeX, 0, GN - 1);
  const int32 MaxDX = FMath::Clamp(MaxCubeX + 1, 0, GN - 1);
  const int32 MinDY = FMath::Clamp(MinCubeY, 0, GN - 1);
  const int32 MaxDY = FMath::Clamp(MaxCubeY + 1, 0, GN - 1);
  const int32 MinDZ = FMath::Clamp(MinCubeZ, 0, GN - 1);
  const int32 MaxDZ = FMath::Clamp(MaxCubeZ + 1, 0, GN - 1);

  // --- 1. Build density field ---
  TArray<float> Density;
  Density.SetNumZeroed(TerraformTotalCorners);

  for (int32 Z = MinDZ; Z <= MaxDZ; ++Z) {
    const float WorldZ = TileOrigin.Z + Z * VS;
    for (int32 Y = MinDY; Y <= MaxDY; ++Y) {
      const float WorldY = TileOrigin.Y + Y * VS;
      for (int32 X = MinDX; X <= MaxDX; ++X) {
        const float WorldX = TileOrigin.X + X * VS;
        const int32 Idx = Z * GN * GN + Y * GN + X;

        // Coarse terrain density: surface Z - voxel Z
        const float SurfZ = TerrainSurfaceZ(WorldX, WorldY, WorldSeed);
        float D = SurfZ - WorldZ;

        // Apply fine overlay delta
        const FIntVector FineKey(FMath::FloorToInt(WorldX / VS),
                                 FMath::FloorToInt(WorldY / VS),
                                 FMath::FloorToInt(WorldZ / VS));
        if (const float *Delta = FineDeltas.Find(FineKey)) {
          D += *Delta;
        }

        Density[Idx] = D;
      }
    }
  }

  // --- 1b. Early out if tile is fully air or fully solid ---
  // This is the key perf optimization: most tiles in a 3x3x3 grid
  // are entirely underground (solid) or above ground (air).
  bool bHasPositive = false;
  bool bHasNegative = false;
  for (int32 Idx = 0; Idx < TerraformTotalCorners; ++Idx) {
    if (Density[Idx] > 0.f) bHasPositive = true;
    else bHasNegative = true;
    if (bHasPositive && bHasNegative) break;
  }
  if (!bHasPositive || !bHasNegative) {
    // Entirely air or entirely solid - no surface to mesh
    return;
  }

  // --- 2. Marching Cubes mesh extraction ---
  // Uses the exact same MC tables as the coarse generator.
  // (EdgeTable and TriTable are defined at file scope)

  for (int32 Z = MinCubeZ; Z <= MaxCubeZ; ++Z) {
    for (int32 Y = MinCubeY; Y <= MaxCubeY; ++Y) {
      for (int32 X = MinCubeX; X <= MaxCubeX; ++X) {
        if (!ActiveCubes.Contains(FIntVector(X, Y, Z)))
          continue;

        // 8 corner densities
        float D[8];
        D[0] = Density[(Z)*GN * GN + (Y)*GN + (X)];
        D[1] = Density[(Z)*GN * GN + (Y)*GN + (X + 1)];
        D[2] = Density[(Z)*GN * GN + (Y + 1) * GN + (X + 1)];
        D[3] = Density[(Z)*GN * GN + (Y + 1) * GN + (X)];
        D[4] = Density[(Z + 1) * GN * GN + (Y)*GN + (X)];
        D[5] = Density[(Z + 1) * GN * GN + (Y)*GN + (X + 1)];
        D[6] = Density[(Z + 1) * GN * GN + (Y + 1) * GN + (X + 1)];
        D[7] = Density[(Z + 1) * GN * GN + (Y + 1) * GN + (X)];

        // Cube index
        int32 CubeIdx = 0;
        for (int32 I = 0; I < 8; ++I)
          if (D[I] > 0.f)
            CubeIdx |= (1 << I);

        if (MCEdgeTable[CubeIdx] == 0)
          continue;

        // Corner positions in world space
        const float BX = TileOrigin.X + X * VS;
        const float BY = TileOrigin.Y + Y * VS;
        const float BZ = TileOrigin.Z + Z * VS;
        FVector C[8] = {
            {BX, BY, BZ},
            {BX + VS, BY, BZ},
            {BX + VS, BY + VS, BZ},
            {BX, BY + VS, BZ},
            {BX, BY, BZ + VS},
            {BX + VS, BY, BZ + VS},
            {BX + VS, BY + VS, BZ + VS},
            {BX, BY + VS, BZ + VS},
        };

        // Edge interpolation
        FVector EdgeVerts[12];
        if (MCEdgeTable[CubeIdx] & 1)
          EdgeVerts[0] = InterpolateEdge(C[0], C[1], D[0], D[1]);
        if (MCEdgeTable[CubeIdx] & 2)
          EdgeVerts[1] = InterpolateEdge(C[1], C[2], D[1], D[2]);
        if (MCEdgeTable[CubeIdx] & 4)
          EdgeVerts[2] = InterpolateEdge(C[2], C[3], D[2], D[3]);
        if (MCEdgeTable[CubeIdx] & 8)
          EdgeVerts[3] = InterpolateEdge(C[3], C[0], D[3], D[0]);
        if (MCEdgeTable[CubeIdx] & 16)
          EdgeVerts[4] = InterpolateEdge(C[4], C[5], D[4], D[5]);
        if (MCEdgeTable[CubeIdx] & 32)
          EdgeVerts[5] = InterpolateEdge(C[5], C[6], D[5], D[6]);
        if (MCEdgeTable[CubeIdx] & 64)
          EdgeVerts[6] = InterpolateEdge(C[6], C[7], D[6], D[7]);
        if (MCEdgeTable[CubeIdx] & 128)
          EdgeVerts[7] = InterpolateEdge(C[7], C[4], D[7], D[4]);
        if (MCEdgeTable[CubeIdx] & 256)
          EdgeVerts[8] = InterpolateEdge(C[0], C[4], D[0], D[4]);
        if (MCEdgeTable[CubeIdx] & 512)
          EdgeVerts[9] = InterpolateEdge(C[1], C[5], D[1], D[5]);
        if (MCEdgeTable[CubeIdx] & 1024)
          EdgeVerts[10] = InterpolateEdge(C[2], C[6], D[2], D[6]);
        if (MCEdgeTable[CubeIdx] & 2048)
          EdgeVerts[11] = InterpolateEdge(C[3], C[7], D[3], D[7]);

        // Emit triangles
        for (int32 I = 0; MCTriTable[CubeIdx][I] != -1; I += 3) {
          const FVector &V0 = EdgeVerts[MCTriTable[CubeIdx][I]];
          const FVector &V1 = EdgeVerts[MCTriTable[CubeIdx][I + 1]];
          const FVector &V2 = EdgeVerts[MCTriTable[CubeIdx][I + 2]];

          // Gradient normal from density field (smooth shading)
          // Compute gradient at each vertex via central differences
          auto GradNormal = [&](const FVector &P) -> FVector {
            const float H = VS * 0.5f;
            auto SampleD = [&](float WX, float WY, float WZ) -> float {
              float D = TerrainSurfaceZ(WX, WY, WorldSeed) - WZ;
              const FIntVector FK(FMath::FloorToInt(WX / VS),
                                  FMath::FloorToInt(WY / VS),
                                  FMath::FloorToInt(WZ / VS));
              if (const float *Dd = FineDeltas.Find(FK))
                D += *Dd;
              return D;
            };
            return FVector(
                       SampleD(P.X - H, P.Y, P.Z) - SampleD(P.X + H, P.Y, P.Z),
                       SampleD(P.X, P.Y - H, P.Z) - SampleD(P.X, P.Y + H, P.Z),
                       SampleD(P.X, P.Y, P.Z - H) - SampleD(P.X, P.Y, P.Z + H))
                .GetSafeNormal();
          };
          FVector N0 = GradNormal(V0);
          FVector N1 = GradNormal(V1);
          FVector N2 = GradNormal(V2);
          // Fallback to face normal if gradient is degenerate
          if (N0.IsNearlyZero() || N1.IsNearlyZero() || N2.IsNearlyZero()) {
            const FVector E1 = V1 - V0;
            const FVector E2 = V2 - V0;
            FVector FN = -FVector::CrossProduct(E1, E2).GetSafeNormal();
            N0 = N1 = N2 = FN;
          }

          // Depth-based soil -> rock transition (Enshrouded-style)
          const float SurfZ0 = TerrainSurfaceZ(V0.X, V0.Y, WorldSeed);
          const float DepthBelow = SurfZ0 - V0.Z; // positive = underground
          const float NormH = FMath::Clamp(V0.Z / 50000.f, 0.f, 1.f);
          FColor VColor;
          if (DepthBelow < 100.f) {
            // Surface / grass layer (< 1m deep)
            const EFPMBiome Biome = BiomeAtWorldXY(V0.X, V0.Y, WorldSeed, NormH);
            VColor = BiomeToVertexColor(Biome);
          } else if (DepthBelow < 400.f) {
            // Soil layer (1-4m deep) - brown dirt
            VColor = FColor(139, 90, 43, 255);
          } else {
            // Rock layer (> 4m deep) - grey stone
            VColor = FColor(128, 128, 128, 255);
          }

          const int32 BaseIdx = OutMesh.Vertices.Num();
          OutMesh.Vertices.Add(V0);
          OutMesh.Vertices.Add(V1);
          OutMesh.Vertices.Add(V2);
          OutMesh.Normals.Add(N0);
          OutMesh.Normals.Add(N1);
          OutMesh.Normals.Add(N2);
          OutMesh.UVs.Add(FVector2D(0, 0));
          OutMesh.UVs.Add(FVector2D(1, 0));
          OutMesh.UVs.Add(FVector2D(0, 1));
          OutMesh.Colors.Add(VColor);
          OutMesh.Colors.Add(VColor);
          OutMesh.Colors.Add(VColor);
          OutMesh.Triangles.Add(BaseIdx);
          OutMesh.Triangles.Add(BaseIdx + 1);
          OutMesh.Triangles.Add(BaseIdx + 2);
        }
      }
    }
  }

  // NOTE: No SmoothNormals() call needed here — we compute per-vertex
  // gradient normals directly from the density field, which are already smooth.

  UE_LOG(LogTemp, Log,
         TEXT("FPM TerraformTile: Origin=(%.0f,%.0f,%.0f) -> %d verts, %d "
              "tris, %d fine deltas"),
         TileOrigin.X, TileOrigin.Y, TileOrigin.Z, OutMesh.Vertices.Num(),
         OutMesh.Triangles.Num() / 3, FineDeltas.Num());
}


