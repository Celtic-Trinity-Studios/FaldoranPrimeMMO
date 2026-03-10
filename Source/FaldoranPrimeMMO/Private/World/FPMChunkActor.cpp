// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkActor.h"
#include "Engine/Engine.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "World/FPMBiomePCGConfig.h"
#include "World/FPMBiomePCGSpawner.h"
#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"

// =====================================================================
//  Constructor
// =====================================================================

AFPMChunkActor::AFPMChunkActor() {
  PrimaryActorTick.bCanEverTick = false;

  // Chunks are generated independently on server & client from the
  // shared WorldSeed — no replication needed, avoids overhead for
  // hundreds of procedural mesh actors.
  bReplicates = false;
  bNetLoadOnClient = false;

  TerrainMesh =
      CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
  RootComponent = TerrainMesh;

  // Per-triangle collision (not convex hull).
  TerrainMesh->bUseComplexAsSimpleCollision = true;

  // Asynchronous collision cooking — physics mesh is baked in the background
  // instead of stalling the game thread for 1-3ms per chunk. The safety floor
  // mesh (BuildSafetyFloorAt) prevents fall-through during the brief cook
  // delay (~1-2 frames). This saves 3-12ms/frame during chunk loading bursts.
  TerrainMesh->bUseAsyncCooking = true;

  TerrainMesh->SetCastShadow(true);
  TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
  TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  // DEBUG MODE: Skip texture-based material, use vertex-color-only material.
  // This avoids loading heavy landscape textures during testing.
  // To restore textures, uncomment the M_ChunkTerrain loader below.
  //
  // static ConstructorHelpers::FObjectFinder<UMaterialInterface>
  // TerrainMatFinder(
  //     TEXT("/Game/Materials/Landscape/M_ChunkTerrain"));
  // if (TerrainMatFinder.Succeeded()) {
  //   TerrainMaterial = TerrainMatFinder.Object;
  // }
  TerrainMaterial = nullptr; // Will use GEngine->VertexColorMaterial at runtime
}

// =====================================================================
//  Height / Color helpers
// =====================================================================

float AFPMChunkActor::HeightToWorldZ(float NormalizedHeight) {
  return FPMChunkConstants::MinWorldZ +
         NormalizedHeight * FPMChunkConstants::WorldHeightRange;
}

FColor AFPMChunkActor::BiomeToVertexColor(EFPMBiome Biome) {
  // Flat debug colors — each biome gets a distinct, intuitive color.
  // No texture splatting, just raw vertex color for fast testing.
  switch (Biome) {
  // --- Climate biomes (lowland) ---
  case EFPMBiome::Meadows:
    return FColor(120, 200, 80, 255); // Bright grass green
  case EFPMBiome::Forest:
    return FColor(34, 120, 34, 255); // Dark forest green
  case EFPMBiome::Plains:
    return FColor(210, 190, 100, 255); // Dry golden grass
  case EFPMBiome::Savanna:
    return FColor(200, 170, 60, 255); // Warm savanna gold
  case EFPMBiome::Jungle:
    return FColor(15, 90, 25, 255); // Deep tropical green
  case EFPMBiome::Desert:
    return FColor(220, 195, 130, 255); // Sandy tan
  case EFPMBiome::Taiga:
    return FColor(70, 110, 70, 255); // Muted cold green
  case EFPMBiome::BorealForest:
    return FColor(45, 85, 55, 255); // Dark spruce green
  case EFPMBiome::Tundra:
    return FColor(160, 170, 175, 255); // Cold grey-blue
  case EFPMBiome::Swamp:
    return FColor(75, 95, 45, 255); // Murky olive-brown

  // --- Elevation biomes ---
  case EFPMBiome::Alpine:
    return FColor(150, 155, 130, 255); // Rocky green-grey
  case EFPMBiome::Mountain:
    return FColor(130, 120, 110, 255); // Grey-brown rock
  case EFPMBiome::Snow:
    return FColor(245, 248, 255, 255); // Pure white snow

  // --- Water biomes ---
  case EFPMBiome::River:
    return FColor(50, 120, 200, 255); // Clear blue river
  case EFPMBiome::Coast:
    return FColor(170, 160, 140, 255); // Rocky shore tan
  case EFPMBiome::Beach:
    return FColor(235, 215, 165, 255); // Warm sand
  case EFPMBiome::Ocean:
    return FColor(25, 70, 150, 255); // Deep ocean blue

  default:
    return FColor(180, 50, 200, 255); // Magenta = unmapped (debug)
  }
}

