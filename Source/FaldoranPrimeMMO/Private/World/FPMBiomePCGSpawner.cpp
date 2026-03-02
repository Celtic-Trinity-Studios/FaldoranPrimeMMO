// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMBiomePCGSpawner.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "World/FPMNexusManager.h"
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
      FVector FaceNormal =
          FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
      if (FMath::Abs(FaceNormal.Z) < 0.3f) {
        continue;
      }

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

  bool SampleZ(float LocalX, float LocalY, float &OutZ) const {
    if (!Mesh || CellSize <= 0.0f)
      return false;

    const int32 CX =
        FMath::Clamp(static_cast<int32>(LocalX / CellSize), 0, GridRes - 1);
    const int32 CY =
        FMath::Clamp(static_cast<int32>(LocalY / CellSize), 0, GridRes - 1);

    const TArray<int32> &Tris = CellTriangles[CY][CX];
    float BestZ = 1e18f;
    bool bFound = false;

    for (int32 TriIdx : Tris) {
      const FVector &V0 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3]];
      const FVector &V1 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3 + 1]];
      const FVector &V2 = Mesh->Vertices[Mesh->Triangles[TriIdx * 3 + 2]];

      const float E1X = V1.X - V0.X, E1Y = V1.Y - V0.Y;
      const float E2X = V2.X - V0.X, E2Y = V2.Y - V0.Y;
      const float PX = LocalX - V0.X, PY = LocalY - V0.Y;

      const float D00 = E1X * E1X + E1Y * E1Y;
      const float D01 = E1X * E2X + E1Y * E2Y;
      const float D11 = E2X * E2X + E2Y * E2Y;
      const float D20 = PX * E1X + PY * E1Y;
      const float D21 = PX * E2X + PY * E2Y;

      const float Denom = D00 * D11 - D01 * D01;
      if (FMath::Abs(Denom) < SMALL_NUMBER)
        continue;

      const float U = (D11 * D20 - D01 * D21) / Denom;
      const float V = (D00 * D21 - D01 * D20) / Denom;
      const float W = 1.0f - U - V;

      if (U >= -0.01f && V >= -0.01f && W >= -0.01f) {
        const float InterpZ = W * V0.Z + U * V1.Z + V * V2.Z;
        if (InterpZ < BestZ) {
          BestZ = InterpZ;
          bFound = true;
        }
      }
    }

    if (bFound)
      OutZ = BestZ;
    return bFound;
  }
};

static void SnapPointsToGrid(TArray<FTransform> &Points,
                             const FMeshHeightGrid &Grid,
                             float ZOffset = 0.0f) {
  for (int32 I = Points.Num() - 1; I >= 0; --I) {
    FVector Loc = Points[I].GetTranslation();
    float SnapZ;
    if (Grid.SampleZ(Loc.X, Loc.Y, SnapZ)) {
      const float Scale = Points[I].GetScale3D().Z;
      Loc.Z = SnapZ + ZOffset * Scale;
      Points[I].SetTranslation(Loc);
    } else {
      Points.RemoveAtSwap(I, EAllowShrinking::No);
    }
  }
}

static bool SampleHeightmapZ(const FFPMChunkHeightmapData &Data, float LocalX,
                             float LocalY, float &OutZ) {
  constexpr float SizeX = FPMChunkConstants::ChunkWorldSize;
  constexpr float SizeY = FPMChunkConstants::ChunkWorldSize;
  constexpr int32 Res = FPMChunkConstants::ChunkResolution;
  constexpr float WorldMinZ = FPMChunkConstants::MinWorldZ;
  constexpr float WorldHeightRange = FPMChunkConstants::WorldHeightRange;

  const float U = LocalX / SizeX;
  const float V = LocalY / SizeY;
  const float GridX = U * (Res - 1);
  const float GridY = V * (Res - 1);

  const int32 X0 = FMath::FloorToInt(GridX);
  const int32 Y0 = FMath::FloorToInt(GridY);
  const int32 X1 = FMath::Min(X0 + 1, Res - 1);
  const int32 Y1 = FMath::Min(Y0 + 1, Res - 1);

  if (X0 < 0 || X0 >= Res || Y0 < 0 || Y0 >= Res)
    return false;

  const float FracX = GridX - X0;
  const float FracY = GridY - Y0;

  const float H00 = Data.HeightValues[Y0 * Res + X0];
  const float H10 = Data.HeightValues[Y0 * Res + X1];
  const float H01 = Data.HeightValues[Y1 * Res + X0];
  const float H11 = Data.HeightValues[Y1 * Res + X1];

  const float H0 = FMath::Lerp(H00, H10, FracX);
  const float H1 = FMath::Lerp(H01, H11, FracX);
  OutZ = WorldMinZ + (FMath::Lerp(H0, H1, FracY) * WorldHeightRange);
  return true;
}

