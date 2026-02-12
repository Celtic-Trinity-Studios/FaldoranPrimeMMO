// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMWorldChunkManager.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

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
  bReplicates = true;
  bAlwaysRelevant = true;
}

void AFPMWorldChunkManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(AFPMWorldChunkManager, WorldSeed);
}

// ===================================================================
//  BeginPlay
// ===================================================================

void AFPMWorldChunkManager::BeginPlay() {
  Super::BeginPlay();

  GActiveChunkManager = this;

  // Randomize seed if requested — only on server so all clients share it.
  // The seed is replicated via DOREPLIFETIME.
  if (bRandomizeSeed && HasAuthority()) {
    WorldSeed = FMath::RandRange(1, 999999);
    UE_LOG(LogTemp, Warning,
           TEXT("FPM: Randomized seed = %d (set bRandomizeSeed=false and "
                "WorldSeed=%d to keep this one)"),
           WorldSeed, WorldSeed);
  }

  // Initialize overlay system
  const FString SaveDir =
      FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChunkOverlays"));
  FPMChunkOverlayManager::Initialize(SaveDir);

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: ========================================"));
  UE_LOG(LogTemp, Warning, TEXT("FPM: Chunk-based world system initialized"));
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Seed=%d, ChunkSize=%.0fm, Island=%dx%d chunks"), WorldSeed,
         FPMChunkConstants::ChunkWorldSize / 100.0f,
         FPMChunkConstants::StarterIslandChunksPerAxis,
         FPMChunkConstants::StarterIslandChunksPerAxis);
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: LOD ranges: Full=%d, Medium=%d, Low=%d chunks"),
         FPMChunkConstants::FullDetailRange,
         FPMChunkConstants::MediumDetailRange,
         FPMChunkConstants::LowDetailRange);
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: ========================================"));

  // Trigger initial chunk load on next tick
  TimeSinceLastUpdate = UpdateInterval;
}

// ===================================================================
//  Tick
// ===================================================================

