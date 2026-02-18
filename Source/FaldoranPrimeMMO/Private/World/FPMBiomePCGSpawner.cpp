// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMBiomePCGSpawner.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "World/FPMVoxelChunk.h"

// Tag applied to HISM components so we can find/remove them later
static const FName BiomeHISMTag = TEXT("FPM_BiomeHISM");

// =====================================================================
//  FMeshHeightGrid — 2D spatial grid for fast triangle-based Z lookup.
//
//  Built once per chunk from the MC mesh data. Each cell stores the
//  indices of triangles whose XY bounding box overlaps it. Looking up
//  the Z for any XY point only needs to test the handful of triangles
//  in that cell (~5-10) instead of walking all ~2000+ triangles.
// =====================================================================

struct FMeshHeightGrid {
  static constexpr int32 GridRes = 32; // matches ChunkVoxelsXY
  float CellSize = 0.0f;

  // Each cell stores a list of triangle indices (into the Triangles array)
  TArray<int32> CellTriangles[GridRes][GridRes];

  // Cache the mesh data pointer for vertex lookups
  const FFPMVoxelMeshData *Mesh = nullptr;

  void Build(const FFPMVoxelMeshData &MeshData) {
    Mesh = &MeshData;
    CellSize = FPMChunkConstants::ChunkWorldSize / static_cast<float>(GridRes);
    const int32 NumTris = MeshData.Triangles.Num() / 3;

    for (int32 Tri = 0; Tri < NumTris; ++Tri) {
      const FVector &V0 = MeshData.Vertices[MeshData.Triangles[Tri * 3]];
      const FVector &V1 = MeshData.Vertices[MeshData.Triangles[Tri * 3 + 1]];
      const FVector &V2 = MeshData.Vertices[MeshData.Triangles[Tri * 3 + 2]];

      // Skip near-vertical triangles (walls) — no trees there.
      // We use Abs() because Marching Cubes has inconsistent winding order,
      // so ground surfaces may have either +Z or -Z normals.
      FVector FaceNormal =
          FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
      if (FMath::Abs(FaceNormal.Z) < 0.3f) {
        continue;
      }

      // XY bounding box → grid cell range
      const float MinX = FMath::Min3(V0.X, V1.X, V2.X);
      const float MaxX = FMath::Max3(V0.X, V1.X, V2.X);
      const float MinY = FMath::Min3(V0.Y, V1.Y, V2.Y);
      const float MaxY = FMath::Max3(V0.Y, V1.Y, V2.Y);

      const int32 CX0 =
          FMath::Clamp(static_cast<int32>(MinX / CellSize), 0, GridRes - 1);
      const int32 CX1 =
          FMath::Clamp(static_cast<int32>(MaxX / CellSize), 0, GridRes - 1);
      const int32 CY0 =
          FMath::Clamp(static_cast<int32>(MinY / CellSize), 0, GridRes - 1);
      const int32 CY1 =
          FMath::Clamp(static_cast<int32>(MaxY / CellSize), 0, GridRes - 1);

      for (int32 CY = CY0; CY <= CY1; ++CY) {
        for (int32 CX = CX0; CX <= CX1; ++CX) {
          CellTriangles[CY][CX].Add(Tri);
        }
      }
    }
  }