// =====================================================================
//  Biome PCG Config
// =====================================================================

void AFPMChunkActor::SetBiomePCGConfig(const UFPMBiomePCGConfig *InConfig,
                                       int32 InWorldSeed) {
  BiomePCGConfig = InConfig;
  CachedWorldSeed = InWorldSeed;
}

// =====================================================================
//  InitializeChunk
// =====================================================================

void AFPMChunkActor::InitializeChunk(const FFPMChunkHeightmapData &InData,
                                     EFPMChunkLOD InLOD) {
  ChunkData = InData;
  CurrentLOD = InLOD;

  SetActorLocation(FPMChunkGenerator::ChunkToWorldOrigin(InData.Coord));

  switch (InLOD) {
  case EFPMChunkLOD::Full:
    BuildMesh(1, true);
    break;
  case EFPMChunkLOD::Medium:
    BuildMesh(2, true);
    break;
  case EFPMChunkLOD::Low:
    BuildMesh(4, true);
    break;
  case EFPMChunkLOD::Unloaded:
    TerrainMesh->ClearAllMeshSections();
    ClearBiome();
    return; // Don't populate unloaded chunks
  }

  // Populate biome only at Full LOD
  if (InLOD == EFPMChunkLOD::Full) {
    PopulateBiome();
  }
}

// =====================================================================
//  SetChunkLOD
// =====================================================================

void AFPMChunkActor::SetChunkLOD(EFPMChunkLOD NewLOD) {
  if (NewLOD == CurrentLOD)
    return;

  // Voxel chunks: the Marching Cubes mesh cannot be rebuilt by BuildMesh
  // (it requires HeightValues which voxel chunks don't have).
  // Keep the original mesh for all LODs; only truly destroy on Unloaded.
  if (bIsVoxelChunk) {
    if (NewLOD == EFPMChunkLOD::Unloaded) {
      TerrainMesh->ClearAllMeshSections();
      ClearBiome();
    }
    // Keep trees/rocks visible on Low + Medium LOD (HISM handles distance)
    CurrentLOD = NewLOD;
    return;
  }

  // Heightmap chunks: rebuild mesh at appropriate resolution
  // Only clear biome when fully unloading. Trees stay on Full/Medium/Low.
  if (NewLOD == EFPMChunkLOD::Unloaded && bBiomePopulated) {
    ClearBiome();
  }

  CurrentLOD = NewLOD;

  switch (NewLOD) {
  case EFPMChunkLOD::Full:
    BuildMesh(1, true);
    PopulateBiome(); // Trees spawn here (or persist from before)
    break;
  case EFPMChunkLOD::Medium:
    BuildMesh(2, true);
    PopulateBiome(); // Keep trees alive on Medium LOD too
    break;
  case EFPMChunkLOD::Low:
    BuildMesh(4, true);
    break;
  case EFPMChunkLOD::Unloaded:
    TerrainMesh->ClearAllMeshSections();
    break;
  }
}

// =====================================================================
//  PopulateBiome — Spawn trees/rocks using HISM
// =====================================================================

