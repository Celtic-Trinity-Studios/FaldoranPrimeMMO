// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMWorldChunkManager.h"
#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/FPMNoise.h"
#include "World/FPMPlanetTraversal.h"
#include "World/FPMVoxelChunk.h"
#include "World/FPMWaterMeshBuilder.h"
#include "World/FPMWaterSimulation.h"
#include "World/FPMWaterSource.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ===================================================================
//  Async Chunk Generation (file-scope state)
// ===================================================================

/** Pending async generation task result. */
struct FPendingChunk {
  FFPMChunkCoord Coord;
  TFuture<FFPMVoxelMeshData> Future;
};

/** Active async generation tasks for this manager. */
static TArray<TSharedPtr<FPendingChunk>> GPendingGenerations;

/** Coords currently dispatched to worker threads. */
static TSet<FFPMChunkCoord> GInFlightCoords;

/** Maximum concurrent async generation tasks.
 *  Scales to CPU core count minus 2 (game + render threads).
 *  Falls back to a minimum of 4 on low-core machines.
 */
static int32 GMaxConcurrentGenerations = 0; // computed at first use

/** Maximum chunks to finalize (spawn actor + build PMC) per tick.
 *  Each finalization is ~1-2ms of game thread time, so 6 keeps us
 *  under a 10ms budget while still draining the queue fast. */
static constexpr int32 GMaxFinalizationsPerTick = 3;

static int32 GetMaxConcurrentGenerations() {
  if (GMaxConcurrentGenerations == 0) {
    const int32 Cores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
    GMaxConcurrentGenerations = FMath::Max(4, Cores - 2);
    UE_LOG(LogTemp, Log,
           TEXT("FPM: Auto-tuned async concurrency to %d workers (%d HW "
                "threads detected)"),
           GMaxConcurrentGenerations, Cores);
  }
  return GMaxConcurrentGenerations;
}

/** Timer for periodic gap-recovery scans. */
static float GGapRecoveryTimer = 0.0f;
static constexpr float GGapRecoveryInterval = 2.0f;

static bool IsTileWithinNeighborhood(const FIntVector &TileCoord,
                                     const FIntVector &CenterTile) {
  return FMath::Abs(TileCoord.X - CenterTile.X) <= 1 &&
         FMath::Abs(TileCoord.Y - CenterTile.Y) <= 1 &&
         FMath::Abs(TileCoord.Z - CenterTile.Z) <= 1;
}

// ===================================================================
//  Console Commands
// ===================================================================

// Pointer to the active manager instance for console commands and terraforming.
// NON-static: externally referenced by FPMVoxelChunk.cpp for voxel overlay
// lookups.
AFPMWorldChunkManager *GActiveChunkManager = nullptr;

FAutoConsoleCommand AFPMWorldChunkManager::CmdGenerateWorld(
    TEXT("FPM.GenerateWorld"),
    TEXT("Initialize the chunk-based world. Usage: FPM.GenerateWorld [seed]"),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          if (GActiveChunkManager) {
            if (Args.Num() > 0) {
              GActiveChunkManager->WorldSeed = FCString::Atoi(*Args[0]);
            }
            GActiveChunkManager->ForceChunkUpdate();
            UE_LOG(LogTemp, Warning,
                   TEXT("FPM: World generation triggered (seed=%d)"),
                   GActiveChunkManager->WorldSeed);
          } else {
            UE_LOG(LogTemp, Error,
                   TEXT("FPM: No active chunk manager! Place an "
                        "AFPMWorldChunkManager in the level."));
          }
        }));

FAutoConsoleCommand AFPMWorldChunkManager::CmdRegenChunks(
    TEXT("FPM.RegenChunks"),
    TEXT("Regenerate all loaded chunks with the current seed."),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          if (GActiveChunkManager) {
            GActiveChunkManager->RegenerateAllChunks();
          } else {
            UE_LOG(LogTemp, Error, TEXT("FPM: No active chunk manager!"));
          }
        }));

// ===================================================================
//  Constructor
// ===================================================================

AFPMWorldChunkManager::AFPMWorldChunkManager() {
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickInterval =
      0.0f; // Tick every frame, but we throttle internally
  bReplicates = false;
  bAlwaysRelevant = false;
}

void AFPMWorldChunkManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(AFPMWorldChunkManager, WorldSeed);
}

AFPMWorldChunkManager *AFPMWorldChunkManager::GetOrCreate(UWorld *World) {
  if (!World)
    return nullptr;

  // 1. Return the existing global instance if it's still valid
  //    This prevents duplicate WCMs on listen servers where
  //    GameMode::InitGame and ClientEnterWorldSuccess both call GetOrCreate.
  if (GActiveChunkManager && IsValid(GActiveChunkManager) &&
      GActiveChunkManager->GetWorld() == World) {
    return GActiveChunkManager;
  }

  // 2. Fall back to searching the world (e.g. after a level transition)
  for (TActorIterator<AFPMWorldChunkManager> It(World); It; ++It) {
    GActiveChunkManager = *It;
    return *It;
  }

  // 3. Spawn a new one  only reachable if none exists yet
  FActorSpawnParameters Params;
  Params.Name = FName(TEXT("FPMWorldChunkManager_0"));
  AFPMWorldChunkManager *WCM = World->SpawnActor<AFPMWorldChunkManager>(
      AFPMWorldChunkManager::StaticClass(), FVector::ZeroVector,
      FRotator::ZeroRotator, Params);

  if (WCM) {
    // Load assets from known content paths
    UMaterialInterface *Mat = Cast<UMaterialInterface>(
        StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                         TEXT("/Game/Materials/Landscape/M_ChunkTerrain")));
    if (Mat)
      WCM->TerrainMaterial = Mat;

    UFPMBiomePCGConfig *PCGConf = Cast<UFPMBiomePCGConfig>(
        StaticLoadObject(UFPMBiomePCGConfig::StaticClass(), nullptr,
                         TEXT("/Game/DA_BiomePCGConfig")));
    if (PCGConf)
      WCM->BiomePCGConfig = PCGConf;

    UE_LOG(LogTemp, Warning,
           TEXT("FPM: Spawned WorldChunkManager from code (Mat=%s, PCG=%s)"),
           Mat ? *Mat->GetName() : TEXT("NULL"),
           PCGConf ? *PCGConf->GetName() : TEXT("NULL"));
  }

  return WCM;
}

// ===================================================================
//  BeginPlay
// ===================================================================

void AFPMWorldChunkManager::BeginPlay() {
  Super::BeginPlay();

  // ---------------------------------------------------------------
  //  SINGLETON GUARD: Only one WorldChunkManager per UWorld.
  //  On a listen server, GameMode::InitGame spawns one, then
  //  ClientEnterWorldSuccess may call GetOrCreate again. If a
  //  second WCM somehow got spawned, self-destruct immediately.
  // ---------------------------------------------------------------
  if (GActiveChunkManager && GActiveChunkManager != this &&
      IsValid(GActiveChunkManager) &&
      GActiveChunkManager->GetWorld() == GetWorld()) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM: Duplicate WorldChunkManager detected  destroying self. "
                "Active WCM: %s"),
           *GActiveChunkManager->GetName());
    Destroy();
    return;
  }

  GActiveChunkManager = this;

  // =================================================================
  //  Load settings from Config/WorldGen.ini
  //  Edit that file and restart PIE to iterate  no recompile needed.
  // =================================================================
  {
    FString IniPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("WorldGen.ini")));
    FConfigCacheIni::NormalizeConfigIniPath(IniPath);

    if (FPaths::FileExists(IniPath)) {
      UE_LOG(LogTemp, Warning, TEXT("FPM: Loading settings from %s"), *IniPath);

      int32 IniSeed = 0;
      if (GConfig->GetInt(TEXT("WorldGen"), TEXT("Seed"), IniSeed, IniPath)) {
        if (IniSeed == 0) {
          WorldSeed = FMath::RandRange(1, 999999);
          UE_LOG(LogTemp, Warning,
                 TEXT("FPM: Seed=0 in INI ? randomized to %d"), WorldSeed);
        } else {
          WorldSeed = IniSeed;
        }
      }

      float IniWaterZ = WaterZHeight;
      if (GConfig->GetFloat(TEXT("WorldGen"), TEXT("WaterZHeight"), IniWaterZ,
                            IniPath)) {
        WaterZHeight = IniWaterZ;
      }

      int32 IniChunks = MaxChunksPerFrame;
      if (GConfig->GetInt(TEXT("WorldGen"), TEXT("MaxChunksPerFrame"),
                          IniChunks, IniPath)) {
        MaxChunksPerFrame = IniChunks;
      }

      float IniUpdate = UpdateInterval;
      if (GConfig->GetFloat(TEXT("WorldGen"), TEXT("UpdateInterval"), IniUpdate,
                            IniPath)) {
        UpdateInterval = IniUpdate;
      }

      bool IniDebug = bDrawDebugChunkBounds;
      if (GConfig->GetBool(TEXT("WorldGen"), TEXT("bDrawDebugChunks"), IniDebug,
                           IniPath)) {
        bDrawDebugChunkBounds = IniDebug;
      }

      // View distance rings (INI-tuneable without recompile)
      int32 IniFullRange = FPMChunkConstants::FullDetailRange;
      if (GConfig->GetInt(TEXT("WorldGen"), TEXT("FullDetailRange"),
                          IniFullRange, IniPath)) {
        FPMChunkConstants::FullDetailRange = IniFullRange;
      }
      int32 IniMedRange = FPMChunkConstants::MediumDetailRange;
      if (GConfig->GetInt(TEXT("WorldGen"), TEXT("MediumDetailRange"),
                          IniMedRange, IniPath)) {
        FPMChunkConstants::MediumDetailRange = IniMedRange;
      }
      int32 IniLowRange = FPMChunkConstants::LowDetailRange;
      if (GConfig->GetInt(TEXT("WorldGen"), TEXT("LowDetailRange"), IniLowRange,
                          IniPath)) {
        FPMChunkConstants::LowDetailRange = IniLowRange;
      }

      // Island shape tuning
      float IniRadius = FPMChunkConstants::IslandRadiusFraction;
      if (GConfig->GetFloat(TEXT("Terrain"), TEXT("IslandRadiusFraction"),
                            IniRadius, IniPath)) {
        FPMChunkConstants::IslandRadiusFraction = IniRadius;
      }
    } else {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: No WorldGen.ini found, using defaults"));

      // Randomize seed if requested (original behavior)
      if (bRandomizeSeed && HasAuthority()) {
        WorldSeed = FMath::RandRange(1, 999999);
      }
    }
  }

  // Initialize overlay system
  const FString SaveDir =
      FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChunkOverlays"));
  FPMChunkOverlayManager::Initialize(SaveDir);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: ========================================"));
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Toroidal planet world system initialized"));
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Seed=%d, ChunkSize=%.0fm, Planet=%d chunks/axis "
              "(%.0fkm circumference)"),
         WorldSeed, FPMChunkConstants::ChunkWorldSize / 100.0f,
         FPMChunkConstants::PlanetChunksPerAxis,
         FPMChunkConstants::PlanetCircumferenceKm);
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: LOD ranges: Full=%d, Medium=%d, Low=%d rings"),
         FPMChunkConstants::FullDetailRange,
         FPMChunkConstants::MediumDetailRange,
         FPMChunkConstants::LowDetailRange);
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: ========================================"));

  // Diagnostic: verify editor-configured assets
  UE_LOG(LogTemp, Warning, TEXT("FPM: BiomePCGConfig=%s, TerrainMaterial=%s"),
         BiomePCGConfig ? *BiomePCGConfig->GetName() : TEXT("NULL"),
         TerrainMaterial ? *TerrainMaterial->GetName() : TEXT("NULL"));

  // Spawn the water plane at sea level
  SpawnWaterPlane();

  // Generate and spawn water sources at river headwaters
  SpawnRiverHeadSources();

  // --- Flowing Water System Initialization ---
  {
    // Reconstruct INI path (the earlier IniPath variable was in a different
    // scope)
    FString WaterIniPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("WorldGen.ini")));
    FConfigCacheIni::NormalizeConfigIniPath(WaterIniPath);

    // Load water simulation settings from INI
    FPMWaterSimulation::LoadSettingsFromINI(WaterIniPath);

    // Load bEnableFlowingWater from INI
    bool IniFlowingWater = bEnableFlowingWater;
    if (GConfig->GetBool(TEXT("WaterSimulation"), TEXT("bEnableFlowingWater"),
                         IniFlowingWater, WaterIniPath)) {
      bEnableFlowingWater = IniFlowingWater;
    }

    if (bEnableFlowingWater) {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Flowing water system ENABLED  SimRate=%.0fHz, "
                  "Evap=%.4f, MaxHops=%d"),
             FPMWaterConstants::SimulationRate,
             FPMWaterConstants::EvaporationRate,
             FPMWaterConstants::MaxFlowHops);
    } else {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Flowing water DISABLED  using flat water plane only"));
    }
  }

  // Trigger initial chunk load on next tick
  TimeSinceLastUpdate = UpdateInterval;
}