  /** Find the mesh surface Z at a local XY position. Returns false if no
   *  triangle was found (point is off the mesh edge). */
  bool SampleZ(float LocalX, float LocalY, float &OutZ) const {
    if (!Mesh || CellSize <= 0.0f) {
      return false;
    }

    const int32 CX =
        FMath::Clamp(static_cast<int32>(LocalX / CellSize), 0, GridRes - 1);
    const int32 CY =
        FMath::Clamp(static_cast<int32>(LocalY / CellSize), 0, GridRes - 1);

    const TArray<int32> &Tris = CellTriangles[CY][CX];
    float BestZ = 1e18f; // Start high — we want the LOWEST surface (ground)
    bool bFound = false;

    for (int32 TriIdx : Tris) {
      const FVector &V0 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3]];
      const FVector &V1 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3 + 1]];
      const FVector &V2 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3 + 2]];

      // Barycentric coordinates in 2D (XY projection)
      const float E1X = V1.X - V0.X, E1Y = V1.Y - V0.Y;
      const float E2X = V2.X - V0.X, E2Y = V2.Y - V0.Y;
      const float PX = LocalX - V0.X, PY = LocalY - V0.Y;

      const float D00 = E1X * E1X + E1Y * E1Y;
      const float D01 = E1X * E2X + E1Y * E2Y;
      const float D11 = E2X * E2X + E2Y * E2Y;
      const float D20 = PX * E1X + PY * E1Y;
      const float D21 = PX * E2X + PY * E2Y;

      const float Denom = D00 * D11 - D01 * D01;
      if (FMath::Abs(Denom) < SMALL_NUMBER) {
        continue;
      }

      const float U = (D11 * D20 - D01 * D21) / Denom;
      const float V = (D00 * D21 - D01 * D20) / Denom;
      const float W = 1.0f - U - V;

      if (U >= -0.01f && V >= -0.01f && W >= -0.01f) {
        const float InterpZ = W * V0.Z + U * V1.Z + V * V2.Z;
        // Pick lowest surface — trees go on ground, not overhangs
        if (InterpZ < BestZ) {
          BestZ = InterpZ;
          bFound = true;
        }
      }
    }

    if (bFound) {
      OutZ = BestZ;
    }
    return bFound;
  }
};

static void SnapPointsToGrid(TArray<FTransform> &Points,
                             const FMeshHeightGrid &Grid,
                             float ZOffset = 0.0f) {
  // Iterate backwards so we can remove entries without invalidating indices
  for (int32 I = Points.Num() - 1; I >= 0; --I) {
    FVector Loc = Points[I].GetTranslation();
    float SnapZ;
    if (Grid.SampleZ(Loc.X, Loc.Y, SnapZ)) {
      // Apply Z offset scaled by instance Z-scale (height)
      const float Scale = Points[I].GetScale3D().Z;
      Loc.Z = SnapZ + ZOffset * Scale;
      Points[I].SetTranslation(Loc);
    } else {
      // No mesh triangle found at this position (chunk edge / hole).
      // Remove the point entirely — the analytical Z won't match
      // the mesh and would cause floating trees.
      Points.RemoveAtSwap(I, EAllowShrinking::No);
    }
  }
}

static bool SampleHeightmapZ(const FFPMChunkHeightmapData &Data, float LocalX,
                             float LocalY, float &OutZ) {
  // Hex bounding box dimensions — heightmap covers this area
  constexpr float SizeX = FPMChunkConstants::HexOuterRadius * 2.0f;
  constexpr float SizeY = FPMChunkConstants::HexInnerRadius * 2.0f;
  constexpr int32 Res = FPMChunkConstants::ChunkResolution;
  // Normalized 0-1 min/max vertical range
  constexpr float WorldMinZ = FPMChunkConstants::MinWorldZ;
  constexpr float WorldHeightRange = FPMChunkConstants::WorldHeightRange;

  // 1. Normalize LocalXY to 0-1 UV space
  const float U = LocalX / SizeX;
  const float V = LocalY / SizeY;

  // 2. Map to grid indices (0..Res-1)
  const float GridX = U * (Res - 1);
  const float GridY = V * (Res - 1);

  // 3. Integer coordinates and fraction
  const int32 X0 = FMath::FloorToInt(GridX);
  const int32 Y0 = FMath::FloorToInt(GridY);
  const int32 X1 = FMath::Min(X0 + 1, Res - 1);
  const int32 Y1 = FMath::Min(Y0 + 1, Res - 1);

  // Bounds check (0..Res-1 is valid)
  if (X0 < 0 || X0 >= Res || Y0 < 0 || Y0 >= Res) {
    return false;
  }

  const float FracX = GridX - X0;
  const float FracY = GridY - Y0;

  // 4. Bilinear interpolation of normalized height (0.0 - 1.0)
  const float H00 = Data.HeightValues[Y0 * Res + X0];
  const float H10 = Data.HeightValues[Y0 * Res + X1];
  const float H01 = Data.HeightValues[Y1 * Res + X0];
  const float H11 = Data.HeightValues[Y1 * Res + X1];

  const float H0 = FMath::Lerp(H00, H10, FracX);
  const float H1 = FMath::Lerp(H01, H11, FracX);
  const float InterpH = FMath::Lerp(H0, H1, FracY);

  // 5. Convert to World Z
  OutZ = WorldMinZ + (InterpH * WorldHeightRange);
  return true;
}