static void SnapPointsToHeightmap(TArray<FTransform> &Points,
                                  const FFPMChunkHeightmapData &Data,
                                  float ZOffset = 0.0f) {
  for (int32 I = Points.Num() - 1; I >= 0; --I) {
    FVector Loc = Points[I].GetTranslation();
    float SnapZ;
    if (SampleHeightmapZ(Data, Loc.X, Loc.Y, SnapZ)) {
      const float Scale = Points[I].GetScale3D().Z;
      Loc.Z = SnapZ + ZOffset * Scale;
      Points[I].SetTranslation(Loc);
    } else {
      Points.RemoveAtSwap(I, EAllowShrinking::No);
    }
  }
}

// =====================================================================
//  GenerateScatterPoints
// =====================================================================

void FPMBiomePCGSpawner::GenerateScatterPoints(
    const FFPMChunkCoord &ChunkCoord, EFPMBiome TargetBiome, int32 Count,
    int32 ChunkSeed, int32 WorldSeed, TArray<FTransform> &OutPoints,
    FVector2D ScaleRange, float MaxSlopeDegrees) {
  if (Count <= 0)
    return;

  OutPoints.Reserve(Count);

  const float BBoxSizeX = FPMChunkConstants::ChunkWorldSize;
  const float BBoxSizeY = FPMChunkConstants::ChunkWorldSize;
  const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(ChunkCoord);
  FRandomStream RNG(ChunkSeed + static_cast<int32>(TargetBiome) * 7919);

  const float MinSlopeDot = FMath::Cos(
      FMath::DegreesToRadians(FMath::Clamp(MaxSlopeDegrees, 0.0f, 90.0f)));

  for (int32 i = 0; i < Count; ++i) {
    const float LocalX = RNG.FRand() * BBoxSizeX;
    const float LocalY = RNG.FRand() * BBoxSizeY;
    const float WorldX = ChunkOrigin.X + LocalX;
    const float WorldY = ChunkOrigin.Y + LocalY;

    const float SurfaceZ =
        FPMVoxelGenerator::TerrainSurfaceZ(WorldX, WorldY, WorldSeed);

    // Reject points below sea level / outside voxel world bounds
    if (SurfaceZ < -50.0f)
      continue;
    if (SurfaceZ < FPMVoxelConstants::WorldZBase ||
        SurfaceZ > FPMVoxelConstants::WorldZTop)
      continue;

    // Slope rejection via central-difference normal
    constexpr float SlopeDelta = 50.0f;
    const float ZLeft = FPMVoxelGenerator::TerrainSurfaceZ(WorldX - SlopeDelta,
                                                           WorldY, WorldSeed);
    const float ZRight = FPMVoxelGenerator::TerrainSurfaceZ(WorldX + SlopeDelta,
                                                            WorldY, WorldSeed);
    const float ZDown = FPMVoxelGenerator::TerrainSurfaceZ(
        WorldX, WorldY - SlopeDelta, WorldSeed);
    const float ZUp = FPMVoxelGenerator::TerrainSurfaceZ(
        WorldX, WorldY + SlopeDelta, WorldSeed);

    FVector Normal(-(ZRight - ZLeft) / (2.0f * SlopeDelta),
                   -(ZUp - ZDown) / (2.0f * SlopeDelta), 1.0f);
    Normal.Normalize();

    if (FVector::DotProduct(Normal, FVector::UpVector) < MinSlopeDot)
      continue;

    FTransform T;
    T.SetLocation(FVector(LocalX, LocalY, SurfaceZ));
    T.SetRotation(FQuat(FRotator(0.0f, RNG.FRandRange(0.0f, 360.0f), 0.0f)));
    T.SetScale3D(FVector(RNG.FRandRange(ScaleRange.X, ScaleRange.Y)));
    OutPoints.Add(T);
  }

  // -----------------------------------------------------------------------
  //  Nexus Safe Zone Filter
  //  Remove any scatter points that fall inside a Nexus radius.
  //  This ensures the empty city area has no trees, rocks, or groundcover.
  // -----------------------------------------------------------------------
  if (OutPoints.Num() > 0) {
    // Get the NexusManager from the world that owns the OwnerActor.
    // We use GWorld as a fallback for the stateless utility context.
    const UWorld *NexusWorld = GWorld;
    if (AFPMNexusManager *NexusMgr =
            AFPMNexusManager::GetOrCreate(const_cast<UWorld *>(NexusWorld))) {
      const FVector ChunkOriginVec =
          FPMChunkGenerator::ChunkToWorldOrigin(ChunkCoord);
      for (int32 I = OutPoints.Num() - 1; I >= 0; --I) {
        const FVector LocalPt = OutPoints[I].GetTranslation();
        const FVector WorldPt(ChunkOriginVec.X + LocalPt.X,
                              ChunkOriginVec.Y + LocalPt.Y, 0.f);
        if (NexusMgr->IsInNexusSafeZone(WorldPt)) {
          OutPoints.RemoveAtSwap(I, EAllowShrinking::No);
        }
      }
    }
  }
}

