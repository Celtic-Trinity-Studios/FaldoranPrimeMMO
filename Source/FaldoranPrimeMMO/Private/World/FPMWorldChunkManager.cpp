// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMWorldChunkManager.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

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
  if (!World) return nullptr;

  // If one already exists, return it
  for (TActorIterator<AFPMWorldChunkManager> It(World); It; ++It) {
    return *It;
  }

  // Spawn a new one
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
    if (Mat) WCM->TerrainMaterial = Mat;

    UFPMBiomePCGConfig *PCGConf = Cast<UFPMBiomePCGConfig>(
        StaticLoadObject(UFPMBiomePCGConfig::StaticClass(), nullptr,
                         TEXT("/Game/DA_BiomePCGConfig")));
    if (PCGConf) WCM->BiomePCGConfig = PCGConf;

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
  UE_LOG(LogTemp, Warning, TEXT("FPM: Chunk-based world system initialized"));
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Seed=%d, ChunkSize=%.0fm, Island=%d rings"),
         WorldSeed, FPMChunkConstants::ChunkWorldSize / 100.0f,
         FPMChunkConstants::StarterIslandRings);
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

  // Trigger initial chunk load on next tick
  TimeSinceLastUpdate = UpdateInterval;
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
              FFPMChunkCoord::HexDistance(It->Key, PlayerChunk);
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

  // Process queues every frame (throttled by MaxChunksPerFrame)
  ProcessQueues();

  // Debug visualization
  if (bDrawDebugChunkBounds) {
    DrawDebugChunks();
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
  for (int32 DQ = -Range; DQ <= Range; ++DQ) {
    for (int32 DR = -Range; DR <= Range; ++DR) {
      const FFPMChunkCoord Coord(PlayerChunk.Q + DQ, PlayerChunk.R + DR);

      // Skip chunks outside starter island bounds
      if (FFPMChunkCoord::HexDistance(Coord, FFPMChunkCoord(0, 0)) >
          FPMChunkConstants::StarterIslandRings) {
        continue;
      }

      OutDesiredChunks.Add(Coord);
    }
  }
}

// ===================================================================
//  LOD Determination
// ===================================================================