void AFPMWorldChunkManager::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  if (GActiveChunkManager == this) {
    GActiveChunkManager = nullptr;
  }
  Super::EndPlay(EndPlayReason);
}

// ===================================================================
//  Tick
// ===================================================================

void AFPMWorldChunkManager::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // Don't run chunk loading until a player pawn actually exists
  // (avoids burning CPU during login / character creation screens).
  APlayerController *PC =
      GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
  if (!PC || !PC->GetPawn()) {
    return;
  }

  // --- TOROIDAL WRAP ---
  // Only apply wrap during explicit high-speed planet traversal.
  // Ground movement near origin must be allowed to go slightly negative
  // without snapping to the far side of the world.
  {
    APawn *Pawn = PC->GetPawn();
    if (Pawn) {
      if (const UFPMPlanetTraversal *Traversal =
              Pawn->FindComponentByClass<UFPMPlanetTraversal>()) {
        if (Traversal->IsRiftRunnerActive()) {
          FVector Pos = Pawn->GetActorLocation();
          const float Circ = FPMChunkConstants::PlanetCircumferenceCm;
          bool bNeedsWrap = false;
          if (Pos.X < 0.0f || Pos.X >= Circ) {
            Pos.X = FPMChunkConstants::WrapWorldCoord(Pos.X);
            bNeedsWrap = true;
          }
          if (Pos.Y < 0.0f || Pos.Y >= Circ) {
            Pos.Y = FPMChunkConstants::WrapWorldCoord(Pos.Y);
            bNeedsWrap = true;
          }
          if (bNeedsWrap) {
            Pawn->SetActorLocation(Pos, false, nullptr,
                                   ETeleportType::TeleportPhysics);
          }
        }
      }
    }
  }

  // Process pending terraform tiles EVERY frame (not throttled) so the
  // bubble fills quickly. The bubble update itself is throttled below.
  // Skip on dedicated server -- terraform data is stored but visual meshes
  // are not needed (server has no renderer). Same pattern as foliage skip.
  if (!GetWorld() || GetWorld()->GetNetMode() != NM_DedicatedServer) {
    ProcessPendingTerraformTiles();
  }

  TimeSinceLastUpdate += DeltaTime;

  // Throttled update check
  if (TimeSinceLastUpdate >= UpdateInterval) {
    TimeSinceLastUpdate = 0.0f;

    const FVector PlayerPos = GetPlayerPosition();
    UpdateTerraformPlayerBubble(PlayerPos);
    const FFPMChunkCoord PlayerChunk =
        FPMChunkGenerator::WorldToChunkCoord(PlayerPos);

    // Track movement direction for anticipatory loading
    // (computed BEFORE LastPlayerChunk is updated)
    const FFPMChunkCoord MoveDelta(PlayerChunk.Q - LastPlayerChunk.Q,
                                   PlayerChunk.R - LastPlayerChunk.R);

    // Diagnostic: log current state
    UE_LOG(LogTemp, Verbose,
           TEXT("FPM Tick: PlayerChunk=%s LastChunk=%s bInitialLoad=%d "
                "LoadQ=%d UnloadQ=%d Loaded=%d"),
           *PlayerChunk.ToString(), *LastPlayerChunk.ToString(),
           bInitialLoadDone ? 1 : 0, ChunkLoadQueue.Num(),
           ChunkUnloadQueue.Num(), LoadedChunks.Num());

    // Only do a full update if the player moved to a new chunk or it's the
    // first load
    if (PlayerChunk != LastPlayerChunk || !bInitialLoadDone) {
      LastPlayerChunk = PlayerChunk;
      bInitialLoadDone = true;

      // Determine desired chunk set
      TSet<FFPMChunkCoord> DesiredChunks;
      GatherDesiredChunks(PlayerChunk, DesiredChunks);

      // Queue chunks to unload (loaded but no longer desired)
      // Add hysteresis: only unload if chunk is 2+ hex rings beyond desired
      // range
      for (auto It = LoadedChunks.CreateIterator(); It; ++It) {
        if (!DesiredChunks.Contains(It->Key)) {
          const int32 HexDist =
              FFPMChunkCoord::WrappedHexDistance(It->Key, PlayerChunk);
          // Only unload if 2 rings beyond the desired range (hysteresis)
          if (HexDist > FPMChunkConstants::LowDetailRange + 2) {
            ChunkUnloadQueue.AddUnique(It->Key);
          }
        }
      }

      // Queue chunks to load (desired but not yet loaded)
      // Also check for LOD changes on already-loaded chunks
      int32 NewlyQueued = 0;
      for (const FFPMChunkCoord &Coord : DesiredChunks) {
        if (!LoadedChunks.Contains(Coord)) {
          ChunkLoadQueue.AddUnique(Coord);
          NewlyQueued++;
        } else {
          // Update LOD if needed
          AFPMChunkActor *Existing = LoadedChunks[Coord];
          if (Existing) {
            EFPMChunkLOD NewLOD = DetermineLOD(Coord, PlayerChunk);
            if (NewLOD != Existing->GetCurrentLOD()) {
              Existing->SetChunkLOD(NewLOD);
            }
          }
        }
      }

      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Chunk update at %s  Desired=%d, NewlyQueued=%d, "
                  "LoadQ=%d, Loaded=%d"),
             *PlayerChunk.ToString(), DesiredChunks.Num(), NewlyQueued,
             ChunkLoadQueue.Num(), LoadedChunks.Num());

      // Sort load queue by hex distance DESCENDING (farthest first)
      // so that Pop()  which removes from the back  returns the
      // closest chunk each time.  Chunks in the movement direction
      // get a small distance bonus so they load first.
      ChunkLoadQueue.Sort([&PlayerChunk, &MoveDelta](const FFPMChunkCoord &A,
                                                     const FFPMChunkCoord &B) {
        int32 DistA = FFPMChunkCoord::HexDistance(A, PlayerChunk);
        int32 DistB = FFPMChunkCoord::HexDistance(B, PlayerChunk);
        // Movement-direction bonus: chunks "ahead" of the player
        // get a -3 distance bonus so they sort closer.
        if (MoveDelta.Q != 0 || MoveDelta.R != 0) {
          const int32 DotA = (A.Q - PlayerChunk.Q) * MoveDelta.Q +
                             (A.R - PlayerChunk.R) * MoveDelta.R;
          const int32 DotB = (B.Q - PlayerChunk.Q) * MoveDelta.Q +
                             (B.R - PlayerChunk.R) * MoveDelta.R;
          if (DotA > 0)
            DistA -= 3;
          if (DotB > 0)
            DistB -= 3;
        }
        return DistA > DistB; // descending  farthest at front
      });
    }
  }

  // Finalize completed async tasks FIRST so their slots are freed
  // before we try to dispatch new ones.
  FinalizePendingChunks();

  // Dispatch new async tasks from the load queue
  ProcessQueues();

  // Safety net: periodically re-scan for missing chunks.
  // This catches edge cases where an async task was lost,
  // a future never completed, or distant chunks haven't been
  // dispatched yet.
  GGapRecoveryTimer += DeltaTime;
  if (GGapRecoveryTimer >= GGapRecoveryInterval && bInitialLoadDone) {
    GGapRecoveryTimer = 0.0f;
    TSet<FFPMChunkCoord> DesiredChunks;
    GatherDesiredChunks(LastPlayerChunk, DesiredChunks);
    int32 Gaps = 0;
    for (const FFPMChunkCoord &Coord : DesiredChunks) {
      if (!LoadedChunks.Contains(Coord) && !GInFlightCoords.Contains(Coord)) {
        ChunkLoadQueue.AddUnique(Coord);
        Gaps++;
      }
    }
    if (Gaps > 0) {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Gap recovery  re-queued %d missing chunks "
                  "(loaded=%d, in-flight=%d, queued=%d)"),
             Gaps, LoadedChunks.Num(), GInFlightCoords.Num(),
             ChunkLoadQueue.Num());
    }
  }

  // Debug visualization
  if (bDrawDebugChunkBounds) {
    DrawDebugChunks();
  }

  // Keep water plane centered on the player
  if (WaterPlaneMesh) {
    const FVector PP = GetPlayerPosition();
    WaterPlaneMesh->SetWorldLocation(FVector(PP.X, PP.Y, WaterZHeight));
  }

  // --- Flowing Water Simulation ---
  if (bEnableFlowingWater && bInitialLoadDone) {
    const float SimInterval = 1.0f / FPMWaterConstants::SimulationRate;
    WaterSimTimer += DeltaTime;
    WaterMeshRebuildTimer += DeltaTime;

    if (WaterSimTimer >= SimInterval) {
      const float SimDelta = WaterSimTimer;
      WaterSimTimer = 0.0f;

      TickWaterSimulation(SimDelta);

      // Only rebuild meshes at a lower frequency (every 0.5s)
      // to avoid GPU stalls from CreateMeshSection
      if (WaterMeshRebuildTimer >= 0.5f) {
        WaterMeshRebuildTimer = 0.0f;
        RebuildDirtyWaterMeshes();
      }
    }
  }
}

// ===================================================================
//  Chunk Gathering
// ===================================================================

void AFPMWorldChunkManager::GatherDesiredChunks(
    const FFPMChunkCoord &PlayerChunk,
    TSet<FFPMChunkCoord> &OutDesiredChunks) const {
  const int32 Range = FPMChunkConstants::LowDetailRange;

  // Square grid: iterate over all chunks within Range of PlayerChunk.
  // Coords wrap toroidally  no bounds check needed.
  for (int32 DQ = -Range; DQ <= Range; ++DQ) {
    for (int32 DR = -Range; DR <= Range; ++DR) {
      const int32 WQ = FPMChunkConstants::WrapChunkCoord(PlayerChunk.Q + DQ);
      const int32 WR = FPMChunkConstants::WrapChunkCoord(PlayerChunk.R + DR);
      OutDesiredChunks.Add(FFPMChunkCoord(WQ, WR));
    }
  }
}

// ===================================================================
//  LOD Determination
// ===================================================================

EFPMChunkLOD
AFPMWorldChunkManager::DetermineLOD(const FFPMChunkCoord &ChunkCoord,
                                    const FFPMChunkCoord &PlayerChunk) const {
  const int32 HexDist =
      FFPMChunkCoord::WrappedHexDistance(ChunkCoord, PlayerChunk);

  if (HexDist <= FPMChunkConstants::FullDetailRange) {
    return EFPMChunkLOD::Full;
  }
  if (HexDist <= FPMChunkConstants::MediumDetailRange) {
    return EFPMChunkLOD::Medium;
  }
  if (HexDist <= FPMChunkConstants::LowDetailRange) {
    return EFPMChunkLOD::Low;
  }
  return EFPMChunkLOD::Unloaded;
}

// ===================================================================
//  Chunk Loading / Unloading
// ===================================================================