void AFPMWorldChunkManager::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  TimeSinceLastUpdate += DeltaTime;

  // Throttled update check
  if (TimeSinceLastUpdate >= UpdateInterval) {
    TimeSinceLastUpdate = 0.0f;

    const FVector PlayerPos = GetPlayerPosition();
    const FFPMChunkCoord PlayerChunk =
        FPMChunkGenerator::WorldToChunkCoord(PlayerPos);

    // Only do a full update if the player moved to a new chunk or it's the
    // first load
    if (PlayerChunk != LastPlayerChunk || !bInitialLoadDone) {
      LastPlayerChunk = PlayerChunk;
      bInitialLoadDone = true;

      // Determine desired chunk set
      TSet<FFPMChunkCoord> DesiredChunks;
      GatherDesiredChunks(PlayerChunk, DesiredChunks);

      // Queue chunks to unload (loaded but no longer desired)
      for (auto It = LoadedChunks.CreateIterator(); It; ++It) {
        if (!DesiredChunks.Contains(It->Key)) {
          ChunkUnloadQueue.AddUnique(It->Key);
        }
      }

      // Queue chunks to load (desired but not yet loaded)
      // Also check for LOD changes on already-loaded chunks
      for (const FFPMChunkCoord &Coord : DesiredChunks) {
        if (!LoadedChunks.Contains(Coord)) {
          ChunkLoadQueue.AddUnique(Coord);
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

      // Sort load queue by distance to player (closest first)
      ChunkLoadQueue.Sort([&PlayerChunk](const FFPMChunkCoord &A,
                                         const FFPMChunkCoord &B) {
        const int32 DistA =
            FMath::Abs(A.X - PlayerChunk.X) + FMath::Abs(A.Y - PlayerChunk.Y);
        const int32 DistB =
            FMath::Abs(B.X - PlayerChunk.X) + FMath::Abs(B.Y - PlayerChunk.Y);
        return DistA < DistB;
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

  for (int32 DY = -Range; DY <= Range; ++DY) {
    for (int32 DX = -Range; DX <= Range; ++DX) {
      const FFPMChunkCoord Coord(PlayerChunk.X + DX, PlayerChunk.Y + DY);

      // Only include chunks within the starter island bounds (0 to 15)
      // In the future, this restriction is removed for infinite worlds
      if (Coord.X < 0 ||
          Coord.X >= FPMChunkConstants::StarterIslandChunksPerAxis ||
          Coord.Y < 0 ||
          Coord.Y >= FPMChunkConstants::StarterIslandChunksPerAxis) {
        continue;
      }

      // Use circular distance check instead of square
      const float Dist = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));
      if (Dist <= static_cast<float>(Range)) {
        OutDesiredChunks.Add(Coord);
      }
    }
  }
}

// ===================================================================
//  LOD Determination
// ===================================================================

EFPMChunkLOD
AFPMWorldChunkManager::DetermineLOD(const FFPMChunkCoord &ChunkCoord,
                                    const FFPMChunkCoord &PlayerChunk) const {
  const int32 DX = FMath::Abs(ChunkCoord.X - PlayerChunk.X);
  const int32 DY = FMath::Abs(ChunkCoord.Y - PlayerChunk.Y);
  const float Dist = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));

  if (Dist <= static_cast<float>(FPMChunkConstants::FullDetailRange)) {
    return EFPMChunkLOD::Full;
  }
  if (Dist <= static_cast<float>(FPMChunkConstants::MediumDetailRange)) {
    return EFPMChunkLOD::Medium;
  }
  if (Dist <= static_cast<float>(FPMChunkConstants::LowDetailRange)) {
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

  // 1. Generate chunk data from seed (deterministic)
  FFPMChunkHeightmapData ChunkData;
  FPMChunkGenerator::GenerateChunk(Coord, WorldSeed, ChunkData);

  // 2. Load and apply overlay (player modifications)
  FFPMChunkOverlay Overlay;
  FPMChunkOverlayManager::LoadOverlay(Coord, Overlay);
  if (Overlay.HasModifications()) {
    FPMChunkOverlayManager::ApplyOverlay(Overlay, ChunkData);
  }
  LoadedOverlays.Add(Coord, Overlay);

  // 3. Spawn chunk actor
  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  AFPMChunkActor *ChunkActor = GetWorld()->SpawnActor<AFPMChunkActor>(
      AFPMChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
      SpawnParams);

  if (ChunkActor) {
    // Apply material if set
    if (TerrainMaterial &&
        ChunkActor->FindComponentByClass<UProceduralMeshComponent>()) {
      ChunkActor->FindComponentByClass<UProceduralMeshComponent>()->SetMaterial(
          0, TerrainMaterial);
    }

    ChunkActor->InitializeChunk(ChunkData, LOD);
    LoadedChunks.Add(Coord, ChunkActor);

    UE_LOG(LogTemp, Verbose, TEXT("FPM: Loaded chunk %s at LOD %d"),
           *Coord.ToString(), static_cast<int32>(LOD));
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

  // Unloads are cheap — do them all immediately
  for (const FFPMChunkCoord &Coord : ChunkUnloadQueue) {
    UnloadChunk(Coord);
  }
  ChunkUnloadQueue.Empty();

  // Loads are expensive — throttle per frame
  while (ChunkLoadQueue.Num() > 0 && ChunksProcessed < MaxChunksPerFrame) {
    const FFPMChunkCoord Coord = ChunkLoadQueue[0];
    ChunkLoadQueue.RemoveAt(0);

    const EFPMChunkLOD LOD = DetermineLOD(Coord, LastPlayerChunk);
    LoadChunk(Coord, LOD);
    ChunksProcessed++;
  }

  // Log queue status if there are pending loads
  if (ChunkLoadQueue.Num() > 0 && ChunksProcessed > 0) {
    UE_LOG(LogTemp, Verbose,
           TEXT("FPM: Processed %d chunks, %d remaining in queue"),
           ChunksProcessed, ChunkLoadQueue.Num());
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
      // Find the closest vertex in the chunk
      const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
      const float LocalX =
          (WorldPos.X - ChunkOrigin.X) / FPMChunkConstants::ChunkWorldSize;
      const float LocalY =
          (WorldPos.Y - ChunkOrigin.Y) / FPMChunkConstants::ChunkWorldSize;

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

  // Load the target chunk AND all 8 neighbors (3x3 grid) immediately
  // All at Full LOD with collision so the player has ground to stand on
  for (int32 DY = -1; DY <= 1; ++DY) {
    for (int32 DX = -1; DX <= 1; ++DX) {
      const FFPMChunkCoord Coord(Center.X + DX, Center.Y + DY);

      // Skip if out of island bounds
      if (Coord.X < 0 ||
          Coord.X >= FPMChunkConstants::StarterIslandChunksPerAxis ||
          Coord.Y < 0 ||
          Coord.Y >= FPMChunkConstants::StarterIslandChunksPerAxis) {
        continue;
      }

      // If already loaded but not at Full LOD, upgrade it
      if (LoadedChunks.Contains(Coord)) {
        AFPMChunkActor *Existing = LoadedChunks[Coord];
        if (Existing && Existing->GetCurrentLOD() != EFPMChunkLOD::Full) {
          Existing->SetChunkLOD(EFPMChunkLOD::Full);
        }
        continue;
      }

      LoadChunk(Coord, EFPMChunkLOD::Full);
    }
  }

  UE_LOG(
      LogTemp, Log,
      TEXT("FPM: Force-loaded 3x3 chunk area around (%d,%d) for player spawn"),
      Center.X, Center.Y);
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

    const FVector Origin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
    const float Size = FPMChunkConstants::ChunkWorldSize;

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

    // Draw chunk boundary box
    const FVector Center = Origin + FVector(Size * 0.5f, Size * 0.5f, 1000.0f);
    const FVector Extent(Size * 0.5f, Size * 0.5f, 100.0f);
    DrawDebugBox(GetWorld(), Center, Extent, Color, false,
                 UpdateInterval + 0.1f, 0, 5.0f);

    // Draw chunk coordinate text
    DrawDebugString(GetWorld(), Center + FVector(0, 0, 200), Coord.ToString(),
                    nullptr, Color, UpdateInterval + 0.1f);
  }
}