static void SnapPointsToHeightmap(TArray<FTransform> &Points,
                                  const FFPMChunkHeightmapData &Data,
                                  float ZOffset = 0.0f) {
  for (int32 I = Points.Num() - 1; I >= 0; --I) {
    FVector Loc = Points[I].GetTranslation();
    float SnapZ;
    if (SampleHeightmapZ(Data, Loc.X, Loc.Y, SnapZ)) {
      // Apply Z offset scaled by instance Z-scale
      const float Scale = Points[I].GetScale3D().Z;
      Loc.Z = SnapZ + ZOffset * Scale;
      Points[I].SetTranslation(Loc);
    } else {
      Points.RemoveAtSwap(I, EAllowShrinking::No);
    }
  }
}

// =====================================================================
//  GenerateScatterPoints — uses voxel system for Z and biome queries
// =====================================================================

void FPMBiomePCGSpawner::GenerateScatterPoints(const FFPMChunkCoord &ChunkCoord,
                                               EFPMBiome TargetBiome,
                                               int32 Count, int32 ChunkSeed,
                                               int32 WorldSeed,
                                               TArray<FTransform> &OutPoints,
                                               FVector2D ScaleRange) {
  if (Count <= 0) {
    return;
  }

  OutPoints.Reserve(Count);

  // Hex bounding box dimensions for scatter area
  const float BBoxSizeX = FPMChunkConstants::HexOuterRadius * 2.0f;
  const float BBoxSizeY = FPMChunkConstants::HexInnerRadius * 2.0f;
  const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(ChunkCoord);
  FRandomStream RNG(ChunkSeed + static_cast<int32>(TargetBiome) * 7919);

  for (int32 i = 0; i < Count; ++i) {
    // Random position within hex bounding box (0-1 fractional)
    const float FracX = RNG.FRand();
    const float FracY = RNG.FRand();

    // Local XY within chunk (relative to chunk actor origin = bbox corner)
    const float LocalX = FracX * BBoxSizeX;
    const float LocalY = FracY * BBoxSizeY;

    // World XY for voxel generator queries
    const float WorldX = ChunkOrigin.X + LocalX;
    const float WorldY = ChunkOrigin.Y + LocalY;

    // Reject points outside the hexagonal chunk boundary.
    // LocalX/LocalY are relative to the bounding box origin,
    // convert to hex-center-relative for the hex test.
    const float HexRelX = LocalX - FPMChunkConstants::HexOuterRadius;
    const float HexRelY = LocalY - FPMChunkConstants::HexInnerRadius;
    if (!FPMChunkGenerator::IsInsideHex(HexRelX, HexRelY)) {
      continue;
    }

    // Get terrain surface Z from the analytical function.
    // This is used for biome/slope checks and as initial Z placement.
    // SnapPointsToMeshData() will correct Z to match the actual mesh.
    const float SurfaceZ =
        FPMVoxelGenerator::TerrainSurfaceZ(WorldX, WorldY, WorldSeed);

    // Reject points below sea level
    if (SurfaceZ < -50.0f) {
      continue;
    }

    // Reject points outside the voxel volume (no terrain there)
    if (SurfaceZ < FPMVoxelConstants::WorldZBase ||
        SurfaceZ > FPMVoxelConstants::WorldZTop) {
      continue;
    }

    // Normalized height for biome query
    const float NormH =
        (SurfaceZ - (-400.0f)) / 5000.0f; // matches voxel HeightBase/Scale

    // Check biome at this world position
    const EFPMBiome LocalBiome =
        FPMVoxelGenerator::BiomeAtWorldXY(WorldX, WorldY, WorldSeed, NormH);
    if (LocalBiome != TargetBiome) {
      continue; // Wrong biome — skip
    }

    // Slope rejection: approximate via neighboring terrain heights
    constexpr float SlopeDelta = 50.0f;
    const float ZLeft = FPMVoxelGenerator::TerrainSurfaceZ(WorldX - SlopeDelta,
                                                           WorldY, WorldSeed);
    const float ZRight = FPMVoxelGenerator::TerrainSurfaceZ(WorldX + SlopeDelta,
                                                            WorldY, WorldSeed);
    const float ZDown = FPMVoxelGenerator::TerrainSurfaceZ(
        WorldX, WorldY - SlopeDelta, WorldSeed);
    const float ZUp = FPMVoxelGenerator::TerrainSurfaceZ(
        WorldX, WorldY + SlopeDelta, WorldSeed);

    const float DZdx = (ZRight - ZLeft) / (2.0f * SlopeDelta);
    const float DZdy = (ZUp - ZDown) / (2.0f * SlopeDelta);
    FVector Normal(-DZdx, -DZdy, 1.0f);
    Normal.Normalize();
    const float SlopeDot = FVector::DotProduct(Normal, FVector::UpVector);

    // Trees: reject slopes > ~40 degrees (cos(40) ≈ 0.766)
    // Mountain rocks: allow steeper slopes
    if (TargetBiome != EFPMBiome::Mountain && SlopeDot < 0.77f) {
      continue; // Too steep for trees
    }

    // Random scale within range
    const float Scale = RNG.FRandRange(ScaleRange.X, ScaleRange.Y);

    // Random yaw rotation
    const float Yaw = RNG.FRandRange(0.0f, 360.0f);

    // LOCAL-SPACE transform: XY are local to chunk, Z is world
    // Since chunk actor is at (ChunkOrigin.X, ChunkOrigin.Y, 0),
    // local Z = SurfaceZ (world Z is absolute, actor Z is 0)
    FTransform T;
    T.SetLocation(FVector(LocalX, LocalY, SurfaceZ));
    T.SetRotation(FQuat(FRotator(0.0f, Yaw, 0.0f)));
    T.SetScale3D(FVector(Scale));

    OutPoints.Add(T);
  }
}