void AFPMWorldChunkManager::LoadChunkSync(const FFPMChunkCoord &Coord,
                                          EFPMChunkLOD LOD) {
  if (LoadedChunks.Contains(Coord)) {
    return; // Already loaded
  }

  // 1. Generate voxel mesh via Marching Cubes (deterministic from seed)
  FFPMVoxelMeshData MeshData;
  FPMVoxelGenerator::GenerateAndMesh(Coord, WorldSeed, MeshData);

  // 3. Clip for active bubble tiles, then finalize
  ClipMeshForTerraformTiles(MeshData, Coord);
  FinalizeChunk(Coord, MoveTemp(MeshData));
}

void AFPMWorldChunkManager::FinalizeChunk(const FFPMChunkCoord &Coord,
                                          FFPMVoxelMeshData &&MeshData) {
  if (LoadedChunks.Contains(Coord)) {
    return; // Double-load guard
  }

  // Spawn chunk actor (game-thread only)
  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  AFPMChunkActor *ChunkActor = GetWorld()->SpawnActor<AFPMChunkActor>(
      AFPMChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
      SpawnParams);

  if (ChunkActor) {
    // Pass biome configuration before initialization
    if (BiomePCGConfig) {
      ChunkActor->SetBiomePCGConfig(BiomePCGConfig, WorldSeed);
    }

    // Initialize with voxel mesh data
    ChunkActor->InitializeVoxelChunk(MeshData, Coord);

    // Apply terrain material.
    // If no material is assigned in the editor, auto-create a minimal one that
    // reads vertex colours  this ensures biome colours always show even
    // before a proper artist material is set up.
    UProceduralMeshComponent *PMC =
        ChunkActor->FindComponentByClass<UProceduralMeshComponent>();
    if (PMC) {
      // Priority 1: editor-assigned material
      if (TerrainMaterial) {
        PMC->SetMaterial(0, TerrainMaterial);
      } else {
        // Priority 2: auto-load M_TerrainBiome (vertex-colour driven, created
        // by FPMTerrainMaterialBuilder on first editor run). Cache once.
        if (!AutoTerrainMaterial) {
          UMaterialInterface *Loaded = Cast<UMaterialInterface>(
              StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                               TEXT("/Game/Materials/M_TerrainBiome")));
          if (Loaded) {
            // Store as a plain UMaterialInterface so we can use it directly
            // on each PMC without creating per-chunk MID instances.
            AutoTerrainMaterial =
                UMaterialInstanceDynamic::Create(Loaded, this);
          }
        }
        if (AutoTerrainMaterial) {
          PMC->SetMaterial(0, AutoTerrainMaterial);
        } else {
          // Priority 3: last resort  at least show something visible
          UMaterialInterface *Grid = Cast<UMaterialInterface>(StaticLoadObject(
              UMaterialInterface::StaticClass(), nullptr,
              TEXT("/Engine/EngineMaterials/WorldGridMaterial")));
          if (Grid)
            PMC->SetMaterial(0, Grid);
          UE_LOG(LogTemp, Warning,
                 TEXT("FPM: M_TerrainBiome not found  biome colours will "
                      "not show. Run the editor once to auto-create it."));
        }
      }
    }

    LoadedChunks.Add(Coord, ChunkActor);

    // Initialize flowing water for this chunk
    if (bEnableFlowingWater) {
      InitializeChunkWater(Coord);
    }

    // Diagnostic: warn about empty chunks
    if (MeshData.Vertices.Num() == 0) {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: ? Chunk %s generated 0 vertices (invisible gap)!"),
             *Coord.ToString());
    } else {
      UE_LOG(LogTemp, Verbose, TEXT("FPM: Finalized chunk %s (%d verts)"),
             *Coord.ToString(), MeshData.Vertices.Num());
    }
  }
}

void AFPMWorldChunkManager::UnloadChunk(const FFPMChunkCoord &Coord) {
  // Cancel any in-flight async task for this coord
  GInFlightCoords.Remove(Coord);

  AFPMChunkActor **FoundActor = LoadedChunks.Find(Coord);
  if (FoundActor && *FoundActor) {
    FFPMChunkOverlay *Overlay = LoadedOverlays.Find(Coord);
    if (Overlay && Overlay->HasModifications()) {
      FPMChunkOverlayManager::SaveOverlay(*Overlay);
    }
    (*FoundActor)->Destroy();
  }

  LoadedChunks.Remove(Coord);
  LoadedOverlays.Remove(Coord);

  // Clean up water data for this chunk
  if (bEnableFlowingWater) {
    CleanupChunkWater(Coord);
  }

  UE_LOG(LogTemp, Verbose, TEXT("FPM: Unloaded chunk %s"), *Coord.ToString());
}

// ===================================================================
//  Queue Processing (Async)
// ===================================================================

void AFPMWorldChunkManager::ProcessQueues() {
  // Throttle unloads -- max 4 per frame
  int32 UnloadsProcessed = 0;
  while (ChunkUnloadQueue.Num() > 0 && UnloadsProcessed < 4) {
    const FFPMChunkCoord Coord = ChunkUnloadQueue.Pop();
    UnloadChunk(Coord);
    UnloadsProcessed++;
  }

  // Dispatch async generation tasks up to the concurrency limit.
  // Each task runs FPMVoxelGenerator::GenerateAndMesh on a worker thread.
  // The queue is sorted so Pop() gives the closest chunk first.
  const int32 MaxConcurrent = GetMaxConcurrentGenerations();
  int32 Dispatched = 0;
  while (ChunkLoadQueue.Num() > 0 &&
         GPendingGenerations.Num() < MaxConcurrent) {
    const FFPMChunkCoord Coord = ChunkLoadQueue.Pop();

    // Skip if already loaded or already in-flight (don't count against limit)
    if (LoadedChunks.Contains(Coord) || GInFlightCoords.Contains(Coord)) {
      continue;
    }

    GInFlightCoords.Add(Coord);

    // Capture values for the lambda (no UObject/Actor access inside!)
    const int32 Seed = WorldSeed;

    TSharedPtr<FPendingChunk> Pending = MakeShared<FPendingChunk>();
    Pending->Coord = Coord;
    Pending->Future = Async(
        EAsyncExecution::ThreadPool, [Coord, Seed]() -> FFPMVoxelMeshData {
          FFPMVoxelMeshData Result;
          // Pre-reserve arrays to avoid reallocation during marching cubes.
          // Typical chunk produces ~2K-6K vertices.
          Result.Vertices.Reserve(4096);
          Result.Triangles.Reserve(4096 * 3);
          Result.Normals.Reserve(4096);
          Result.UVs.Reserve(4096);
          Result.Colors.Reserve(4096);
          FPMVoxelGenerator::GenerateAndMesh(Coord, Seed, Result);
          return Result;
        });

    GPendingGenerations.Add(Pending);
    Dispatched++;
  }

  // Log dispatch info (Verbose to avoid per-frame overhead)
  if (Dispatched > 0 || GPendingGenerations.Num() > 0) {
    UE_LOG(LogTemp, Verbose,
           TEXT("FPM: Dispatched %d, in-flight %d, queued %d, loaded %d"),
           Dispatched, GPendingGenerations.Num(), ChunkLoadQueue.Num(),
           LoadedChunks.Num());
  }
}

void AFPMWorldChunkManager::FinalizePendingChunks() {
  // Poll all pending futures; finalize any that are ready.
  // Throttle to GMaxFinalizationsPerTick to keep game-thread cost bounded.
  // Iterate in reverse so we can RemoveAtSwap safely.
  int32 Finalized = 0;
  for (int32 i = GPendingGenerations.Num() - 1;
       i >= 0 && Finalized < GMaxFinalizationsPerTick; --i) {
    TSharedPtr<FPendingChunk> &Pending = GPendingGenerations[i];
    if (!Pending->Future.IsReady()) {
      continue;
    }

    // Retrieve completed mesh data
    FFPMVoxelMeshData MeshData = Pending->Future.Get();
    const FFPMChunkCoord Coord = Pending->Coord;

    // Remove from tracking
    GInFlightCoords.Remove(Coord);
    GPendingGenerations.RemoveAtSwap(i);

    // Clip for active bubble tiles, then finalize
    ClipMeshForTerraformTiles(MeshData, Coord);
    FinalizeChunk(Coord, MoveTemp(MeshData));
    Finalized++;
  }
}

// ===================================================================
//  Public Methods
// ===================================================================

void AFPMWorldChunkManager::RegenerateAllChunks() {
  UE_LOG(LogTemp, Warning, TEXT("FPM: Regenerating all %d loaded chunks..."),
         LoadedChunks.Num());

  // Collect current chunk coords
  TArray<FFPMChunkCoord> CurrentChunks;
  LoadedChunks.GetKeys(CurrentChunks);

  // Unload all
  for (const FFPMChunkCoord &Coord : CurrentChunks) {
    UnloadChunk(Coord);
  }

  // Force reload on next update
  bInitialLoadDone = false;
  TimeSinceLastUpdate = UpdateInterval;
}

// ===================================================================
//  Terraforming  Console Commands
// ===================================================================

FAutoConsoleCommand AFPMWorldChunkManager::CmdTerraformDig(
    TEXT("FPM.Dig"),
    TEXT("Dig terrain at camera crosshair. Usage: FPM.Dig [radius] [strength]"),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          if (GActiveChunkManager) {
            float Radius = 400.f; // 4m default
            float Str = 1.0f;
            if (Args.Num() > 0)
              Radius = FCString::Atof(*Args[0]);
            if (Args.Num() > 1)
              Str = FCString::Atof(*Args[1]);
            GActiveChunkManager->TerraformFromCamera(Radius, FMath::Abs(Str));
          } else {
            UE_LOG(LogTemp, Error, TEXT("FPM: No active chunk manager!"));
          }
        }));

FAutoConsoleCommand AFPMWorldChunkManager::CmdTerraformFill(
    TEXT("FPM.Fill"),
    TEXT("Fill terrain at camera crosshair. Usage: FPM.Fill [radius] "
         "[strength]"),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          if (GActiveChunkManager) {
            float Radius = 400.f; // 4m default
            float Str = 1.0f;
            if (Args.Num() > 0)
              Radius = FCString::Atof(*Args[0]);
            if (Args.Num() > 1)
              Str = FCString::Atof(*Args[1]);
            GActiveChunkManager->TerraformFromCamera(Radius, -FMath::Abs(Str));
          } else {
            UE_LOG(LogTemp, Error, TEXT("FPM: No active chunk manager!"));
          }
        }));

FAutoConsoleCommand AFPMWorldChunkManager::CmdTerraformReset(
    TEXT("FPM.TerraformReset"),
    TEXT("Reset all terraforming to procedural terrain."),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          if (GActiveChunkManager) {
            GActiveChunkManager->ResetAllTerraforming();
          } else {
            UE_LOG(LogTemp, Error, TEXT("FPM: No active chunk manager!"));
          }
        }));

// ===================================================================
//  Terraforming  Camera Trace
// ===================================================================