// =====================================================================
//  SpawnHISMInstances
// =====================================================================

UHierarchicalInstancedStaticMeshComponent *
FPMBiomePCGSpawner::SpawnHISMInstances(AActor *OwnerActor, UStaticMesh *Mesh,
                                       const TArray<FTransform> &Transforms,
                                       FName ComponentName, bool bIsRock,
                                       bool bCastShadow,
                                       FVector2D CullDistances) {
  if (!OwnerActor || !Mesh || Transforms.Num() == 0)
    return nullptr;

  UHierarchicalInstancedStaticMeshComponent *HISM =
      NewObject<UHierarchicalInstancedStaticMeshComponent>(OwnerActor,
                                                           ComponentName);
  HISM->SetStaticMesh(Mesh);
  // Movable mobility required — the ChunkActor's root (TerrainMesh) is
  // Movable, and UE refuses to attach a Static component to a Movable parent.
  // Foliage still batches correctly with Movable; the Static path just adds
  // light-map baking which we don't need at runtime.
  HISM->SetMobility(EComponentMobility::Movable);
  HISM->SetCastShadow(bCastShadow);

  if (bIsRock) {
    // Solid objects (rocks, trunks) — block all channels.
    // Set CTF_UseComplexAsSimple on the mesh's BodySetup so UE uses the
    // actual rendered triangles for collision instead of the auto-generated
    // convex hull / bounding box.  This is the correct way to get tight
    // collision on irregular shapes like tree trunks.
    HISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    HISM->SetCollisionProfileName(TEXT("BlockAll"));
    if (Mesh && Mesh->GetBodySetup()) {
      Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
    }
  } else {
    HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  }

  HISM->ComponentTags.Add(BiomeHISMTag);
  HISM->SetCullDistances(static_cast<int32>(CullDistances.X),
                         static_cast<int32>(CullDistances.Y));
  HISM->bNeverDistanceCull = false;
  HISM->bUseAsOccluder = false;

  HISM->RegisterComponent();
  HISM->AttachToComponent(OwnerActor->GetRootComponent(),
                          FAttachmentTransformRules::KeepRelativeTransform);

  HISM->PreAllocateInstancesMemory(Transforms.Num());
  HISM->AddInstances(Transforms, /*bWorldSpace=*/false);
  HISM->MarkRenderStateDirty();

  UE_LOG(LogTemp, Verbose, TEXT("FPM PCG: HISM '%s' — %d instances (rock=%d)"),
         *ComponentName.ToString(), Transforms.Num(), bIsRock ? 1 : 0);

  return HISM;
}

// =====================================================================
//  ClearSpawnedInstances
// =====================================================================

