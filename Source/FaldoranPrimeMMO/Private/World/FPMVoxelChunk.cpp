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
      // Carve DOWN from local terrain by RiverCarveDepth.
      // River factor (0-1) acts as a cross-section profile:
      //   center of river (River≈1) gets full depth,
      //   edges (River≈0) get a shallow bank.
      const float CarveAmount =
          S.RiverCarveDepth * River * S.RiverCarveStrength;

      // One-time debug: log actual carving values
      static bool bCarveDebug = false;
      if (!bCarveDebug) {
        bCarveDebug = true;
        UE_LOG(LogTemp, Warning,
               TEXT("FPM RIVER CARVE: LandHeight=%.6f, River=%.4f, "
                    "CarveDepth=%.4f, CarveStrength=%.2f, CarveAmount=%.6f, "
                    "NewHeight=%.6f, WorldZ=%.1f -> %.1f (delta=%.1fcm)"),
               LandHeight, River, S.RiverCarveDepth, S.RiverCarveStrength,
               CarveAmount, LandHeight - CarveAmount,
               S.HeightBase + LandHeight * S.HeightScale,
               S.HeightBase + (LandHeight - CarveAmount) * S.HeightScale,
               CarveAmount * S.HeightScale);
      }

      LandHeight -= CarveAmount;
    }
    // One-time diagnostic: sample near player spawn
    static bool bRiverDiag = false;
    if (!bRiverDiag && FMath::Abs(WorldX) < 200000.0f &&
        FMath::Abs(WorldY) < 200000.0f) {
      bRiverDiag = true;
      // Test multiple points along R=3 river direction (315 degrees)
      for (int32 D = 0; D < 20; ++D) {
        float TestDist = D * 0.02f; // 0 to 0.38 in normalized
        float TX = 0.5f + TestDist * 0.707f;
        float TY = 0.5f - TestDist * 0.707f;
        float RF = FPMChunkGenerator::RiverFactor(TX, TY, WorldSeed);
        UE_LOG(LogTemp, Warning,
               TEXT("FPM RIVER DIAG: Norm(%.3f,%.3f) RF=%.4f Mask=%.3f "
                    "MaskThresh=%.3f bEnable=%d"),
               TX, TY, RF, Mask, S.RiverMaskThreshold, S.bEnableRivers);
      }
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

  // --- Altitude Diagnostic ---
  static bool bAltDiag = false;
  if (!bAltDiag) {
    bAltDiag = true;
    const float ExpectedSurfaceZ = TerrainSurfaceZ(WorldX, WorldY, WorldSeed);
    UE_LOG(LogTemp, Warning,
           TEXT("FPM PLAYER ALT DIAG: WorldX=%.1f WorldP.Z=%.1f, NormH=%.4f, "
                "ExpectedSurfaceZ=%.1f (Delta=%.1f) BiomeIdx=%d"),
           WorldX, (NormalizedHeight * 2000000.0f) - 1100000.0f,
           NormalizedHeight, ExpectedSurfaceZ,
           ((NormalizedHeight * 2000000.0f) - 1100000.0f) - ExpectedSurfaceZ,
           static_cast<int32>(Biome));
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
    return FColor(0, 255, 255, 0); // DEBUG: bright cyan for rivers
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

          // UVs: world-space XY projected
          for (int32 VI = 0; VI < 3; ++VI) {
            const FVector &VP = OutMesh.Vertices[BaseIdx + VI];
            OutMesh.UVs.Emplace((ChunkOrigin.X + VP.X) / 100.0f,
                                (ChunkOrigin.Y + VP.Y) / 100.0f);
          }

          // Use the SAME biome code path as the HUD so terrain colour always
          // matches the displayed biome name.
          // WorldToIslandNorm converts world cm → normalised [0,1] island
          // coords. AssignBiomeFromNoise uses those same coords internally
          // (matching HUD).
          const FVector TriCentroid = (V0 + V1 + V2) / 3.0f;
          const float WorldCX = ChunkOrigin.X + TriCentroid.X;
          const float WorldCY = ChunkOrigin.Y + TriCentroid.Y;

          float NormX, NormY;
          FPMChunkGenerator::WorldToIslandNorm(FVector(WorldCX, WorldCY, 0.0f),
                                               NormX, NormY);

          const EFPMBiome WBiome = FPMChunkGenerator::AssignBiomeFromNoise(
              NormX, NormY, WorldSeed, 0.5f);
          const FColor BColor = FPMVoxelGenerator::BiomeToVertexColor(WBiome);
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

  // One-time diagnostic: scan vertex Z range to verify carving
  static bool bMeshZDiag = false;
  if (!bMeshZDiag && OutMesh.Vertices.Num() > 0) {
    bMeshZDiag = true;
    float MinZ = TNumericLimits<float>::Max();
    float MaxZ = TNumericLimits<float>::Lowest();
    for (const FVector &V : OutMesh.Vertices) {
      MinZ = FMath::Min(MinZ, V.Z);
      MaxZ = FMath::Max(MaxZ, V.Z);
    }
    UE_LOG(LogTemp, Warning,
           TEXT("FPM MESH DIAG: Chunk %s -> %d verts, Z range [%.1f .. %.1f] "
                "(span=%.1fcm)"),
           *Coord.ToString(), OutMesh.Vertices.Num(), MinZ, MaxZ, MaxZ - MinZ);
  }
}