void AFPMWorldChunkManager::TerraformFromCamera(float Radius, float Strength) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  APlayerController *PC = World->GetFirstPlayerController();
  if (!PC) {
    UE_LOG(LogTemp, Warning, TEXT("FPM Terraform: No player controller found"));
    return;
  }

  // Get camera location and direction
  FVector CamLoc;
  FRotator CamRot;
  PC->GetPlayerViewPoint(CamLoc, CamRot);

  const FVector TraceDir = CamRot.Vector();
  const FVector TraceEnd = CamLoc + TraceDir * 500000.f; // 5km trace

  UE_LOG(LogTemp, Log,
         TEXT("FPM Terraform: Trace from (%.0f, %.0f, %.0f) dir (%.2f, %.2f, "
              "%.2f)"),
         CamLoc.X, CamLoc.Y, CamLoc.Z, TraceDir.X, TraceDir.Y, TraceDir.Z);

  FHitResult Hit;
  FCollisionQueryParams Params;
  Params.bTraceComplex = false; // PMC uses complex-as-simple, so simple is fine
  Params.bReturnPhysicalMaterial = false;
  if (PC->GetPawn()) {
    Params.AddIgnoredActor(PC->GetPawn());
  }

  // Try Visibility channel first, fall back to WorldStatic
  bool bHit = World->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd,
                                              ECC_Visibility, Params);
  if (!bHit) {
    bHit = World->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd,
                                           ECC_WorldStatic, Params);
  }

  if (bHit) {
    const FString Msg = FString::Printf(
        TEXT("Terraform HIT at (%.0f, %.0f, %.0f)  %s R=%.0f S=%.1f on %s"),
        Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z,
        Strength > 0 ? TEXT("DIG") : TEXT("FILL"), Radius, Strength,
        Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("null"));
    UE_LOG(LogTemp, Warning, TEXT("FPM %s"), *Msg);
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Msg);
    TerraformAtPoint(Hit.ImpactPoint, Radius, Strength);
  } else {
    const FString Msg = FString::Printf(
        TEXT("Terraform MISS  no terrain hit from camera (%.0f,%.0f,%.0f)"),
        CamLoc.X, CamLoc.Y, CamLoc.Z);
    UE_LOG(LogTemp, Warning, TEXT("FPM %s"), *Msg);
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Msg);
  }
}

// ===================================================================
//  Terraforming - Fine-Resolution Tile System (200cm voxels)
// ===================================================================

FIntVector AFPMWorldChunkManager::WorldToTileCoord(const FVector &WorldPos) {
  using namespace FPMVoxelConstants;
  return FIntVector(FMath::FloorToInt(WorldPos.X / TerraformTileWorldSize),
                    FMath::FloorToInt(WorldPos.Y / TerraformTileWorldSize),
                    FMath::FloorToInt(WorldPos.Z / TerraformTileWorldSize));
}

void AFPMWorldChunkManager::TerraformAtPoint(FVector WorldPos, float Radius,
                                             float Strength) {
  using namespace FPMVoxelConstants;

  const float FineVS = TerraformVoxelSizeCm;
  const float CoarseVS = VoxelSizeCm;

  const int32 MinIX = FMath::FloorToInt((WorldPos.X - Radius) / FineVS);
  const int32 MaxIX = FMath::FloorToInt((WorldPos.X + Radius) / FineVS);
  const int32 MinIY = FMath::FloorToInt((WorldPos.Y - Radius) / FineVS);
  const int32 MaxIY = FMath::FloorToInt((WorldPos.Y + Radius) / FineVS);
  const int32 MinIZ = FMath::FloorToInt((WorldPos.Z - Radius) / FineVS);
  const int32 MaxIZ = FMath::FloorToInt((WorldPos.Z + Radius) / FineVS);

  TSet<FIntVector> AffectedTiles;
  TSet<FFPMChunkCoord> AffectedChunks;
  int32 ModifiedVoxels = 0;

  for (int32 IZ = MinIZ; IZ <= MaxIZ; ++IZ) {
    for (int32 IY = MinIY; IY <= MaxIY; ++IY) {
      for (int32 IX = MinIX; IX <= MaxIX; ++IX) {
        const FVector VC((IX + 0.5f) * FineVS, (IY + 0.5f) * FineVS,
                         (IZ + 0.5f) * FineVS);
        const float Dist = FVector::Dist(WorldPos, VC);
        if (Dist > Radius)
          continue;

        const float T = Dist / Radius;
        const float Falloff =
            1.0f - (T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f));
        const float Delta = -Strength * Falloff * FineVS;

        const FIntVector FineKey(IX, IY, IZ);
        const FIntVector TileCoord = WorldToTileCoord(VC);
        const FFPMChunkCoord ChunkCoord =
            FPMChunkGenerator::WorldToChunkCoord(VC);
        ChunkTerraformOverlays.FindOrAdd(ChunkCoord).FindOrAdd(FineKey) += Delta;

        const FIntVector CoarseKey(FMath::FloorToInt(VC.X / CoarseVS),
                                   FMath::FloorToInt(VC.Y / CoarseVS),
                                   FMath::FloorToInt(VC.Z / CoarseVS));
        VoxelOverlays.FindOrAdd(ChunkCoord).FindOrAdd(CoarseKey) +=
            Delta * (FineVS / CoarseVS);
        ++ModifiedVoxels;
        AffectedTiles.Add(TileCoord);
        AffectedChunks.Add(ChunkCoord);
      }
    }
  }

  FineTerraformOverlays.Reset();

  for (const FIntVector &TC : AffectedTiles) {
    RegenerateTerraformTile(TC, true);
  }

  for (const FIntVector &TC : AffectedTiles) {
    for (int32 DZ = -1; DZ <= 1; ++DZ) {
      for (int32 DY = -1; DY <= 1; ++DY) {
        for (int32 DX = -1; DX <= 1; ++DX) {
          if (DX == 0 && DY == 0 && DZ == 0) continue;
          const FIntVector Neighbor(TC.X + DX, TC.Y + DY, TC.Z + DZ);
          if (!InFlightTerraformTiles.Contains(Neighbor)) {
            PendingTilesToGenerate.AddUnique(Neighbor);
          }
        }
      }
    }
  }

  // Refresh the local bubble before coarse chunks are regenerated so clip
  // removal sees the current edited tiles instead of stale bubble state.
  UpdateTerraformPlayerBubble(WorldPos);

  int32 RegeneratedChunks = 0;
  for (const FFPMChunkCoord &Coord : AffectedChunks) {
    PendingChunkClips.AddUnique(Coord);
    if (LoadedChunks.Contains(Coord)) {
      RegenerateChunk(Coord);
      ++RegeneratedChunks;
    }
  }

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(
        -1, 4.0f, FColor::Cyan,
        FString::Printf(TEXT("Terraform: %d voxels, %d tiles, %d chunks (R=%.0f)"),
                        ModifiedVoxels, AffectedTiles.Num(), RegeneratedChunks,
                        Radius));
  }
}

void AFPMWorldChunkManager::GatherChunkTerraformDeltasForTileNeighborhood(
    const FIntVector &TileCoord, TMap<FIntVector, float> &OutDeltas) const {
  using namespace FPMVoxelConstants;

  for (const TPair<FFPMChunkCoord, TMap<FIntVector, float>> &ChunkEntry :
       ChunkTerraformOverlays) {
    for (const TPair<FIntVector, float> &VoxelEntry : ChunkEntry.Value) {
      const FVector VoxelCenter((VoxelEntry.Key.X + 0.5f) * TerraformVoxelSizeCm,
                                (VoxelEntry.Key.Y + 0.5f) * TerraformVoxelSizeCm,
                                (VoxelEntry.Key.Z + 0.5f) * TerraformVoxelSizeCm);
      const FIntVector VoxelTileCoord = WorldToTileCoord(VoxelCenter);
      if (IsTileWithinNeighborhood(VoxelTileCoord, TileCoord)) {
        OutDeltas.FindOrAdd(VoxelEntry.Key) += VoxelEntry.Value;
      }
    }
  }
}

bool AFPMWorldChunkManager::HasChunkTerraformDeltasForTile(
    const FIntVector &TileCoord) const {
  using namespace FPMVoxelConstants;

  for (const TPair<FFPMChunkCoord, TMap<FIntVector, float>> &ChunkEntry :
       ChunkTerraformOverlays) {
    for (const TPair<FIntVector, float> &VoxelEntry : ChunkEntry.Value) {
      const FVector VoxelCenter((VoxelEntry.Key.X + 0.5f) * TerraformVoxelSizeCm,
                                (VoxelEntry.Key.Y + 0.5f) * TerraformVoxelSizeCm,
                                (VoxelEntry.Key.Z + 0.5f) * TerraformVoxelSizeCm);
      if (WorldToTileCoord(VoxelCenter) == TileCoord) {
        return true;
      }
    }
  }

  return false;
}

void AFPMWorldChunkManager::ProcessPendingTerraformTiles() {
  using namespace FPMVoxelConstants;

  // --- 1. Finalize completed async tasks FIRST (frees slots) ---
  FinalizePendingTerraformTiles();

  // --- 2. Dispatch new async tasks from the pending queue ---
  while (PendingTilesToGenerate.Num() > 0 &&
         PendingTerraformGenerations.Num() < MaxConcurrentTerraformGenerations) {
    const FIntVector TC = PendingTilesToGenerate.Pop(EAllowShrinking::No);

    // Skip if already generated or already in-flight
    if (InFlightTerraformTiles.Contains(TC)) {
      continue;
    }
    AActor *const *Existing = TerraformTileActors.Find(TC);
    if (Existing && IsValid(*Existing)) {
      continue;
    }

    InFlightTerraformTiles.Add(TC);
    TMap<FIntVector, float> CombinedDeltas;
    GatherChunkTerraformDeltasForTileNeighborhood(TC, CombinedDeltas);

    const bool bForce = false; // edit-driven neighbor regen only
    if (CombinedDeltas.Num() == 0 && !bForce) {
      InFlightTerraformTiles.Remove(TC);
      continue;
    }

    // Capture values for the lambda - no UObject access inside!
    const FVector TileOrigin(TC.X * TerraformTileWorldSize,
                             TC.Y * TerraformTileWorldSize,
                             TC.Z * TerraformTileWorldSize);
    const int32 Seed = WorldSeed;

    TSharedPtr<FPendingTerraformTile> Pending = MakeShared<FPendingTerraformTile>();
    Pending->TileCoord = TC;
    Pending->bForceBaseSurface = bForce;
    Pending->Future = Async(
        EAsyncExecution::ThreadPool,
        [TileOrigin, Seed, Deltas = MoveTemp(CombinedDeltas), bForce]() -> FFPMVoxelMeshData {
          FFPMVoxelMeshData Result;
          Result.Vertices.Reserve(1024);
          Result.Triangles.Reserve(1024 * 3);
          Result.Normals.Reserve(1024);
          Result.UVs.Reserve(1024);
          Result.Colors.Reserve(1024);
          FPMVoxelGenerator::GenerateTerraformTile(
              TileOrigin, Seed, Deltas, Result, bForce);
          return Result;
        });

    PendingTerraformGenerations.Add(Pending);
  }

    // Process at most 1 chunk clip-regen per frame to avoid lag spikes.
  if (PendingChunkClips.Num() > 0) {
    const FFPMChunkCoord CC = PendingChunkClips.Pop();
    if (LoadedChunks.Contains(CC)) {
      RegenerateChunk(CC);
    }
  }
}

// ===================================================================
//  Finalize completed async terraform tiles (game thread)
// ===================================================================

void AFPMWorldChunkManager::FinalizePendingTerraformTiles() {
  int32 Finalized = 0;
  for (int32 i = PendingTerraformGenerations.Num() - 1;
       i >= 0 && Finalized < MaxTerraformFinalizationsPerTick; --i) {
    TSharedPtr<FPendingTerraformTile> &Pending = PendingTerraformGenerations[i];
    if (!Pending->Future.IsReady()) {
      continue;
    }

    FFPMVoxelMeshData MeshData = Pending->Future.Get();
    const FIntVector TC = Pending->TileCoord;

    InFlightTerraformTiles.Remove(TC);
    PendingTerraformGenerations.RemoveAtSwap(i);

    FinalizeTerraformTile(TC, MoveTemp(MeshData));
    Finalized++;
  }
}

// ===================================================================
//  Finalize a single terraform tile: spawn/update actor with mesh
// ===================================================================

