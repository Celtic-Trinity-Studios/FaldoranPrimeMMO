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
static constexpr int32 GMaxFinalizationsPerTick = 6;

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

// ===================================================================
//  Console Commands
// ===================================================================

// Pointer to the active manager instance for console commands
static AFPMWorldChunkManager *GActiveChunkManager = nullptr;

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

  // 3. Spawn a new one — only reachable if none exists yet
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
           TEXT("FPM: Duplicate WorldChunkManager detected — destroying self. "
                "Active WCM: %s"),
           *GActiveChunkManager->GetName());
    Destroy();
    return;
  }

  GActiveChunkManager = this;

  // =================================================================
  //  Load settings from Config/WorldGen.ini
  //  Edit that file and restart PIE to iterate — no recompile needed.
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
                 TEXT("FPM: Seed=0 in INI → randomized to %d"), WorldSeed);
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
             TEXT("FPM: Flowing water system ENABLED — SimRate=%.0fHz, "
                  "Evap=%.4f, MaxHops=%d"),
             FPMWaterConstants::SimulationRate,
             FPMWaterConstants::EvaporationRate,
             FPMWaterConstants::MaxFlowHops);
    } else {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: Flowing water DISABLED — using flat water plane only"));
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

  // --- TOROIDAL WRAP: keep player inside [0, PlanetCircumference) ---
  {
    APawn *Pawn = PC->GetPawn();
    if (Pawn) {
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

  TimeSinceLastUpdate += DeltaTime;

  // Throttled update check
  if (TimeSinceLastUpdate >= UpdateInterval) {
    TimeSinceLastUpdate = 0.0f;

    const FVector PlayerPos = GetPlayerPosition();
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
             TEXT("FPM: Chunk update at %s — Desired=%d, NewlyQueued=%d, "
                  "LoadQ=%d, Loaded=%d"),
             *PlayerChunk.ToString(), DesiredChunks.Num(), NewlyQueued,
             ChunkLoadQueue.Num(), LoadedChunks.Num());

      // Sort load queue by hex distance DESCENDING (farthest first)
      // so that Pop() — which removes from the back — returns the
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
        return DistA > DistB; // descending — farthest at front
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
             TEXT("FPM: Gap recovery — re-queued %d missing chunks "
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
  // Coords wrap toroidally — no bounds check needed.
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
  const int32 HexDist = FFPMChunkCoord::WrappedHexDistance(ChunkCoord, PlayerChunk);

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

  // 2. Finalize on this (game) thread
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
    // reads vertex colours — this ensures biome colours always show even
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
          // Priority 3: last resort — at least show something visible
          UMaterialInterface *Grid = Cast<UMaterialInterface>(StaticLoadObject(
              UMaterialInterface::StaticClass(), nullptr,
              TEXT("/Engine/EngineMaterials/WorldGridMaterial")));
          if (Grid)
            PMC->SetMaterial(0, Grid);
          UE_LOG(LogTemp, Warning,
                 TEXT("FPM: M_TerrainBiome not found — biome colours will "
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
             TEXT("FPM: ⚠ Chunk %s generated 0 vertices (invisible gap)!"),
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

    // Finalize on game thread (spawn actor, build PMC mesh)
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

  // Chunk not loaded — generate on the fly (lightweight)
  float NormX, NormY;
  FPMChunkGenerator::WorldToIslandNorm(WorldPos, NormX, NormY);

  // Quick biome check without full chunk generation
  const float Mask = 1.0f; // Simplified — would need IslandMask for accuracy
  // For now, return a default
  return EFPMBiome::Meadows;
}

void AFPMWorldChunkManager::ForceChunkUpdate() {
  bInitialLoadDone = false;
  TimeSinceLastUpdate = UpdateInterval;
}

void AFPMWorldChunkManager::EnsureChunkLoadedAtWorldPos(FVector WorldPos) {
  const FFPMChunkCoord Center = FPMChunkGenerator::WorldToChunkCoord(WorldPos);

  // Force-load a 3-ring square grid (7x7 = 49 chunks) around the spawn.
  // Synchronous - causes a brief loading pause (~1.7s at 35ms/chunk),
  // but guarantees the player has collision-ready terrain in every
  // direction before they can move.
  constexpr int32 ForceLoadRadius = 3;
  int32 ChunksLoaded = 0;

  for (int32 DQ = -ForceLoadRadius; DQ <= ForceLoadRadius; ++DQ) {
    for (int32 DR = -ForceLoadRadius; DR <= ForceLoadRadius; ++DR) {
      const int32 WQ = FPMChunkConstants::WrapChunkCoord(Center.Q + DQ);
      const int32 WR = FPMChunkConstants::WrapChunkCoord(Center.R + DR);
      const FFPMChunkCoord Coord(WQ, WR);

      if (LoadedChunks.Contains(Coord)) {
        AFPMChunkActor *Existing = LoadedChunks[Coord];
        if (Existing && Existing->GetCurrentLOD() != EFPMChunkLOD::Full) {
          Existing->SetChunkLOD(EFPMChunkLOD::Full);
        }
        continue;
      }

      LoadChunkSync(Coord, EFPMChunkLOD::Full);
      ChunksLoaded++;
    }
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Force-loaded %d chunks in %d-ring grid around (%d,%d)"),
         ChunksLoaded, ForceLoadRadius, Center.Q, Center.R);
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
    // Collision only — no rendering
    SafetyFloorMesh->SetVisibility(false);
    SafetyFloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SafetyFloorMesh->SetCollisionResponseToAllChannels(
        ECollisionResponse::ECR_Block);
    SafetyFloorMesh->bUseComplexAsSimpleCollision = false;
  }

  // Feed mesh data (section 0 = replace)
  SafetyFloorMesh->CreateMeshSection(
      0, Verts, Tris, Normals, UVs,
      TArray<FColor>(),           // vertex colours — none
      TArray<FProcMeshTangent>(), // tangents — none needed
      /*bCreateCollision=*/true);

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Safety floor built at (%.0f,%.0f) — %d×%d grid, "
              "%.0fkm half-extent, %.0fcm below terrain"),
         WorldPos.X, WorldPos.Y, GridSteps, GridSteps, HalfExtentCm / 100000.f,
         SinkCm);
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

  // Cast shadows off — water doesn't cast shadows
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
  // noise local maxima — the headwaters of the carved river network.
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
           TEXT("FPM Water: Chunk %s — %d sources, water data allocated"),
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
  // when the actor is destroyed — no manual cleanup needed.
}