void FPMBiomePCGSpawner::ClearSpawnedInstances(AActor *OwnerActor) {
  if (!OwnerActor)
    return;

  TArray<UActorComponent *> Components;
  OwnerActor->GetComponents(Components);
  for (UActorComponent *Comp : Components) {
    if (Comp && Comp->ComponentHasTag(BiomeHISMTag))
      Comp->DestroyComponent();
  }
}

// =====================================================================
//  PopulateChunk — generic biome loop
// =====================================================================

/**
 * Helper: pick a random mesh index from a weighted layer array.
 * Returns -1 if the array is empty.
 */
template <typename TLayer>
static int32 PickWeightedMesh(const TArray<TLayer> &Layers,
                              FRandomStream &RNG) {
  if (Layers.Num() == 0)
    return -1;
  if (Layers.Num() == 1)
    return 0;

  float TotalWeight = 0.0f;
  for (const TLayer &L : Layers)
    TotalWeight += L.Weight;

  float Roll = RNG.FRandRange(0.0f, TotalWeight);
  for (int32 I = 0; I < Layers.Num(); ++I) {
    Roll -= Layers[I].Weight;
    if (Roll <= 0.0f)
      return I;
  }
  return Layers.Num() - 1;
}

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

  ClearSpawnedInstances(OwnerActor);

  const int32 ChunkSeed = HashCombine(WorldSeed, GetTypeHash(ChunkCoord));
  const FVector2D CullDist = Config->CullDistances;

  // Build spatial height grid once — shared across all biome passes.
  FMeshHeightGrid HeightGrid;
  bool bHasGrid = false;
  if (MeshData && MeshData->Triangles.Num() >= 3) {
    HeightGrid.Build(*MeshData);
    bHasGrid = true;
  }

  int32 TotalSpawned = 0;

  // -------------------------------------------------------------------
  // DIAGNOSTIC — always logs at Warning so it's easy to find in the
  // Output Log. Remove once spawning is confirmed working.
  // -------------------------------------------------------------------
  UE_LOG(LogTemp, Warning,
         TEXT("FPM PCG: PopulateChunk %s — HasMesh=%d, HasHeightmap=%d, "
              "Overrides=%d, Exclusions=%d"),
         *ChunkCoord.ToString(), bHasGrid ? 1 : 0,
         HeightData != nullptr ? 1 : 0, Config->BiomeParams.Num(),
         Config->BiomeExclusions.Num());

  // Iterate all 17 biomes: use per-biome override if present, else
  // DefaultBiomeParams, skip entirely if in BiomeExclusions or bEnabled=false.
  static const EFPMBiome AllBiomes[] = {
      EFPMBiome::Meadows, EFPMBiome::Forest,       EFPMBiome::Plains,
      EFPMBiome::Savanna, EFPMBiome::Jungle,       EFPMBiome::Desert,
      EFPMBiome::Taiga,   EFPMBiome::BorealForest, EFPMBiome::Tundra,
      EFPMBiome::Swamp,   EFPMBiome::Alpine,       EFPMBiome::Mountain,
      EFPMBiome::Snow,    EFPMBiome::River,        EFPMBiome::Coast,
      EFPMBiome::Beach,   EFPMBiome::Ocean,
  };

  // -------------------------------------------------------------------

  // ====================================================================
  //  Global Default Pass — runs DefaultBiomeParams exactly ONCE.
  //  Spawns trees/rocks/groundcover across all land (sea-level filter
  //  handles water). BiomeExclusions are not applied here because this
  //  is a global land-wide pass; use density=0 or bEnabled=false on
  //  DefaultBiomeParams to disable it entirely.
  // ====================================================================
  {
    const FFPMBiomeSpawnParams &P = Config->DefaultBiomeParams;
    if (P.bEnabled) {
      // Trees
      if (P.TreeDensity > 0 && P.Trees.Num() > 0) {
        TArray<FTransform> Points;
        GenerateScatterPoints(ChunkCoord, EFPMBiome::Meadows, P.TreeDensity,
                              ChunkSeed, WorldSeed, Points, P.TreeScaleRange,
                              P.TreeMaxSlopeDegrees);
        if (Points.Num() > 0) {
          if (HeightData)
            SnapPointsToHeightmap(Points, *HeightData, P.TreeZOffset);
          else if (bHasGrid)
            SnapPointsToGrid(Points, HeightGrid, P.TreeZOffset);

          FRandomStream MeshRNG(ChunkSeed + 11317);
          TArray<TArray<FTransform>> PerVariant;
          PerVariant.SetNum(P.Trees.Num());
          for (const FTransform &T : Points) {
            const int32 Idx = PickWeightedMesh(P.Trees, MeshRNG);
            if (Idx >= 0)
              PerVariant[Idx].Add(T);
          }
          for (int32 VI = 0; VI < P.Trees.Num(); ++VI) {
            if (!PerVariant[VI].Num())
              continue;
            UStaticMesh *Canopy = P.Trees[VI].CanopyMesh.LoadSynchronous();
            if (!Canopy)
              continue;
            const FName CName =
                *FString::Printf(TEXT("HISM_DefTree_%d_%d"), VI, ChunkSeed);
            SpawnHISMInstances(OwnerActor, Canopy, PerVariant[VI], CName,
                               /*bIsRock=*/false, Config->bTreeCastsShadow,
                               CullDist);
            if (!P.Trees[VI].TrunkMesh.IsNull()) {
              UStaticMesh *Trunk = P.Trees[VI].TrunkMesh.LoadSynchronous();
              if (Trunk) {
                const FName TName = *FString::Printf(
                    TEXT("HISM_DefTrunk_%d_%d"), VI, ChunkSeed);
                SpawnHISMInstances(OwnerActor, Trunk, PerVariant[VI], TName,
                                   P.bTreeCollision, Config->bTreeCastsShadow,
                                   CullDist);
              }
            }
            TotalSpawned += PerVariant[VI].Num();
          }
        }
      }

      // Rocks
      if (P.RockDensity > 0 && P.Rocks.Num() > 0) {
        TArray<FTransform> Points;
        GenerateScatterPoints(ChunkCoord, EFPMBiome::Meadows, P.RockDensity,
                              ChunkSeed + 500, WorldSeed, Points,
                              P.RockScaleRange, P.RockMaxSlopeDegrees);
        if (Points.Num() > 0) {
          if (HeightData)
            SnapPointsToHeightmap(Points, *HeightData, P.RockZOffset);
          else if (bHasGrid)
            SnapPointsToGrid(Points, HeightGrid, P.RockZOffset);

          FRandomStream RockRNG(ChunkSeed + 22573);
          TArray<TArray<FTransform>> PerVariant;
          PerVariant.SetNum(P.Rocks.Num());
          for (const FTransform &T : Points) {
            const int32 Idx = PickWeightedMesh(P.Rocks, RockRNG);
            if (Idx >= 0)
              PerVariant[Idx].Add(T);
          }
          for (int32 VI = 0; VI < P.Rocks.Num(); ++VI) {
            if (!PerVariant[VI].Num())
              continue;
            UStaticMesh *Mesh = P.Rocks[VI].RockMesh.LoadSynchronous();
            if (!Mesh)
              continue;
            const FName Name =
                *FString::Printf(TEXT("HISM_DefRock_%d_%d"), VI, ChunkSeed);
            SpawnHISMInstances(OwnerActor, Mesh, PerVariant[VI], Name,
                               /*bIsRock=*/true, /*bCastShadow=*/false,
                               CullDist);
            TotalSpawned += PerVariant[VI].Num();
          }
        }
      }

      // Ground Cover
      if (P.GroundCoverDensity > 0 && P.GroundCover.Num() > 0) {
        TArray<FTransform> Points;
        GenerateScatterPoints(ChunkCoord, EFPMBiome::Meadows,
                              P.GroundCoverDensity, ChunkSeed + 1000, WorldSeed,
                              Points, P.GroundCoverScaleRange,
                              P.GroundCoverMaxSlopeDegrees);
        if (Points.Num() > 0) {
          if (HeightData)
            SnapPointsToHeightmap(Points, *HeightData, P.GroundCoverZOffset);
          else if (bHasGrid)
            SnapPointsToGrid(Points, HeightGrid, P.GroundCoverZOffset);

          FRandomStream GCovRNG(ChunkSeed + 33791);
          TArray<TArray<FTransform>> PerVariant;
          PerVariant.SetNum(P.GroundCover.Num());
          for (const FTransform &T : Points) {
            const int32 Idx = PickWeightedMesh(P.GroundCover, GCovRNG);
            if (Idx >= 0)
              PerVariant[Idx].Add(T);
          }
          for (int32 VI = 0; VI < P.GroundCover.Num(); ++VI) {
            if (!PerVariant[VI].Num())
              continue;
            UStaticMesh *Mesh = P.GroundCover[VI].Mesh.LoadSynchronous();
            if (!Mesh)
              continue;
            const FName Name =
                *FString::Printf(TEXT("HISM_DefGC_%d_%d"), VI, ChunkSeed);
            SpawnHISMInstances(OwnerActor, Mesh, PerVariant[VI], Name,
                               /*bIsRock=*/false,
                               Config->bGroundCoverCastsShadow, CullDist);
            TotalSpawned += PerVariant[VI].Num();
          }
        }
      }
    }
  }

  // ====================================================================
  //  Per-Biome Override Passes — each BiomeParams entry runs once,
  //  with a biome-specific seed so positions differ from the default
  //  pass and from each other. These override (ADD TO) the default.
  //  To REPLACE the default for a biome, set DefaultBiomeParams.bEnabled
  //  = false and use only BiomeParams entries.
  // ====================================================================
  for (const auto &KV : Config->BiomeParams) {
    const EFPMBiome Biome = KV.Key;
    const FFPMBiomeSpawnParams &P = KV.Value;

    if (Config->BiomeExclusions.Contains(Biome))
      continue;
    if (!P.bEnabled)
      continue;

    // ---- Trees ------------------------------------------------
    if (P.TreeDensity > 0 && P.Trees.Num() > 0) {
      TArray<FTransform> Points;
      GenerateScatterPoints(ChunkCoord, Biome, P.TreeDensity, ChunkSeed,
                            WorldSeed, Points, P.TreeScaleRange,
                            P.TreeMaxSlopeDegrees);

      if (Points.Num() > 0) {
        if (HeightData)
          SnapPointsToHeightmap(Points, *HeightData, P.TreeZOffset);
        else if (bHasGrid)
          SnapPointsToGrid(Points, HeightGrid, P.TreeZOffset);

        // Weighted mesh selection per instance
        FRandomStream MeshRNG(ChunkSeed + static_cast<int32>(Biome) * 113);
        int32 MeshSpawnCount = 0;

        // Build per-variant point lists
        TArray<TArray<FTransform>> PerVariant;
        PerVariant.SetNum(P.Trees.Num());
        for (const FTransform &T : Points) {
          const int32 Idx = PickWeightedMesh(P.Trees, MeshRNG);
          if (Idx >= 0)
            PerVariant[Idx].Add(T);
        }

        for (int32 VI = 0; VI < P.Trees.Num(); ++VI) {
          if (PerVariant[VI].Num() == 0)
            continue;

          UStaticMesh *Canopy = P.Trees[VI].CanopyMesh.LoadSynchronous();
          if (!Canopy)
            continue;

          const FName CanopyName =
              *FString::Printf(TEXT("HISM_Tree_%d_%d_%d"),
                               static_cast<int32>(Biome), VI, ChunkSeed);
          // Canopy/foliage: never solid — player should walk under tree crowns
          SpawnHISMInstances(OwnerActor, Canopy, PerVariant[VI], CanopyName,
                             /*bIsRock=*/false, Config->bTreeCastsShadow,
                             CullDist);

          if (!P.Trees[VI].TrunkMesh.IsNull()) {
            UStaticMesh *Trunk = P.Trees[VI].TrunkMesh.LoadSynchronous();
            if (Trunk) {
              const FName TrunkName =
                  *FString::Printf(TEXT("HISM_Trunk_%d_%d_%d"),
                                   static_cast<int32>(Biome), VI, ChunkSeed);
              // Trunk: solid — block the player (bIsRock = bTreeCollision)
              SpawnHISMInstances(OwnerActor, Trunk, PerVariant[VI], TrunkName,
                                 P.bTreeCollision, Config->bTreeCastsShadow,
                                 CullDist);
            }
          }

          MeshSpawnCount += PerVariant[VI].Num();
        }

        TotalSpawned += MeshSpawnCount;
      }
    }

    // ---- Rocks ------------------------------------------------
    if (P.RockDensity > 0 && P.Rocks.Num() > 0) {
      TArray<FTransform> Points;
      GenerateScatterPoints(ChunkCoord, Biome, P.RockDensity, ChunkSeed + 500,
                            WorldSeed, Points, P.RockScaleRange,
                            P.RockMaxSlopeDegrees);

      if (Points.Num() > 0) {
        if (HeightData)
          SnapPointsToHeightmap(Points, *HeightData, P.RockZOffset);
        else if (bHasGrid)
          SnapPointsToGrid(Points, HeightGrid, P.RockZOffset);

        FRandomStream RockRNG(ChunkSeed + static_cast<int32>(Biome) * 337);
        TArray<TArray<FTransform>> PerVariant;
        PerVariant.SetNum(P.Rocks.Num());
        for (const FTransform &T : Points) {
          const int32 Idx = PickWeightedMesh(P.Rocks, RockRNG);
          if (Idx >= 0)
            PerVariant[Idx].Add(T);
        }

        for (int32 VI = 0; VI < P.Rocks.Num(); ++VI) {
          if (PerVariant[VI].Num() == 0)
            continue;

          UStaticMesh *Mesh = P.Rocks[VI].RockMesh.LoadSynchronous();
          if (!Mesh)
            continue;

          const FName Name =
              *FString::Printf(TEXT("HISM_Rock_%d_%d_%d"),
                               static_cast<int32>(Biome), VI, ChunkSeed);
          // Rocks always get QueryOnly collision
          SpawnHISMInstances(OwnerActor, Mesh, PerVariant[VI], Name,
                             /*bIsRock=*/true, /*bCastShadow=*/false, CullDist);
          TotalSpawned += PerVariant[VI].Num();
        }
      }
    }

    // ---- Ground Cover -----------------------------------------
    if (P.GroundCoverDensity > 0 && P.GroundCover.Num() > 0) {
      TArray<FTransform> Points;
      GenerateScatterPoints(
          ChunkCoord, Biome, P.GroundCoverDensity, ChunkSeed + 1000, WorldSeed,
          Points, P.GroundCoverScaleRange, P.GroundCoverMaxSlopeDegrees);

      if (Points.Num() > 0) {
        if (HeightData)
          SnapPointsToHeightmap(Points, *HeightData, P.GroundCoverZOffset);
        else if (bHasGrid)
          SnapPointsToGrid(Points, HeightGrid, P.GroundCoverZOffset);

        FRandomStream GCovRNG(ChunkSeed + static_cast<int32>(Biome) * 991);
        TArray<TArray<FTransform>> PerVariant;
        PerVariant.SetNum(P.GroundCover.Num());
        for (const FTransform &T : Points) {
          const int32 Idx = PickWeightedMesh(P.GroundCover, GCovRNG);
          if (Idx >= 0)
            PerVariant[Idx].Add(T);
        }

        for (int32 VI = 0; VI < P.GroundCover.Num(); ++VI) {
          if (PerVariant[VI].Num() == 0)
            continue;

          UStaticMesh *Mesh = P.GroundCover[VI].Mesh.LoadSynchronous();
          if (!Mesh)
            continue;

          const FName Name =
              *FString::Printf(TEXT("HISM_GC_%d_%d_%d"),
                               static_cast<int32>(Biome), VI, ChunkSeed);
          // Ground cover: no collision, configurable shadow
          SpawnHISMInstances(OwnerActor, Mesh, PerVariant[VI], Name,
                             /*bIsRock=*/false, Config->bGroundCoverCastsShadow,
                             CullDist);
          TotalSpawned += PerVariant[VI].Num();
        }
      }
    }
  }

  if (TotalSpawned > 0) {
    UE_LOG(LogTemp, Log,
           TEXT("FPM PCG: Chunk %s — %d instances across %d biomes"),
           *ChunkCoord.ToString(), TotalSpawned, Config->BiomeParams.Num());
  }
}