void AFPMWorldChunkManager::FinalizeTerraformTile(
    const FIntVector &TileCoord, FFPMVoxelMeshData &&MeshData) {

  AActor *&TileActor = TerraformTileActors.FindOrAdd(TileCoord);

  // If this tile no longer produces geometry, remove any stale actor.
  if (MeshData.Vertices.Num() == 0) {
    if (TileActor) {
      TileActor->Destroy();
      TileActor = nullptr;
    }
    TerraformTileActors.Remove(TileCoord);
    return;
  }

  if (!TileActor) {
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TileActor = GetWorld()->SpawnActor<AActor>(
        AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
        SpawnParams);
    if (!TileActor)
      return;

    TileActor->SetReplicates(false);

    UProceduralMeshComponent *PMC =
        NewObject<UProceduralMeshComponent>(TileActor, TEXT("TerraformMesh"));
    PMC->RegisterComponent();
    PMC->AttachToComponent(TileActor->GetRootComponent(),
                           FAttachmentTransformRules::KeepRelativeTransform);
    TileActor->SetRootComponent(PMC);

    PMC->bUseComplexAsSimpleCollision = true;
    PMC->SetCollisionProfileName(TEXT("BlockAll"));
    PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (TerrainMaterial) {
      PMC->SetMaterial(0, TerrainMaterial);
    } else if (AutoTerrainMaterial) {
      PMC->SetMaterial(0, AutoTerrainMaterial);
    }
  }

  UProceduralMeshComponent *PMC =
      TileActor->FindComponentByClass<UProceduralMeshComponent>();
  if (PMC && MeshData.Vertices.Num() > 0) {
    TileActor->SetActorLocation(FVector(0.0f, 0.0f, 2.0f));
    TArray<FProcMeshTangent> Tangents;
    PMC->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles,
                           MeshData.Normals, MeshData.UVs, MeshData.Colors,
                           Tangents, true);
  }
}
void AFPMWorldChunkManager::UpdateTerraformPlayerBubble(
    const FVector &PlayerPos) {
  const FIntVector CenterTile = WorldToTileCoord(PlayerPos);
  if (bTerraformBubbleInitialized && CenterTile == LastTerraformBubbleCenter) {
    return;
  }

  constexpr int32 BubbleRadiusTiles = 2;
  TSet<FIntVector> DesiredTiles;

  for (const TPair<FFPMChunkCoord, TMap<FIntVector, float>> &ChunkEntry :
       ChunkTerraformOverlays) {
    for (const TPair<FIntVector, float> &VoxelEntry : ChunkEntry.Value) {
      const FVector VoxelCenter(
          (VoxelEntry.Key.X + 0.5f) * FPMVoxelConstants::TerraformVoxelSizeCm,
          (VoxelEntry.Key.Y + 0.5f) * FPMVoxelConstants::TerraformVoxelSizeCm,
          (VoxelEntry.Key.Z + 0.5f) * FPMVoxelConstants::TerraformVoxelSizeCm);
      const FIntVector TileCoord = WorldToTileCoord(VoxelCenter);
      if (FMath::Abs(TileCoord.X - CenterTile.X) > BubbleRadiusTiles ||
          FMath::Abs(TileCoord.Y - CenterTile.Y) > BubbleRadiusTiles ||
          FMath::Abs(TileCoord.Z - CenterTile.Z) > BubbleRadiusTiles) {
        continue;
      }

      for (int32 DZ = -1; DZ <= 1; ++DZ) {
        for (int32 DY = -1; DY <= 1; ++DY) {
          for (int32 DX = -1; DX <= 1; ++DX) {
            const FIntVector NeighborTile(TileCoord.X + DX, TileCoord.Y + DY,
                                          TileCoord.Z + DZ);
            if (FMath::Abs(NeighborTile.X - CenterTile.X) <= BubbleRadiusTiles + 1 &&
                FMath::Abs(NeighborTile.Y - CenterTile.Y) <= BubbleRadiusTiles + 1 &&
                FMath::Abs(NeighborTile.Z - CenterTile.Z) <= BubbleRadiusTiles + 1) {
              DesiredTiles.Add(NeighborTile);
            }
          }
        }
      }
    }
  }

  TSet<FIntVector> TilesToRemove;
  for (const FIntVector &TileCoord : ActiveTerraformBubbleTiles) {
    if (!DesiredTiles.Contains(TileCoord)) {
      TilesToRemove.Add(TileCoord);
    }
  }

  TSet<FIntVector> TilesToAdd;
  for (const FIntVector &TileCoord : DesiredTiles) {
    if (!ActiveTerraformBubbleTiles.Contains(TileCoord)) {
      TilesToAdd.Add(TileCoord);
    }
  }

  for (const FIntVector &TileCoord : TilesToRemove) {
    if (AActor **TileActor = TerraformTileActors.Find(TileCoord)) {
      if (IsValid(*TileActor)) {
        (*TileActor)->Destroy();
      }
      TerraformTileActors.Remove(TileCoord);
    }
    InFlightTerraformTiles.Remove(TileCoord);
    PendingTilesToGenerate.Remove(TileCoord);
  }

  for (const FIntVector &TileCoord : TilesToAdd) {
    RegenerateTerraformTile(TileCoord, false);
    if (!TerraformTileActors.Contains(TileCoord) &&
        !InFlightTerraformTiles.Contains(TileCoord)) {
      PendingTilesToGenerate.AddUnique(TileCoord);
    }

    const FVector TileCenter(
        (TileCoord.X + 0.5f) * FPMVoxelConstants::TerraformTileWorldSize,
        (TileCoord.Y + 0.5f) * FPMVoxelConstants::TerraformTileWorldSize,
        (TileCoord.Z + 0.5f) * FPMVoxelConstants::TerraformTileWorldSize);
    const FFPMChunkCoord ChunkCoord =
        FPMChunkGenerator::WorldToChunkCoord(TileCenter);
    if (LoadedChunks.Contains(ChunkCoord)) {
      PendingChunkClips.AddUnique(ChunkCoord);
    }
  }

  ActiveTerraformBubbleTiles = MoveTemp(DesiredTiles);
  LastTerraformBubbleCenter = CenterTile;
  bTerraformBubbleInitialized = true;
}
void AFPMWorldChunkManager::RegenerateTerraformTile(const FIntVector &TileCoord,
                                                    bool bForceBaseSurface) {
  using namespace FPMVoxelConstants;

  const FVector TileOrigin(TileCoord.X * TerraformTileWorldSize,
                           TileCoord.Y * TerraformTileWorldSize,
                           TileCoord.Z * TerraformTileWorldSize);

  const bool bHasLocalDeltas = HasChunkTerraformDeltasForTile(TileCoord);
  const bool bShouldForceBaseSurface = bForceBaseSurface && !bHasLocalDeltas;

  TMap<FIntVector, float> CombinedDeltas;
  GatherChunkTerraformDeltasForTileNeighborhood(TileCoord, CombinedDeltas);

  if (CombinedDeltas.Num() == 0 && !bShouldForceBaseSurface)
    return;

  FFPMVoxelMeshData MeshData;
  FPMVoxelGenerator::GenerateTerraformTile(
      TileOrigin, WorldSeed, CombinedDeltas, MeshData, bShouldForceBaseSurface);

  AActor *&TileActor = TerraformTileActors.FindOrAdd(TileCoord);

  // If this tile no longer produces geometry, remove any stale actor.
  if (MeshData.Vertices.Num() == 0) {
    if (TileActor) {
      TileActor->Destroy();
      TileActor = nullptr;
    }
    TerraformTileActors.Remove(TileCoord);
    return;
  }

  if (!TileActor) {
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TileActor = GetWorld()->SpawnActor<AActor>(
        AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
        SpawnParams);
    if (!TileActor)
      return;

    // Terraform tiles are client-side only
    // disable network replication to prevent FNetGUIDCache warnings about
    // unsupported ProceduralMeshComponent.
    TileActor->SetReplicates(false);

    UProceduralMeshComponent *PMC =
        NewObject<UProceduralMeshComponent>(TileActor, TEXT("TerraformMesh"));
    PMC->RegisterComponent();
    PMC->AttachToComponent(TileActor->GetRootComponent(),
                           FAttachmentTransformRules::KeepRelativeTransform);
    TileActor->SetRootComponent(PMC);

    PMC->bUseComplexAsSimpleCollision = true;

    PMC->SetCollisionProfileName(TEXT("BlockAll"));
    PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (TerrainMaterial) {
      PMC->SetMaterial(0, TerrainMaterial);
    } else if (AutoTerrainMaterial) {
      PMC->SetMaterial(0, AutoTerrainMaterial);
    }
  }

  UProceduralMeshComponent *PMC =
      TileActor->FindComponentByClass<UProceduralMeshComponent>();
  if (PMC && MeshData.Vertices.Num() > 0) {
    // Small Z offset (+2cm) so fine tile renders reliably above coarse mesh
    // at overlap regions. Prevents Z-fighting at the LOD seam boundary.
    TileActor->SetActorLocation(FVector(0.0f, 0.0f, 2.0f));
    TArray<FProcMeshTangent> Tangents;
    PMC->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles,
                           MeshData.Normals, MeshData.UVs, MeshData.Colors,
                           Tangents, true);
  }
}

// ===================================================================
//  Terraforming  Single Chunk Regeneration
// ===================================================================

// ===================================================================
//  ClipMeshForTerraformTiles - Remove coarse mesh triangles that
//  overlap with active terraform tiles. This creates precise holes
//  in the coarse mesh that the fine tiles fill.
//
//  IMPORTANT: We build the clip region from ActiveTerraformBubbleTiles
void AFPMWorldChunkManager::ClipMeshForTerraformTiles(
    FFPMVoxelMeshData &MeshData, const FFPMChunkCoord &ChunkCoord) {
  using namespace FPMVoxelConstants;

  if (ActiveTerraformBubbleTiles.Num() == 0 || MeshData.Triangles.Num() == 0) {
    return;
  }

  const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(ChunkCoord);
  const float ChunkSize = FPMChunkConstants::ChunkWorldSize;
  constexpr float ClipPad = 200.0f;

  struct FClipRect {
    float MinX;
    float MinY;
    float MaxX;
    float MaxY;
  };

  TArray<FClipRect> ClipRects;

  for (const FIntVector &TileCoord : ActiveTerraformBubbleTiles) {
    if (!HasChunkTerraformDeltasForTile(TileCoord)) {
      continue;
    }

    const FVector TileOrigin(TileCoord.X * TerraformTileWorldSize,
                             TileCoord.Y * TerraformTileWorldSize,
                             TileCoord.Z * TerraformTileWorldSize);
    const float LocalMinX = TileOrigin.X - ChunkOrigin.X - ClipPad;
    const float LocalMinY = TileOrigin.Y - ChunkOrigin.Y - ClipPad;
    const float LocalMaxX = LocalMinX + TerraformTileWorldSize + ClipPad * 2.0f;
    const float LocalMaxY = LocalMinY + TerraformTileWorldSize + ClipPad * 2.0f;

    if (LocalMaxX < 0.0f || LocalMaxY < 0.0f ||
        LocalMinX > ChunkSize || LocalMinY > ChunkSize) {
      continue;
    }

    ClipRects.Add({LocalMinX, LocalMinY, LocalMaxX, LocalMaxY});
  }

  if (ClipRects.Num() == 0) {
    return;
  }

  FFPMVoxelMeshData Filtered;
  const int32 NumTris = MeshData.Triangles.Num() / 3;
  int32 ClippedCount = 0;

  for (int32 T = 0; T < NumTris; ++T) {
    const int32 I0 = MeshData.Triangles[T * 3];
    const int32 I1 = MeshData.Triangles[T * 3 + 1];
    const int32 I2 = MeshData.Triangles[T * 3 + 2];

    const FVector &V0 = MeshData.Vertices[I0];
    const FVector &V1 = MeshData.Vertices[I1];
    const FVector &V2 = MeshData.Vertices[I2];
    const FVector Centroid = (V0 + V1 + V2) / 3.0f;

    bool bCentroidInClipRect = false;
    for (const FClipRect &Rect : ClipRects) {
      if (Centroid.X >= Rect.MinX && Centroid.X <= Rect.MaxX &&
          Centroid.Y >= Rect.MinY && Centroid.Y <= Rect.MaxY) {
        bCentroidInClipRect = true;
        break;
      }
    }

    if (bCentroidInClipRect) {
      ++ClippedCount;
      continue;
    }

    const int32 Base = Filtered.Vertices.Num();
    for (int32 V : {I0, I1, I2}) {
      Filtered.Vertices.Add(MeshData.Vertices[V]);
      if (V < MeshData.Normals.Num())
        Filtered.Normals.Add(MeshData.Normals[V]);
      if (V < MeshData.UVs.Num())
        Filtered.UVs.Add(MeshData.UVs[V]);
      if (V < MeshData.Colors.Num())
        Filtered.Colors.Add(MeshData.Colors[V]);
    }
    Filtered.Triangles.Add(Base);
    Filtered.Triangles.Add(Base + 1);
    Filtered.Triangles.Add(Base + 2);
  }

  UE_LOG(LogTemp, Verbose,
         TEXT("FPM ClipMesh: Clipped %d / %d coarse triangles for chunk (%d,%d) across %d edited bubble tiles"),
         ClippedCount, NumTris, ChunkCoord.Q, ChunkCoord.R, ClipRects.Num());

  MeshData = MoveTemp(Filtered);
}