void AFPMChunkActor::PopulateBiome() {
  if (bBiomePopulated) {
    return; // Already populated
  }

  if (!BiomePCGConfig) {
    UE_LOG(LogTemp, Verbose,
           TEXT("FPM: Chunk %s — no BiomePCGConfig, skipping population"),
           *ChunkData.Coord.ToString());
    return;
  }

  // Skip foliage spawning on dedicated servers — purely visual, wastes CPU
  if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer) {
    bBiomePopulated = true;
    return;
  }

  // Only pass heightmap data if it was actually populated.
  // Voxel-initialized chunks (InitializeVoxelChunk) don't fill HeightValues,
  // so passing &ChunkData would cause SampleHeightmapZ to index into an
  // empty array and crash.
  const FFPMChunkHeightmapData *HeightPtr =
      (ChunkData.bIsValid && ChunkData.HeightValues.Num() > 0) ? &ChunkData
                                                               : nullptr;
  UE_LOG(
      LogTemp, Warning,
      TEXT("FPM PCG: PopulateBiome for chunk %s (VoxelVerts=%d, NetMode=%d)"),
      *ChunkData.Coord.ToString(), CachedVoxelMesh.Vertices.Num(),
      GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1);
  FPMBiomePCGSpawner::PopulateChunk(
      this, BiomePCGConfig, ChunkData.Coord, CachedWorldSeed,
      CachedVoxelMesh.Vertices.Num() > 0 ? &CachedVoxelMesh : nullptr,
      HeightPtr);

  // Free mesh data — no longer needed after placement is computed
  CachedVoxelMesh.Reset();

  bBiomePopulated = true;
}

// =====================================================================
//  ClearBiome — Remove all spawned instances
// =====================================================================

void AFPMChunkActor::ClearBiome() {
  if (!bBiomePopulated) {
    return;
  }

  FPMBiomePCGSpawner::ClearSpawnedInstances(this);
  bBiomePopulated = false;
}

// =====================================================================
//  BuildMesh
// =====================================================================