// =====================================================================
//  SpawnHISMInstances
// =====================================================================

UHierarchicalInstancedStaticMeshComponent *
FPMBiomePCGSpawner::SpawnHISMInstances(AActor *OwnerActor, UStaticMesh *Mesh,
                                       const TArray<FTransform> &Transforms,
                                       FName ComponentName) {
  if (!OwnerActor || !Mesh || Transforms.Num() == 0) {
    return nullptr;
  }

  UHierarchicalInstancedStaticMeshComponent *HISM =
      NewObject<UHierarchicalInstancedStaticMeshComponent>(OwnerActor,
                                                           ComponentName);

  HISM->SetStaticMesh(Mesh);
  HISM->SetMobility(EComponentMobility::Movable);
  HISM->SetCastShadow(false); // Shadows on instanced foliage are very expensive
  HISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  HISM->SetCollisionProfileName(TEXT("OverlapAll"));
  HISM->ComponentTags.Add(BiomeHISMTag);

  // Distance culling: fade out individual instances beyond 350m
  HISM->SetCullDistances(35000, 40000);
  HISM->bNeverDistanceCull = false;

  // Don't use trees as occluders (prevents rendering glitches)
  HISM->bUseAsOccluder = false;

  HISM->RegisterComponent();
  HISM->AttachToComponent(OwnerActor->GetRootComponent(),
                          FAttachmentTransformRules::KeepRelativeTransform);

  // Batch add all instances (LOCAL space — relative to component/actor)
  HISM->PreAllocateInstancesMemory(Transforms.Num());
  HISM->AddInstances(Transforms, /*bWorldSpace=*/false);

  // Force bounds update so frustum culling works correctly
  HISM->MarkRenderStateDirty();

  return HISM;
}

// =====================================================================
//  ClearSpawnedInstances
// =====================================================================

void FPMBiomePCGSpawner::ClearSpawnedInstances(AActor *OwnerActor) {
  if (!OwnerActor) {
    return;
  }

  TArray<UActorComponent *> Components;
  OwnerActor->GetComponents(Components);

  for (UActorComponent *Comp : Components) {
    if (Comp && Comp->ComponentHasTag(BiomeHISMTag)) {
      Comp->DestroyComponent();
    }
  }
}

// =====================================================================
//  PopulateChunk — Main entry point
// =====================================================================