void AFPMWorldChunkManager::RegenerateChunk(const FFPMChunkCoord &Coord) {
  // 1. Destroy the existing chunk actor
  AFPMChunkActor **FoundActor = LoadedChunks.Find(Coord);
  if (FoundActor && *FoundActor) {
    (*FoundActor)->Destroy();
  }
  LoadedChunks.Remove(Coord);

  // 2. Re-generate voxel mesh (will pick up voxel overlay deltas)
  FFPMVoxelMeshData MeshData;
  FPMVoxelGenerator::GenerateAndMesh(Coord, WorldSeed, MeshData);

  // 2b. Clip triangles that overlap with active terraform tiles
  ClipMeshForTerraformTiles(MeshData, Coord);

  // 3. Apply voxel overlay to the density field and re-mesh
  // (The overlay is applied inside GenerateAndMesh via the global accessor)

  // 4. Finalize  spawn new chunk actor
  FinalizeChunk(Coord, MoveTemp(MeshData));
}

// ===================================================================
//  Terraforming  Voxel Delta Accessors
// ===================================================================

float AFPMWorldChunkManager::GetVoxelDelta(const FFPMChunkCoord &Coord,
                                           const FIntVector &VoxelKey) const {
  const TMap<FIntVector, float> *ChunkDeltas = VoxelOverlays.Find(Coord);
  if (!ChunkDeltas)
    return 0.0f;
  const float *Delta = ChunkDeltas->Find(VoxelKey);
  return Delta ? *Delta : 0.0f;
}

bool AFPMWorldChunkManager::HasVoxelDeltas(const FFPMChunkCoord &Coord) const {
  const TMap<FIntVector, float> *ChunkDeltas = VoxelOverlays.Find(Coord);
  return ChunkDeltas && ChunkDeltas->Num() > 0;
}

void AFPMWorldChunkManager::ResetAllTerraforming() {
  // Collect affected chunks before clearing
  TArray<FFPMChunkCoord> AffectedCoords;
  VoxelOverlays.GetKeys(AffectedCoords);

  VoxelOverlays.Empty();
  ChunkTerraformOverlays.Empty();

  for (TPair<FIntVector, AActor *> &Entry : TerraformTileActors) {
    if (IsValid(Entry.Value)) {
      Entry.Value->Destroy();
    }
  }
  TerraformTileActors.Empty();
  PendingTilesToGenerate.Empty();
  PendingChunkClips.Empty();
  InFlightTerraformTiles.Empty();

  // Re-mesh affected chunks that are still loaded
  int32 RegenCount = 0;
  for (const FFPMChunkCoord &Coord : AffectedCoords) {
    if (LoadedChunks.Contains(Coord)) {
      RegenerateChunk(Coord);
      RegenCount++;
    }
  }

  UE_LOG(LogTemp, Warning,
         TEXT("FPM Terraform: Reset all modifications. Regenerated %d chunks."),
         RegenCount);
}

EFPMBiome AFPMWorldChunkManager::GetBiomeAtWorldPos(FVector WorldPos) {
  const FFPMChunkCoord Coord = FPMChunkGenerator::WorldToChunkCoord(WorldPos);

  // Check if the chunk is loaded
  AFPMChunkActor **FoundActor = LoadedChunks.Find(Coord);
  if (FoundActor && *FoundActor) {
    const FFPMChunkHeightmapData &Data = (*FoundActor)->GetChunkData();
    if (Data.bIsValid) {
      // Find the closest vertex in the chunk (hex bounding box UV)
      const FVector HexCenter = FPMChunkGenerator::ChunkToWorldCenter(Coord);
      const float LocalX = ((WorldPos.X - HexCenter.X) /
                                (FPMChunkConstants::ChunkWorldSize * 0.5f) +
                            1.0f) *
                           0.5f;
      const float LocalY = ((WorldPos.Y - HexCenter.Y) /
                                (FPMChunkConstants::ChunkWorldSize * 0.5f) +
                            1.0f) *
                           0.5f;

      const int32 Res = FPMChunkConstants::ChunkResolution;
      const int32 IX =
          FMath::Clamp(FMath::FloorToInt(LocalX * (Res - 1)), 0, Res - 1);
      const int32 IY =
          FMath::Clamp(FMath::FloorToInt(LocalY * (Res - 1)), 0, Res - 1);

      return Data.BiomeValues[IY * Res + IX];
    }
  }

  // Chunk not loaded  generate on the fly (lightweight)
  float NormX, NormY;
  FPMChunkGenerator::WorldToIslandNorm(WorldPos, NormX, NormY);

  // Quick biome check without full chunk generation
  const float Mask = 1.0f; // Simplified  would need IslandMask for accuracy
  // For now, return a default
  return EFPMBiome::Meadows;
}

void AFPMWorldChunkManager::ForceChunkUpdate() {
  bInitialLoadDone = false;
  TimeSinceLastUpdate = UpdateInterval;
}

void AFPMWorldChunkManager::EnsureChunkLoadedAtWorldPos(FVector WorldPos) {
  const FFPMChunkCoord Center = FPMChunkGenerator::WorldToChunkCoord(WorldPos);

  // Startup policy:
  // 1) Load the center/near chunks synchronously so terrain under the player
  //    is available immediately.
  // 2) Queue outer-ring chunks for async generation so startup does not hitch.
  constexpr int32 ForceLoadRadius = 3;
  constexpr int32 SyncLoadRadius = 1;
  int32 SyncChunksLoaded = 0;
  int32 AsyncChunksQueued = 0;

  TArray<FFPMChunkCoord> OrderedCoords;
  OrderedCoords.Reserve((ForceLoadRadius * 2 + 1) * (ForceLoadRadius * 2 + 1));

  for (int32 DQ = -ForceLoadRadius; DQ <= ForceLoadRadius; ++DQ) {
    for (int32 DR = -ForceLoadRadius; DR <= ForceLoadRadius; ++DR) {
      const int32 WQ = FPMChunkConstants::WrapChunkCoord(Center.Q + DQ);
      const int32 WR = FPMChunkConstants::WrapChunkCoord(Center.R + DR);
      OrderedCoords.AddUnique(FFPMChunkCoord(WQ, WR));
    }
  }

  // Always process nearest chunks first (center first, then rings outward).
  OrderedCoords.Sort(
      [&Center](const FFPMChunkCoord &A, const FFPMChunkCoord &B) {
        const int32 DistA = FFPMChunkCoord::WrappedHexDistance(A, Center);
        const int32 DistB = FFPMChunkCoord::WrappedHexDistance(B, Center);
        if (DistA != DistB) {
          return DistA < DistB;
        }
        // Deterministic tie-breaker.
        if (A.Q != B.Q) {
          return A.Q < B.Q;
        }
        return A.R < B.R;
      });

  for (const FFPMChunkCoord &Coord : OrderedCoords) {
    const int32 Dist = FFPMChunkCoord::WrappedHexDistance(Coord, Center);

    if (AFPMChunkActor *const *Found = LoadedChunks.Find(Coord)) {
      AFPMChunkActor *Existing = *Found;
      if (Existing && Existing->GetCurrentLOD() != EFPMChunkLOD::Full) {
        Existing->SetChunkLOD(EFPMChunkLOD::Full);
      }
      continue;
    }

    if (Dist <= SyncLoadRadius) {
      // Force center/near chunks now, even if an async task is already in
      // flight. Duplicate completion is safely ignored in FinalizeChunk().
      ChunkLoadQueue.Remove(Coord);
      LoadChunkSync(Coord, EFPMChunkLOD::Full);
      ++SyncChunksLoaded;
      continue;
    }

    if (!ChunkLoadQueue.Contains(Coord) && !GInFlightCoords.Contains(Coord)) {
      ChunkLoadQueue.Add(Coord);
      ++AsyncChunksQueued;
    }
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Spawn preload center-first at (%d,%d): sync=%d "
              "(radius=%d), queued=%d (outer to radius=%d)"),
         Center.Q, Center.R, SyncChunksLoaded, SyncLoadRadius,
         AsyncChunksQueued, ForceLoadRadius);
}
void AFPMWorldChunkManager::PrepareSpawnAreaAtWorldPos(FVector WorldPos) {
  EnsureChunkLoadedAtWorldPos(WorldPos);

  // Keep a collision fallback under the player while the outer terrain and
  // remaining fine tiles continue to stream.
  BuildSafetyFloorAt(WorldPos, 250000.0f, 24, 300.0f);

  // Prime the fine replacement terrain immediately instead of waiting for the
  // throttled manager tick after spawn.
  UpdateTerraformPlayerBubble(WorldPos);

  // Make sure normal streaming continues from this location on the next tick.
  ForceChunkUpdate();
}

// ===================================================================
//  BuildSafetyFloorAt

void AFPMWorldChunkManager::BuildSafetyFloorAt(FVector WorldPos,
                                               float HalfExtentCm,
                                               int32 GridSteps, float SinkCm) {
  // Clamp GridSteps to something sane
  GridSteps = FMath::Clamp(GridSteps, 4, 64);

  // ---- Build heightfield vertices ----
  const float StepSize = (HalfExtentCm * 2.f) / (float)GridSteps;
  const float OriginX = WorldPos.X - HalfExtentCm;
  const float OriginY = WorldPos.Y - HalfExtentCm;
  const int32 VertRows = GridSteps + 1;

  TArray<FVector> Verts;
  TArray<int32> Tris;
  TArray<FVector> Normals;
  TArray<FVector2D> UVs;

  Verts.Reserve(VertRows * VertRows);
  Tris.Reserve(GridSteps * GridSteps * 6);
  Normals.Reserve(VertRows * VertRows);
  UVs.Reserve(VertRows * VertRows);

  for (int32 Row = 0; Row <= GridSteps; ++Row) {
    for (int32 Col = 0; Col <= GridSteps; ++Col) {
      const float WX = OriginX + Col * StepSize;
      const float WY = OriginY + Row * StepSize;
      const float WZ =
          FPMVoxelGenerator::TerrainSurfaceZ(WX, WY, WorldSeed) - SinkCm;

      Verts.Add(FVector(WX, WY, WZ));
      Normals.Add(FVector::UpVector);
      UVs.Add(FVector2D((float)Col / GridSteps, (float)Row / GridSteps));
    }
  }

  // ---- Build triangle indices (two tris per quad) ----
  for (int32 Row = 0; Row < GridSteps; ++Row) {
    for (int32 Col = 0; Col < GridSteps; ++Col) {
      const int32 BL = Row * VertRows + Col;
      const int32 BR = BL + 1;
      const int32 TL = BL + VertRows;
      const int32 TR = TL + 1;
      // Tri 1
      Tris.Add(BL);
      Tris.Add(TL);
      Tris.Add(BR);
      // Tri 2
      Tris.Add(BR);
      Tris.Add(TL);
      Tris.Add(TR);
    }
  }

  // ---- Create or reuse the ProceduralMeshComponent ----
  if (!SafetyFloorMesh || !IsValid(SafetyFloorMesh)) {
    SafetyFloorMesh = NewObject<UProceduralMeshComponent>(
        this, UProceduralMeshComponent::StaticClass(), TEXT("SafetyFloor"));
    SafetyFloorMesh->RegisterComponent();
    SafetyFloorMesh->AttachToComponent(
        GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    // Collision only  no rendering
    SafetyFloorMesh->SetVisibility(false);
    SafetyFloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SafetyFloorMesh->SetCollisionResponseToAllChannels(
        ECollisionResponse::ECR_Block);
    SafetyFloorMesh->bUseComplexAsSimpleCollision = false;
  }

  // Feed mesh data (section 0 = replace)
  SafetyFloorMesh->CreateMeshSection(
      0, Verts, Tris, Normals, UVs,
      TArray<FColor>(),           // vertex colours  none
      TArray<FProcMeshTangent>(), // tangents  none needed
      /*bCreateCollision=*/true);

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Safety floor built at (%.0f,%.0f)  %d%d grid, "
              "%.0fkm half-extent, %.0fcm below terrain"),
         WorldPos.X, WorldPos.Y, GridSteps, GridSteps, HalfExtentCm / 100000.f,
         SinkCm);
}