void AFPMChunkActor::BuildMesh(int32 LODStep, bool bCollision) {
  if (!ChunkData.bIsValid) {
    return;
  }

  TerrainMesh->ClearAllMeshSections();

  constexpr int32 FullRes = FPMChunkConstants::ChunkResolution;
  const int32 Res = (FullRes - 1) / LODStep + 1;
  // Hex bounding box: width = 2*OuterRadius, height = 2*InnerRadius
  constexpr float SizeX = FPMChunkConstants::ChunkWorldSize;
  constexpr float SizeY = FPMChunkConstants::ChunkWorldSize;

  if (Res <= 1) {
    return;
  }

  // ---- 1. Vertices ----
  const int32 NumVerts = Res * Res;

  TArray<FVector> Verts;
  TArray<FVector2D> UVs;
  TArray<FColor> VColors;

  Verts.Reserve(NumVerts);
  UVs.Reserve(NumVerts);
  VColors.Reserve(NumVerts);

  for (int32 Y = 0; Y < Res; ++Y) {
    for (int32 X = 0; X < Res; ++X) {
      const int32 SrcX = FMath::Min(X * LODStep, FullRes - 1);
      const int32 SrcY = FMath::Min(Y * LODStep, FullRes - 1);
      const int32 SrcIdx = SrcY * FullRes + SrcX;

      const float U = static_cast<float>(X) / (Res - 1);
      const float V = static_cast<float>(Y) / (Res - 1);

      Verts.Emplace(U * SizeX, V * SizeY,
                    HeightToWorldZ(ChunkData.HeightValues[SrcIdx]));
      UVs.Emplace(U, V);
      VColors.Add(BiomeToVertexColor(ChunkData.BiomeValues[SrcIdx]));

      // Override colour with winner-biome display colour if climate data
      // available
      if (ChunkData.BiomeNoiseValues.Num() > SrcIdx) {
        const float H = ChunkData.HeightValues[SrcIdx];
        const FVector CC =
            FPMChunkGenerator::ChunkToWorldCenter(ChunkData.Coord);
        const float BH = FPMChunkConstants::ChunkWorldSize * 0.5f;
        const float VWX = CC.X + (U * 2.0f - 1.0f) * BH;
        const float VWY = CC.Y + (V * 2.0f - 1.0f) * BH;

        const float IslandMask = FPMNoise::IslandMask(VWX, VWY, 0);
        const EFPMBiome WinnerBiome = FPMChunkGenerator::AssignBiomeWeighted(
            FPMNoise::Temperature(VWX, VWY, 0), FPMNoise::Moisture(VWX, VWY, 0),
            H, IslandMask, 0.5f, 0);
        VColors.Last() = BiomeToVertexColor(WinnerBiome);
      }
    }
  }

  // ---- 1b. Smooth vertex colors (biome transition blending) ----
  // Full 3x3 box blur: averages with all 8 neighbors for organic blending.
  constexpr int32 BlurPasses = 3; // Reduced from 5 — prevents over-averaging
  for (int32 Pass = 0; Pass < BlurPasses; ++Pass) {
    TArray<FColor> Blurred;
    Blurred.SetNum(NumVerts);
    for (int32 Y = 0; Y < Res; ++Y) {
      for (int32 X = 0; X < Res; ++X) {
        const int32 Idx = Y * Res + X;
        int32 SumR = VColors[Idx].R * 2;
        int32 SumG = VColors[Idx].G * 2;
        int32 SumB = VColors[Idx].B * 2;
        int32 SumA = VColors[Idx].A * 2;
        int32 Count = 2;
        auto AddN = [&](int32 NIdx) {
          SumR += VColors[NIdx].R;
          SumG += VColors[NIdx].G;
          SumB += VColors[NIdx].B;
          SumA += VColors[NIdx].A;
          ++Count;
        };
        if (X > 0)
          AddN(Idx - 1);
        if (X < Res - 1)
          AddN(Idx + 1);
        if (Y > 0)
          AddN(Idx - Res);
        if (Y < Res - 1)
          AddN(Idx + Res);
        if (X > 0 && Y > 0)
          AddN(Idx - Res - 1);
        if (X < Res - 1 && Y > 0)
          AddN(Idx - Res + 1);
        if (X > 0 && Y < Res - 1)
          AddN(Idx + Res - 1);
        if (X < Res - 1 && Y < Res - 1)
          AddN(Idx + Res + 1);
        Blurred[Idx] = FColor(uint8(SumR / Count), uint8(SumG / Count),
                              uint8(SumB / Count), uint8(SumA / Count));
      }
    }
    VColors = MoveTemp(Blurred);
  }

  // ---- 2. Triangles ----
  const int32 NumQuads = (Res - 1) * (Res - 1);
  TArray<int32> Tris;
  Tris.Reserve(NumQuads * 6);
  for (int32 Y = 0; Y < Res - 1; ++Y) {
    for (int32 X = 0; X < Res - 1; ++X) {
      const int32 BL = Y * Res + X, BR = BL + 1;
      const int32 TL = BL + Res, TR = TL + 1;
      if ((X + Y) % 2 == 0) {
        Tris.Add(BL);
        Tris.Add(TR);
        Tris.Add(TL);
        Tris.Add(BL);
        Tris.Add(BR);
        Tris.Add(TR);
      } else {
        Tris.Add(BL);
        Tris.Add(BR);
        Tris.Add(TL);
        Tris.Add(BR);
        Tris.Add(TR);
        Tris.Add(TL);
      }
    }
  }

  // ---- 3. Normals ----
  TArray<FVector> Normals;
  Normals.SetNumZeroed(NumVerts);
  for (int32 i = 0; i < Tris.Num(); i += 3) {
    const FVector &A = Verts[Tris[i]], &B = Verts[Tris[i + 1]],
                  &C = Verts[Tris[i + 2]];
    FVector FaceN = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
    Normals[Tris[i]] += FaceN;
    Normals[Tris[i + 1]] += FaceN;
    Normals[Tris[i + 2]] += FaceN;
  }
  for (FVector &N : Normals) {
    N = N.GetSafeNormal();
    if (N.IsNearlyZero())
      N = FVector::UpVector;
  }

  // ---- 4. Skirts ----
  constexpr float SkirtDrop = 3200.f;
  auto AddSkirtQuad = [&](int32 IdxA, int32 IdxB) {
    const int32 BotA = Verts.Num();
    Verts.Emplace(Verts[IdxA].X, Verts[IdxA].Y, Verts[IdxA].Z - SkirtDrop);
    UVs.Add(UVs[IdxA]);
    VColors.Add(VColors[IdxA]);
    Normals.Add(Normals[IdxA]);
    const int32 BotB = Verts.Num();
    Verts.Emplace(Verts[IdxB].X, Verts[IdxB].Y, Verts[IdxB].Z - SkirtDrop);
    UVs.Add(UVs[IdxB]);
    VColors.Add(VColors[IdxB]);
    Normals.Add(Normals[IdxB]);
    Tris.Add(IdxA);
    Tris.Add(BotA);
    Tris.Add(BotB);
    Tris.Add(IdxA);
    Tris.Add(BotB);
    Tris.Add(IdxB);
  };
  for (int32 X = 0; X < Res - 1; ++X)
    AddSkirtQuad(X + 1, X);
  for (int32 X = 0; X < Res - 1; ++X) {
    const int32 Row = (Res - 1) * Res;
    AddSkirtQuad(Row + X, Row + X + 1);
  }
  for (int32 Y = 0; Y < Res - 1; ++Y)
    AddSkirtQuad(Y * Res, (Y + 1) * Res);
  for (int32 Y = 0; Y < Res - 1; ++Y) {
    const int32 Col = Res - 1;
    AddSkirtQuad((Y + 1) * Res + Col, Y * Res + Col);
  }

  TArray<FProcMeshTangent> Tangents;
  TerrainMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, VColors,
                                 Tangents, bCollision);

  // Apply material: editor-assigned > M_TerrainBiome auto-load > fallback
  // NOTE: We retry load until it succeeds — avoids the startup race where the
  // auto-builder hasn't created the asset yet when the first chunk is built.
  UMaterialInterface *MatToUse = TerrainMaterial;
  if (!MatToUse) {
    static UMaterialInterface *SCachedMat = nullptr;
    if (!SCachedMat) {
      SCachedMat = Cast<UMaterialInterface>(
          StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                           TEXT("/Game/Materials/M_TerrainBiome")));
      // Only fall back to GEngine vertex-color material if the named asset
      // genuinely doesn't exist (not just a timing failure).  We keep
      // SCachedMat == nullptr so we retry on the next chunk build.
      if (!SCachedMat && GEngine &&
          !FPackageName::DoesPackageExist(
              TEXT("/Game/Materials/M_TerrainBiome"))) {
        SCachedMat = GEngine->VertexColorMaterial;
      }
    }
    MatToUse = SCachedMat;
  }
  if (MatToUse)
    TerrainMesh->SetMaterial(0, MatToUse);

  TerrainMesh->SetCollisionEnabled(bCollision
                                       ? ECollisionEnabled::QueryAndPhysics
                                       : ECollisionEnabled::NoCollision);
} // end AFPMChunkActor::BuildMesh