void FPMBiomePCGSpawner::PopulateChunk(
    AActor *OwnerActor, const UFPMBiomePCGConfig *Config,
    const FFPMChunkCoord &ChunkCoord, int32 WorldSeed,
    const FFPMVoxelMeshData *MeshData,
    const FFPMChunkHeightmapData *HeightData) {
  if (!OwnerActor || !Config) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM PCG: PopulateChunk skipped — invalid params "
                "(Actor=%d, Config=%d)"),
           OwnerActor != nullptr, Config != nullptr);
    return;
  }

  // Clear any previous instances
  ClearSpawnedInstances(OwnerActor);

  // Deterministic per-chunk seed
  const int32 ChunkSeed = HashCombine(WorldSeed, GetTypeHash(ChunkCoord));

  // Build spatial grid ONCE for all biome types (biggest perf win).
  // Grid bins ~2000 triangles into 32x32 cells so each Z lookup
  // only tests ~5–10 triangles instead of all of them.
  FMeshHeightGrid HeightGrid;
  bool bHasGrid = false;
  if (MeshData && MeshData->Triangles.Num() >= 3) {
    HeightGrid.Build(*MeshData);
    bHasGrid = true;
  }

  int32 TotalSpawned = 0;

  // ---- Forest Trees (canopy + trunk) ----
  if (Config->ForestTreeDensity > 0 && Config->ForestTreeMeshes.Num() > 0) {
    TArray<FTransform> Points;
    GenerateScatterPoints(ChunkCoord, EFPMBiome::Forest,
                          Config->ForestTreeDensity, ChunkSeed, WorldSeed,
                          Points, Config->ForestTreeScaleRange);

    if (Points.Num() > 0) {
      if (HeightData) {
        SnapPointsToHeightmap(Points, *HeightData, Config->ForestTreeZOffset);
      } else if (bHasGrid) {
        SnapPointsToGrid(Points, HeightGrid, Config->ForestTreeZOffset);
      }

      // Distribute across available meshes
      const int32 MeshCount = Config->ForestTreeMeshes.Num();
      TArray<TArray<FTransform>> PerMeshPoints;
      PerMeshPoints.SetNum(MeshCount);

      FRandomStream MeshRNG(ChunkSeed + 111);
      for (const FTransform &P : Points) {
        const int32 MeshIdx = MeshRNG.RandRange(0, MeshCount - 1);
        PerMeshPoints[MeshIdx].Add(P);
      }

      for (int32 M = 0; M < MeshCount; ++M) {
        if (PerMeshPoints[M].Num() == 0) {
          continue;
        }

        // Spawn canopy (async loading: skip if mesh not loaded yet)
        UStaticMesh *CanopyMesh = Config->ForestTreeMeshes[M].LoadSynchronous();
        if (!CanopyMesh) {
          // Mesh not loaded yet, skip for now
          continue;
        }
        const FName CanopyName =
            *FString::Printf(TEXT("HISM_ForestCanopy_%d"), M);
        SpawnHISMInstances(OwnerActor, CanopyMesh, PerMeshPoints[M],
                           CanopyName);

        // Spawn trunk at same positions (paired with canopy)
        if (Config->ForestTrunkMeshes.Num() > 0) {
          const int32 TrunkIdx =
              FMath::Min(M, Config->ForestTrunkMeshes.Num() - 1);
          UStaticMesh *TrunkMesh =
              Config->ForestTrunkMeshes[TrunkIdx].LoadSynchronous();
          if (TrunkMesh) {
            const FName TrunkName =
                *FString::Printf(TEXT("HISM_ForestTrunk_%d"), M);
            SpawnHISMInstances(OwnerActor, TrunkMesh, PerMeshPoints[M],
                               TrunkName);
          }
        }

        TotalSpawned += PerMeshPoints[M].Num();
      }
    }
  }

  // ---- Meadow Trees (canopy + trunk) ----
  if (Config->MeadowTreeDensity > 0 && Config->MeadowTreeMeshes.Num() > 0) {
    TArray<FTransform> Points;
    GenerateScatterPoints(ChunkCoord, EFPMBiome::Meadows,
                          Config->MeadowTreeDensity, ChunkSeed, WorldSeed,
                          Points, FVector2D(0.7f, 1.2f));

    if (Points.Num() > 0) {
      if (HeightData) {
        SnapPointsToHeightmap(Points, *HeightData, Config->MeadowTreeZOffset);
      } else if (bHasGrid) {
        SnapPointsToGrid(Points, HeightGrid, Config->MeadowTreeZOffset);
      }

      // Spawn canopy (async loading: skip if mesh not loaded yet)
      UStaticMesh *CanopyMesh = Config->MeadowTreeMeshes[0].LoadSynchronous();
      if (!CanopyMesh) {
        // Mesh not loaded yet, skip meadow trees for this frame
        UE_LOG(LogTemp, Verbose,
               TEXT("FPM PCG: Skipping meadow trees — canopy mesh not loaded"));
      } else {
        SpawnHISMInstances(OwnerActor, CanopyMesh, Points,
                           TEXT("HISM_MeadowCanopy"));

        // Spawn trunk at same positions
        if (Config->MeadowTrunkMeshes.Num() > 0) {
          UStaticMesh *TrunkMesh =
              Config->MeadowTrunkMeshes[0].LoadSynchronous();
          if (TrunkMesh) {
            SpawnHISMInstances(OwnerActor, TrunkMesh, Points,
                               TEXT("HISM_MeadowTrunk"));
          }
        }

        TotalSpawned += Points.Num();
      }
    }
  }

  // ---- Mountain Rocks ----
  if (Config->MountainRockDensity > 0 && Config->MountainRockMeshes.Num() > 0) {
    TArray<FTransform> Points;
    GenerateScatterPoints(ChunkCoord, EFPMBiome::Mountain,
                          Config->MountainRockDensity, ChunkSeed, WorldSeed,
                          Points, Config->MountainRockScaleRange);

    if (Points.Num() > 0) {
      if (HeightData) {
        SnapPointsToHeightmap(Points, *HeightData, Config->MountainRockZOffset);
      } else if (bHasGrid) {
        SnapPointsToGrid(Points, HeightGrid, Config->MountainRockZOffset);
      }

      const int32 MeshCount = Config->MountainRockMeshes.Num();
      TArray<TArray<FTransform>> PerMeshPoints;
      PerMeshPoints.SetNum(MeshCount);

      FRandomStream MeshRNG(ChunkSeed + 333);
      for (const FTransform &P : Points) {
        const int32 MeshIdx = MeshRNG.RandRange(0, MeshCount - 1);
        PerMeshPoints[MeshIdx].Add(P);
      }

      for (int32 M = 0; M < MeshCount; ++M) {
        if (PerMeshPoints[M].Num() == 0) {
          continue;
        }

        // Async loading: skip if mesh not loaded yet
        UStaticMesh *Mesh = Config->MountainRockMeshes[M].LoadSynchronous();
        if (!Mesh) {
          continue;
        }

        const FName Name = *FString::Printf(TEXT("HISM_MtnRock_%d"), M);
        SpawnHISMInstances(OwnerActor, Mesh, PerMeshPoints[M], Name);
        TotalSpawned += PerMeshPoints[M].Num();
      }
    }
  }

  // ---- Scatter Rocks (all non-water biomes) ----
  if (Config->ScatterRockDensity > 0 && Config->ScatterRockMeshes.Num() > 0) {
    static const EFPMBiome ScatterBiomes[] = {
        EFPMBiome::Meadows, EFPMBiome::Forest, EFPMBiome::Mountain};

    for (EFPMBiome Biome : ScatterBiomes) {
      TArray<FTransform> Points;
      GenerateScatterPoints(ChunkCoord, Biome, Config->ScatterRockDensity,
                            ChunkSeed + 500, WorldSeed, Points,
                            FVector2D(0.3f, 1.0f));

      if (Points.Num() > 0) {
        if (HeightData) {
          SnapPointsToHeightmap(Points, *HeightData,
                                Config->ScatterRockZOffset);
        } else if (bHasGrid) {
          SnapPointsToGrid(Points, HeightGrid, Config->ScatterRockZOffset);
        }

        // Async loading: skip if mesh not loaded yet
        UStaticMesh *Mesh = Config->ScatterRockMeshes[0].LoadSynchronous();
        if (Mesh) {
          const FName Name = *FString::Printf(TEXT("HISM_ScatterRock_%d"),
                                              static_cast<int32>(Biome));
          SpawnHISMInstances(OwnerActor, Mesh, Points, Name);
          TotalSpawned += Points.Num();
        }
      }
    }
  }

  if (TotalSpawned > 0) {
    UE_LOG(LogTemp, Log, TEXT("FPM PCG: Chunk %s populated with %d instances"),
           *ChunkCoord.ToString(), TotalSpawned);
  }
}