void AFPMWorldChunkManager::ClearSafetyFloor() {
  if (!SafetyFloorMesh || !IsValid(SafetyFloorMesh)) {
    return;
  }

  SafetyFloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  SafetyFloorMesh->ClearAllMeshSections();
}

FVector AFPMWorldChunkManager::GetPlayerPosition() const {
  if (!GetWorld()) {
    return FVector::ZeroVector;
  }

  // Try to get the player pawn position
  APlayerController *PC = GetWorld()->GetFirstPlayerController();
  if (PC) {
    APawn *Pawn = PC->GetPawn();
    if (Pawn) {
      return Pawn->GetActorLocation();
    }
  }

#if WITH_EDITOR
  // In editor, use the editor camera position
  if (GEditor) {
    FEditorViewportClient *ViewportClient =
        static_cast<FEditorViewportClient *>(
            GEditor->GetActiveViewport()
                ? GEditor->GetActiveViewport()->GetClient()
                : nullptr);
    if (ViewportClient) {
      return ViewportClient->GetViewLocation();
    }
  }
#endif

  return FVector::ZeroVector;
}

// ===================================================================
//  Debug Visualization
// ===================================================================

void AFPMWorldChunkManager::DrawDebugChunks() const {
  if (!GetWorld()) {
    return;
  }

  for (const auto &Pair : LoadedChunks) {
    const FFPMChunkCoord &Coord = Pair.Key;
    const AFPMChunkActor *Actor = Pair.Value;
    if (!Actor)
      continue;

    const FVector HexCenter = FPMChunkGenerator::ChunkToWorldCenter(Coord);

    // Color based on LOD
    FColor Color;
    switch (Actor->GetCurrentLOD()) {
    case EFPMChunkLOD::Full:
      Color = FColor::Green;
      break;
    case EFPMChunkLOD::Medium:
      Color = FColor::Yellow;
      break;
    case EFPMChunkLOD::Low:
      Color = FColor::Red;
      break;
    default:
      Color = FColor::White;
      break;
    }

    // Draw square chunk boundary (4 edges)
    const float CS = FPMChunkConstants::ChunkWorldSize;
    const float DebugZ = 1000.0f;
    const FVector Origin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
    const FVector P0 = Origin + FVector(0, 0, DebugZ);
    const FVector P1 = Origin + FVector(CS, 0, DebugZ);
    const FVector P2 = Origin + FVector(CS, CS, DebugZ);
    const FVector P3 = Origin + FVector(0, CS, DebugZ);
    DrawDebugLine(GetWorld(), P0, P1, Color, false, UpdateInterval + 0.1f, 0,
                  5.0f);
    DrawDebugLine(GetWorld(), P1, P2, Color, false, UpdateInterval + 0.1f, 0,
                  5.0f);
    DrawDebugLine(GetWorld(), P2, P3, Color, false, UpdateInterval + 0.1f, 0,
                  5.0f);
    DrawDebugLine(GetWorld(), P3, P0, Color, false, UpdateInterval + 0.1f, 0,
                  5.0f);

    // Draw chunk coordinate text
    DrawDebugString(GetWorld(), HexCenter + FVector(0, 0, DebugZ + 200),
                    Coord.ToString(), nullptr, Color, UpdateInterval + 0.1f);
  }
}

// ===================================================================
//  Water Plane
// ===================================================================

void AFPMWorldChunkManager::SpawnWaterPlane() {
  // Use UE's built-in Plane mesh (100x100 cm, centered)
  UStaticMesh *PlaneMesh =
      LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

  if (!PlaneMesh) {
    UE_LOG(LogTemp, Error, TEXT("FPM: Could not load Plane mesh for water!"));
    return;
  }

  // Create the mesh component attached to this actor
  WaterPlaneMesh = NewObject<UStaticMeshComponent>(
      this, UStaticMeshComponent::StaticClass(), TEXT("WaterPlane"));
  WaterPlaneMesh->SetStaticMesh(PlaneMesh);
  WaterPlaneMesh->SetMobility(EComponentMobility::Movable);
  WaterPlaneMesh->RegisterComponent();
  WaterPlaneMesh->AttachToComponent(
      GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

  // --- Size: cover entire starter island + 50% padding ---
  // The default Plane mesh is 100x100 cm.
  // StarterIslandWorldSize = 16 * 6400 = 102400 cm.
  // With 50% padding: 153600 cm. Scale factor = 153600 / 100 = 1536.
  constexpr float IslandCm = FPMChunkConstants::StarterIslandWorldSize;
  constexpr float Padding = IslandCm * 0.5f;
  constexpr float TotalSize = IslandCm + Padding * 2.0f;
  constexpr float ScaleFactor = TotalSize / 100.0f; // Plane is 100cm

  WaterPlaneMesh->SetWorldLocation(FVector(0.0f, 0.0f, WaterZHeight));
  WaterPlaneMesh->SetWorldScale3D(FVector(ScaleFactor, ScaleFactor, 1.0f));

  // --- Material ---
  if (WaterMaterial) {
    WaterPlaneMesh->SetMaterial(0, WaterMaterial);
  } else {
    // Use BasicShapeMaterial which has a "Color" vector parameter
    UMaterialInterface *BaseMat = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (BaseMat) {
      UMaterialInstanceDynamic *WaterMID =
          UMaterialInstanceDynamic::Create(BaseMat, this);
      // Teal blue-green ocean color
      WaterMID->SetVectorParameterValue(
          TEXT("Color"), FLinearColor(0.02f, 0.35f, 0.40f, 1.0f));
      WaterPlaneMesh->SetMaterial(0, WaterMID);
    } else {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Could not load BasicShapeMaterial for water"));
    }
  }

  // Enable collision so the player doesn't fall through the ocean.
  // Below-sea-level terrain IS generated, but the water surface acts
  // as a walkable floor over it (until swimming is implemented).
  WaterPlaneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  WaterPlaneMesh->SetCollisionResponseToAllChannels(
      ECollisionResponse::ECR_Overlap);

  // Cast shadows off  water doesn't cast shadows
  WaterPlaneMesh->SetCastShadow(false);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Water plane spawned at Z=%.0f, scale=%.0f"), WaterZHeight,
         ScaleFactor);
}

// ===================================================================
//  River Head (Spring) Generation
// ===================================================================

void AFPMWorldChunkManager::SpawnRiverHeadSources() {
  if (bRiverHeadsGenerated)
    return;
  bRiverHeadsGenerated = true;

  // Scan a grid across the island to find river ridge local maxima.
  // These represent natural spring headwaters.
  constexpr int32 GridRes = 40; // 40x40 sample grid
  constexpr float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  constexpr float CellSize = 1.0f / static_cast<float>(GridRes);
  constexpr float RiverThreshold = 0.4f; // Minimum RF to be a candidate
  constexpr float MaskThreshold = 0.3f;  // Must be solidly on land

  // Sample the river factor across the grid
  TArray<float> RFGrid;
  RFGrid.SetNumZeroed(GridRes * GridRes);
  for (int32 Y = 0; Y < GridRes; ++Y) {
    for (int32 X = 0; X < GridRes; ++X) {
      const float NormX = (X + 0.5f) * CellSize;
      const float NormY = (Y + 0.5f) * CellSize;
      RFGrid[Y * GridRes + X] =
          FPMChunkGenerator::RiverFactor(NormX, NormY, WorldSeed);
    }
  }

  // Find local maxima: cells where RF > all 8 neighbors AND > threshold
  for (int32 Y = 1; Y < GridRes - 1; ++Y) {
    for (int32 X = 1; X < GridRes - 1; ++X) {
      const float RF = RFGrid[Y * GridRes + X];
      if (RF < RiverThreshold)
        continue;

      // Check if local maximum
      bool bIsMax = true;
      for (int32 DY = -1; DY <= 1 && bIsMax; ++DY) {
        for (int32 DX = -1; DX <= 1 && bIsMax; ++DX) {
          if (DX == 0 && DY == 0)
            continue;
          if (RFGrid[(Y + DY) * GridRes + (X + DX)] >= RF)
            bIsMax = false;
        }
      }
      if (!bIsMax)
        continue;

      // Check island mask (must be on land)
      const float NormX = (X + 0.5f) * CellSize;
      const float NormY = (Y + 0.5f) * CellSize;
      const float WorldX =
          (NormX - 0.5f) * FPMChunkConstants::StarterIslandWorldSize;
      const float WorldY =
          (NormY - 0.5f) * FPMChunkConstants::StarterIslandWorldSize;
      const float Mask = FPMNoise::IslandMask(WorldX, WorldY, WorldSeed);
      if (Mask < MaskThreshold)
        continue;

      // Get terrain height at this position via trace
      float TerrainZ =
          WaterZHeight + 500.0f; // default above water (SeaLevel=0)
      {
        FHitResult Hit;
        FCollisionQueryParams Params;
        // Search from 10km altitude down to 5km below sea level
        const FVector TraceStart(WorldX, WorldY, 1000000.0f);
        const FVector TraceEnd(WorldX, WorldY, -500000.0f);
        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd,
                                                 ECC_WorldStatic, Params)) {
          TerrainZ = Hit.ImpactPoint.Z;
        }
      }

      // Only place springs above sea level
      if (TerrainZ < WaterZHeight)
        continue;

      const FVector SpringPos(WorldX, WorldY, TerrainZ);
      RiverHeadPositions.Add(SpringPos);

      // Spawn a water source actor at this position
      FActorSpawnParameters SpawnParams;
      SpawnParams.SpawnCollisionHandlingOverride =
          ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      AFPMWaterSource *Source = GetWorld()->SpawnActor<AFPMWaterSource>(
          AFPMWaterSource::StaticClass(), SpringPos, FRotator::ZeroRotator,
          SpawnParams);
      if (Source) {
        Source->FlowRate =
            30.0f + RF * 70.0f; // Stronger springs have more flow
        Source->bInfiniteSource = true;
        Source->SourceType = 0; // Spring
      }
    }
  }

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Generated %d river head springs across the island"),
         RiverHeadPositions.Num());
}

bool AFPMWorldChunkManager::GetNearestRiverHead(const FVector &WorldPos,
                                                FVector &OutPos,
                                                float &OutDist) const {
  if (RiverHeadPositions.Num() == 0)
    return false;

  float BestDist2 = FLT_MAX;
  int32 BestIdx = -1;
  for (int32 I = 0; I < RiverHeadPositions.Num(); ++I) {
    const float D2 = FVector::DistSquared(WorldPos, RiverHeadPositions[I]);
    if (D2 < BestDist2) {
      BestDist2 = D2;
      BestIdx = I;
    }
  }

  OutPos = RiverHeadPositions[BestIdx];
  OutDist = FMath::Sqrt(BestDist2);
  return true;
}