EFPMChunkLOD
AFPMWorldChunkManager::DetermineLOD(const FFPMChunkCoord &ChunkCoord,
                                    const FFPMChunkCoord &PlayerChunk) const {
  const int32 HexDist = FFPMChunkCoord::HexDistance(ChunkCoord, PlayerChunk);

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

void AFPMWorldChunkManager::LoadChunk(const FFPMChunkCoord &Coord,
                                      EFPMChunkLOD LOD) {
  if (LoadedChunks.Contains(Coord)) {
    return; // Already loaded
  }

  // 1. Generate voxel mesh via Marching Cubes (deterministic from seed)
  FFPMVoxelMeshData MeshData;
  FPMVoxelGenerator::GenerateAndMesh(Coord, WorldSeed, MeshData);

  // 2. Spawn chunk actor
  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  AFPMChunkActor *ChunkActor = GetWorld()->SpawnActor<AFPMChunkActor>(
      AFPMChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
      SpawnParams);

  if (ChunkActor) {
    // Pass biome configuration before initialization
    // so vegetation spawning has mesh data available
    if (BiomePCGConfig) {
      ChunkActor->SetBiomePCGConfig(BiomePCGConfig, WorldSeed);
    }

    // Initialize with voxel mesh data
    ChunkActor->InitializeVoxelChunk(MeshData, Coord);

    // Apply terrain material
    if (TerrainMaterial) {
      UProceduralMeshComponent *PMC =
          ChunkActor->FindComponentByClass<UProceduralMeshComponent>();
      if (PMC) {
        PMC->SetMaterial(0, TerrainMaterial);
      }
    }

    LoadedChunks.Add(Coord, ChunkActor);

    // Diagnostic: warn about empty chunks that will be invisible
    if (MeshData.Vertices.Num() == 0) {
      UE_LOG(LogTemp, Warning,
             TEXT("FPM: ⚠ Chunk %s generated 0 vertices (invisible gap)!"),
             *Coord.ToString());
    } else {
      UE_LOG(LogTemp, Log, TEXT("FPM: Loaded voxel chunk %s (%d verts)"),
             *Coord.ToString(), MeshData.Vertices.Num());
    }
  }
}

void AFPMWorldChunkManager::UnloadChunk(const FFPMChunkCoord &Coord) {
  AFPMChunkActor **FoundActor = LoadedChunks.Find(Coord);
  if (FoundActor && *FoundActor) {
    // Save overlay if it has modifications (auto-persist)
    FFPMChunkOverlay *Overlay = LoadedOverlays.Find(Coord);
    if (Overlay && Overlay->HasModifications()) {
      FPMChunkOverlayManager::SaveOverlay(*Overlay);
    }

    (*FoundActor)->Destroy();
  }

  LoadedChunks.Remove(Coord);
  LoadedOverlays.Remove(Coord);

  UE_LOG(LogTemp, Verbose, TEXT("FPM: Unloaded chunk %s"), *Coord.ToString());
}

// ===================================================================
//  Queue Processing
// ===================================================================

void AFPMWorldChunkManager::ProcessQueues() {
  int32 ChunksProcessed = 0;

  // Throttle unloads -- max 4 per frame
  int32 UnloadsProcessed = 0;
  while (ChunkUnloadQueue.Num() > 0 && UnloadsProcessed < 4) {
    const FFPMChunkCoord Coord = ChunkUnloadQueue.Pop();
    UnloadChunk(Coord);
    UnloadsProcessed++;
  }

  // TIME-BASED budget: each voxel chunk takes 25-40ms to generate.
  // A fixed chunk count (e.g. 16) caused 400-600ms frames (2 FPS).
  // Instead, allow chunk loading to consume at most N milliseconds
  // per frame, ensuring consistent frame rate.
  //
  // Budget:  8ms steady-state (maintains 60fps during loading)
  //         16ms during heavy load (1-2 chunks, brief stutter OK)
  //         Each voxel chunk takes ~35ms, so usually 0-1 chunks per frame.
  const double BudgetMs = (ChunkLoadQueue.Num() > 50) ? 16.0 : 8.0;
  const double StartTime = FPlatformTime::Seconds();
  const double Deadline = StartTime + BudgetMs / 1000.0;

  // Loads -- time-throttled per frame.
  // Queue is sorted farthest-first so Pop() gives closest.
  while (ChunkLoadQueue.Num() > 0) {
    // Check time budget (always do at least 1 chunk per frame)
    if (ChunksProcessed > 0 && FPlatformTime::Seconds() >= Deadline) {
      break;
    }

    const FFPMChunkCoord Coord = ChunkLoadQueue.Pop();
    const EFPMChunkLOD LOD = DetermineLOD(Coord, LastPlayerChunk);
    LoadChunk(Coord, LOD);
    ChunksProcessed++;
  }

  // Log queue progress
  if (ChunksProcessed > 0) {
    const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    UE_LOG(LogTemp, Log,
           TEXT("FPM: Loaded %d chunks (%.1fms), %d remaining, %d total"),
           ChunksProcessed, ElapsedMs, ChunkLoadQueue.Num(),
           LoadedChunks.Num());
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
      const float LocalX =
          ((WorldPos.X - HexCenter.X) / (FPMChunkConstants::ChunkWorldSize * 0.5f) +
           1.0f) *
          0.5f;
      const float LocalY =
          ((WorldPos.Y - HexCenter.Y) / (FPMChunkConstants::ChunkWorldSize * 0.5f) +
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
      const FFPMChunkCoord Coord(Center.Q + DQ, Center.R + DR);

      if (FFPMChunkCoord::HexDistance(Coord, FFPMChunkCoord(0, 0)) >
          FPMChunkConstants::StarterIslandRings) {
        continue;
      }

      if (LoadedChunks.Contains(Coord)) {
        AFPMChunkActor *Existing = LoadedChunks[Coord];
        if (Existing && Existing->GetCurrentLOD() != EFPMChunkLOD::Full) {
          Existing->SetChunkLOD(EFPMChunkLOD::Full);
        }
        continue;
      }

      LoadChunk(Coord, EFPMChunkLOD::Full);
      ChunksLoaded++;
    }
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM: Force-loaded %d chunks in %d-ring grid around (%d,%d)"),
         ChunksLoaded, ForceLoadRadius, Center.Q, Center.R);
}
// ===================================================================
//  Player Position
// ===================================================================

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
    DrawDebugLine(GetWorld(), P0, P1, Color, false, UpdateInterval + 0.1f, 0, 5.0f);
    DrawDebugLine(GetWorld(), P1, P2, Color, false, UpdateInterval + 0.1f, 0, 5.0f);
    DrawDebugLine(GetWorld(), P2, P3, Color, false, UpdateInterval + 0.1f, 0, 5.0f);
    DrawDebugLine(GetWorld(), P3, P0, Color, false, UpdateInterval + 0.1f, 0, 5.0f);

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
  WaterPlaneMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  WaterPlaneMesh->SetCollisionProfileName(TEXT("BlockAll"));

  // Cast shadows off — water doesn't cast shadows
  WaterPlaneMesh->SetCastShadow(false);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Water plane spawned at Z=%.0f, scale=%.0f"), WaterZHeight,
         ScaleFactor);
}