// =====================================================================
//  Voxel Chunk Initialization (Marching Cubes mesh)
// =====================================================================

void AFPMChunkActor::InitializeVoxelChunk(const FFPMVoxelMeshData &MeshData,
                                          const FFPMChunkCoord &Coord) {
  ChunkData.Coord = Coord;
  CurrentLOD = EFPMChunkLOD::Full;
  bIsVoxelChunk = true;

  if (MeshData.Vertices.Num() == 0) {
    return;
  }

  // Cache for tree/rock Z-position snapping
  CachedVoxelMesh = MeshData;

  const FVector Origin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
  SetActorLocation(Origin);

  TArray<FProcMeshTangent> Tangents;

  TerrainMesh->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles,
                                 MeshData.Normals, MeshData.UVs,
                                 MeshData.Colors, Tangents, true);

  // Apply material: same retry-until-success pattern as BuildMesh.
  UMaterialInterface *VoxelMat = TerrainMaterial;
  if (!VoxelMat) {
    static UMaterialInterface *SCachedVoxelMat = nullptr;
    if (!SCachedVoxelMat) {
      SCachedVoxelMat = Cast<UMaterialInterface>(
          StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                           TEXT("/Game/Materials/M_TerrainBiome")));
      if (!SCachedVoxelMat && GEngine &&
          !FPackageName::DoesPackageExist(
              TEXT("/Game/Materials/M_TerrainBiome"))) {
        SCachedVoxelMat = GEngine->VertexColorMaterial;
      }
    }
    VoxelMat = SCachedVoxelMat;
  }
  if (VoxelMat) {
    TerrainMesh->SetMaterial(0, VoxelMat);
  }

  TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  // Voxel chunks also get biome population (server skips foliage)
  PopulateBiome();
}