// ===================================================================
//  Flowing Water System
// ===================================================================

void AFPMWorldChunkManager::InitializeChunkWater(const FFPMChunkCoord &Coord) {
  // Generate a heightmap for water simulation terrain lookups.
  // Voxel chunks don't retain their heightmap, so we generate a fresh one.
  if (!CachedHeightmaps.Contains(Coord)) {
    FFPMChunkHeightmapData Heightmap;
    FPMChunkGenerator::GenerateChunk(Coord, WorldSeed, Heightmap);
    CachedHeightmaps.Add(Coord, MoveTemp(Heightmap));
  }

  const FFPMChunkHeightmapData &Heightmap = CachedHeightmaps[Coord];

  // Place procedural water sources (mountain/snow/alpine biome springs)
  TArray<FFPMWaterSourceDef> Sources;
  FPMWaterSimulation::PlaceProceduralSources(Coord, WorldSeed, Heightmap,
                                             Sources);

  // Also register any river head positions that fall within this chunk.
  // These are spawned by SpawnRiverHeadSources() and represent the ridge-
  // noise local maxima  the headwaters of the carved river network.
  {
    const FVector ChunkCenter = FPMChunkGenerator::ChunkToWorldCenter(Coord);
    const float HalfChunk = FPMChunkConstants::ChunkWorldSize * 0.5f;
    const int32 WRes = FPMWaterConstants::WaterResolution;

    for (const FVector &SpringPos : RiverHeadPositions) {
      // Quick AABB check: is this spring within this chunk's bounding box?
      if (FMath::Abs(SpringPos.X - ChunkCenter.X) > HalfChunk ||
          FMath::Abs(SpringPos.Y - ChunkCenter.Y) > HalfChunk) {
        continue;
      }

      // Convert world position to water grid cell
      const float LocalX = SpringPos.X - (ChunkCenter.X - HalfChunk);
      const float LocalY = SpringPos.Y - (ChunkCenter.Y - HalfChunk);
      const int32 WX = FMath::Clamp(
          FMath::FloorToInt(LocalX / FPMChunkConstants::ChunkWorldSize *
                            (WRes - 1)),
          0, WRes - 1);
      const int32 WY = FMath::Clamp(
          FMath::FloorToInt(LocalY / FPMChunkConstants::ChunkWorldSize *
                            (WRes - 1)),
          0, WRes - 1);

      FFPMWaterSourceDef Src;
      Src.CellIndex = WY * WRes + WX;
      Src.FlowRate = 80.0f; // River head springs have strong flow
      Src.Type = EFPMWaterSourceType::Spring;
      Src.bInfinite = true;

      Sources.Add(Src);

      UE_LOG(LogTemp, Log,
             TEXT("FPM Water: Registered river head spring in chunk %s at "
                  "cell (%d,%d), WorldPos=%s"),
             *Coord.ToString(), WX, WY, *SpringPos.ToString());
    }
  }

  if (Sources.Num() > 0) {
    // Allocate water data for this chunk
    FFPMChunkWaterData WaterData;
    WaterData.Coord = Coord;
    WaterData.Allocate();

    ChunkWaterData.Add(Coord, MoveTemp(WaterData));
    ChunkWaterSources.Add(Coord, MoveTemp(Sources));

    UE_LOG(LogTemp, Log,
           TEXT("FPM Water: Chunk %s  %d sources, water data allocated"),
           *Coord.ToString(), ChunkWaterSources[Coord].Num());
  }

  // Also allocate water data for chunks neighboring a source chunk
  // (water will flow into them)
  for (int32 Dir = 0; Dir < FFPMChunkCoord::NumDirections; ++Dir) {
    const FFPMChunkCoord NCoord = Coord.Neighbor(Dir);
    if (ChunkWaterSources.Contains(Coord) && !ChunkWaterData.Contains(NCoord) &&
        LoadedChunks.Contains(NCoord)) {
      // Pre-allocate neighbor water data
      if (!CachedHeightmaps.Contains(NCoord)) {
        FFPMChunkHeightmapData NH;
        FPMChunkGenerator::GenerateChunk(NCoord, WorldSeed, NH);
        CachedHeightmaps.Add(NCoord, MoveTemp(NH));
      }

      FFPMChunkWaterData NWater;
      NWater.Coord = NCoord;
      NWater.Allocate();
      ChunkWaterData.Add(NCoord, MoveTemp(NWater));
    }
  }
}

void AFPMWorldChunkManager::TickWaterSimulation(float DeltaTime) {
  if (ChunkWaterData.Num() == 0) {
    return;
  }

  // 1. Inject water from sources
  for (auto &Pair : ChunkWaterSources) {
    const FFPMChunkCoord &Coord = Pair.Key;
    FFPMChunkWaterData *WaterData = ChunkWaterData.Find(Coord);
    if (WaterData && WaterData->bAllocated) {
      FPMWaterSimulation::InjectSources(*WaterData, Pair.Value, DeltaTime);
    }
  }

  // 2. Run per-chunk pipe model simulation
  for (auto &Pair : ChunkWaterData) {
    FFPMChunkWaterData &Water = Pair.Value;
    if (!Water.bAllocated) {
      continue;
    }

    const FFPMChunkHeightmapData *Heightmap = CachedHeightmaps.Find(Pair.Key);
    if (!Heightmap) {
      continue;
    }

    FPMWaterSimulation::SimulateChunk(Water, *Heightmap, DeltaTime);
  }

  // 3. Resolve cross-chunk flow
  FPMWaterSimulation::ResolveCrossChunkFlow(ChunkWaterData);

  // 4. Compute flow vectors for rendering
  for (auto &Pair : ChunkWaterData) {
    if (Pair.Value.bMeshDirty) {
      FPMWaterSimulation::ComputeFlowVectors(Pair.Value);
    }
  }

  // 5. Expand water data to neighbor chunks that don't have it yet
  //    (water reaching chunk edges needs somewhere to flow)
  //    PERF: limit to 1 expansion per tick to avoid generating heightmaps
  //    during the simulation tick (very expensive).
  TArray<FFPMChunkCoord> NewWaterChunks;
  for (auto &Pair : ChunkWaterData) {
    if (!Pair.Value.bHasWater) {
      continue;
    }

    const int32 WRes = FPMWaterConstants::WaterResolution;
    const FFPMChunkCoord &Coord = Pair.Key;

    // Check if water is touching the edges
    bool bEdgeWater = false;
    for (int32 i = 0; i < WRes && !bEdgeWater; ++i) {
      if (Pair.Value.WaterDepth[i] > 0.01f)
        bEdgeWater = true; // North
      if (Pair.Value.WaterDepth[(WRes - 1) * WRes + i] > 0.01f)
        bEdgeWater = true; // South
      if (Pair.Value.WaterDepth[i * WRes] > 0.01f)
        bEdgeWater = true; // West
      if (Pair.Value.WaterDepth[i * WRes + WRes - 1] > 0.01f)
        bEdgeWater = true; // East
    }

    if (bEdgeWater) {
      // Check cardinal neighbors (N, E, S, W)
      const FFPMChunkCoord CardinalNeighbors[4] = {
          FFPMChunkCoord(Coord.Q, Coord.R - 1), // N
          FFPMChunkCoord(Coord.Q + 1, Coord.R), // E
          FFPMChunkCoord(Coord.Q, Coord.R + 1), // S
          FFPMChunkCoord(Coord.Q - 1, Coord.R), // W
      };

      for (int32 D = 0; D < 4; ++D) {
        const FFPMChunkCoord &NC = CardinalNeighbors[D];
        if (!ChunkWaterData.Contains(NC) && LoadedChunks.Contains(NC)) {
          NewWaterChunks.AddUnique(NC);
        }
      }
    }

    // PERF: limit expansions to avoid blowing up the tick
    if (NewWaterChunks.Num() >= 1) {
      break;
    }
  }

  // Allocate water data for newly reached chunks (max 1 per tick)
  const int32 MaxExpansionsPerTick = 1;
  for (int32 i = 0; i < FMath::Min(NewWaterChunks.Num(), MaxExpansionsPerTick);
       ++i) {
    const FFPMChunkCoord &NC = NewWaterChunks[i];
    if (!CachedHeightmaps.Contains(NC)) {
      FFPMChunkHeightmapData NH;
      FPMChunkGenerator::GenerateChunk(NC, WorldSeed, NH);
      CachedHeightmaps.Add(NC, MoveTemp(NH));
    }

    FFPMChunkWaterData NWater;
    NWater.Coord = NC;
    NWater.Allocate();
    ChunkWaterData.Add(NC, MoveTemp(NWater));

    UE_LOG(LogTemp, Verbose, TEXT("FPM Water: Expanded water grid to chunk %s"),
           *NC.ToString());
  }
}

void AFPMWorldChunkManager::RebuildDirtyWaterMeshes() {
  // PERF: Limit rebuilds per call to avoid GPU stalls
  int32 RebuildsThisTick = 0;
  static constexpr int32 MaxMeshRebuildsPerTick = 2;

  for (auto &Pair : ChunkWaterData) {
    FFPMChunkWaterData &Water = Pair.Value;
    if (!Water.bMeshDirty || !Water.bAllocated) {
      continue;
    }

    if (RebuildsThisTick >= MaxMeshRebuildsPerTick) {
      break; // Defer remaining rebuilds to next call
    }
    ++RebuildsThisTick;

    const FFPMChunkCoord &Coord = Pair.Key;
    AFPMChunkActor **ChunkActor = LoadedChunks.Find(Coord);
    if (!ChunkActor || !*ChunkActor) {
      continue;
    }

    const FFPMChunkHeightmapData *Heightmap = CachedHeightmaps.Find(Coord);
    if (!Heightmap) {
      continue;
    }

    // Find or create the water mesh component on this chunk actor
    UProceduralMeshComponent *WaterMeshComp = nullptr;
    TArray<UProceduralMeshComponent *> PMCs;
    (*ChunkActor)->GetComponents<UProceduralMeshComponent>(PMCs);

    // The first PMC is terrain; the second (if it exists) is water
    if (PMCs.Num() >= 2) {
      WaterMeshComp = PMCs[1];
    } else {
      // Create a new PMC for water
      WaterMeshComp = NewObject<UProceduralMeshComponent>(
          *ChunkActor, UProceduralMeshComponent::StaticClass(),
          TEXT("WaterMesh"));
      WaterMeshComp->SetupAttachment((*ChunkActor)->GetRootComponent());
      WaterMeshComp->RegisterComponent();
      WaterMeshComp->SetCastShadow(false);
      WaterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

      // Apply water material if available
      if (WaterMaterial) {
        WaterMeshComp->SetMaterial(0, WaterMaterial);
      } else {
        // Create a basic translucent blue material
        UMaterialInterface *BaseMat = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (BaseMat) {
          UMaterialInstanceDynamic *WaterMID =
              UMaterialInstanceDynamic::Create(BaseMat, *ChunkActor);
          WaterMID->SetVectorParameterValue(
              TEXT("Color"), FLinearColor(0.05f, 0.40f, 0.55f, 0.7f));
          WaterMeshComp->SetMaterial(0, WaterMID);
        }
      }
    }

    if (WaterMeshComp) {
      FPMWaterMeshBuilder::UpdateWaterMeshComponent(WaterMeshComp, Water,
                                                    *Heightmap, Coord);
    }

    Water.bMeshDirty = false;
  }
}

void AFPMWorldChunkManager::CleanupChunkWater(const FFPMChunkCoord &Coord) {
  ChunkWaterData.Remove(Coord);
  ChunkWaterSources.Remove(Coord);
  CachedHeightmaps.Remove(Coord);

  // The water PMC is attached to the ChunkActor and will be destroyed
  // when the actor is destroyed  no manual cleanup needed.
}







