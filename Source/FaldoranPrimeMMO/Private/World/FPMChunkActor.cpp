// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkActor.h"
#include "UObject/ConstructorHelpers.h"
#include "World/FPMBiomePCGConfig.h"
#include "World/FPMBiomePCGSpawner.h"
#include "World/FPMChunkData.h"

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

  // Async collision cooking — prevents game-thread stalls.
  TerrainMesh->bUseAsyncCooking = true;

  TerrainMesh->SetCastShadow(true);
  TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
  TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  // Load the terrain material (uses vertex colors for biome splatting)
  static ConstructorHelpers::FObjectFinder<UMaterialInterface> TerrainMatFinder(
      TEXT("/Game/Materials/Landscape/M_ChunkTerrain"));
  if (TerrainMatFinder.Succeeded()) {
    TerrainMaterial = TerrainMatFinder.Object;
  }
}

// =====================================================================
//  Height / Color helpers
// =====================================================================

float AFPMChunkActor::HeightToWorldZ(float NormalizedHeight) {
  return FPMChunkConstants::MinWorldZ +
         NormalizedHeight * FPMChunkConstants::WorldHeightRange;
}

FColor AFPMChunkActor::BiomeToVertexColor(EFPMBiome Biome) {
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
    BuildMesh(4, false);
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

  // Only clear biome when dropping to Low or Unloaded.
  // Trees/rocks stay visible on both Full AND Medium LOD chunks.
  if ((NewLOD == EFPMChunkLOD::Low || NewLOD == EFPMChunkLOD::Unloaded) &&
      bBiomePopulated) {
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
    BuildMesh(4, false);
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
  UE_LOG(LogTemp, Warning,
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
    }
  }

  // ---- 1b. Smooth vertex colors (biome transition blending) ----
  // Box blur on the grid: average each vertex color with its cardinal
  // neighbors.  Run multiple iterations for a wider gradient.
  constexpr int32 BlurPasses = 2;
  for (int32 Pass = 0; Pass < BlurPasses; ++Pass) {
    TArray<FColor> Blurred;
    Blurred.SetNum(NumVerts);

    for (int32 Y = 0; Y < Res; ++Y) {
      for (int32 X = 0; X < Res; ++X) {
        const int32 Idx = Y * Res + X;
        int32 SumR = VColors[Idx].R;
        int32 SumG = VColors[Idx].G;
        int32 SumB = VColors[Idx].B;
        int32 SumA = VColors[Idx].A;
        int32 Count = 1;

        // Cardinal neighbors
        if (X > 0) {
          const FColor &C = VColors[Idx - 1];
          SumR += C.R;
          SumG += C.G;
          SumB += C.B;
          SumA += C.A;
          ++Count;
        }
        if (X < Res - 1) {
          const FColor &C = VColors[Idx + 1];
          SumR += C.R;
          SumG += C.G;
          SumB += C.B;
          SumA += C.A;
          ++Count;
        }
        if (Y > 0) {
          const FColor &C = VColors[Idx - Res];
          SumR += C.R;
          SumG += C.G;
          SumB += C.B;
          SumA += C.A;
          ++Count;
        }
        if (Y < Res - 1) {
          const FColor &C = VColors[Idx + Res];
          SumR += C.R;
          SumG += C.G;
          SumB += C.B;
          SumA += C.A;
          ++Count;
        }

        Blurred[Idx] = FColor(
            static_cast<uint8>(SumR / Count), static_cast<uint8>(SumG / Count),
            static_cast<uint8>(SumB / Count), static_cast<uint8>(SumA / Count));
      }
    }
    VColors = MoveTemp(Blurred);
  }

  // ---- 2. Triangles (alternating diagonal to prevent sawtooth) ----
  const int32 NumQuads = (Res - 1) * (Res - 1);
  TArray<int32> Tris;
  Tris.Reserve(NumQuads * 6);

  for (int32 Y = 0; Y < Res - 1; ++Y) {
    for (int32 X = 0; X < Res - 1; ++X) {
      const int32 BL = Y * Res + X;
      const int32 BR = BL + 1;
      const int32 TL = BL + Res;
      const int32 TR = TL + 1;

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
    const FVector &A = Verts[Tris[i]];
    const FVector &B = Verts[Tris[i + 1]];
    const FVector &C = Verts[Tris[i + 2]];

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
  constexpr float SkirtDrop = 200.f;

  auto AddSkirtQuad = [&](int32 IdxA, int32 IdxB) {
    const FVector PA = Verts[IdxA];
    const FVector PB = Verts[IdxB];
    const FVector2D UvA = UVs[IdxA];
    const FVector2D UvB = UVs[IdxB];
    const FColor ColA = VColors[IdxA];
    const FColor ColB = VColors[IdxB];

    const FVector NormalA = Normals[IdxA];
    const FVector NormalB = Normals[IdxB];

    const int32 BotA = Verts.Num();
    Verts.Emplace(PA.X, PA.Y, PA.Z - SkirtDrop);
    UVs.Add(UvA);
    VColors.Add(ColA);
    Normals.Add(NormalA);

    const int32 BotB = Verts.Num();
    Verts.Emplace(PB.X, PB.Y, PB.Z - SkirtDrop);
    UVs.Add(UvB);
    VColors.Add(ColB);
    Normals.Add(NormalB);

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

  // Apply terrain material (vertex color biome splatting)
  if (TerrainMaterial) {
    TerrainMesh->SetMaterial(0, TerrainMaterial);
  }

  TerrainMesh->SetCollisionEnabled(bCollision
                                       ? ECollisionEnabled::QueryAndPhysics
                                       : ECollisionEnabled::NoCollision);
}

// =====================================================================
//  Voxel Chunk Initialization (Marching Cubes mesh)
// =====================================================================

void AFPMChunkActor::InitializeVoxelChunk(const FFPMVoxelMeshData &MeshData,
                                          const FFPMChunkCoord &Coord) {
  ChunkData.Coord = Coord;
  CurrentLOD = EFPMChunkLOD::Full;

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

  // Apply terrain material (vertex color biome splatting)
  if (TerrainMaterial) {
    TerrainMesh->SetMaterial(0, TerrainMaterial);
  }

  TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  // Voxel chunks also get biome population (server skips foliage)
  PopulateBiome();
}
