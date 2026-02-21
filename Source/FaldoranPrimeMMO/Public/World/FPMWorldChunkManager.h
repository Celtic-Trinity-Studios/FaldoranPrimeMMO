// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/FPMBiomePCGConfig.h"
#include "World/FPMChunkActor.h"
#include "World/FPMChunkData.h"
#include "World/FPMChunkOverlay.h"
#include "FPMWorldChunkManager.generated.h"


/**
 * AFPMWorldChunkManager
 *
 * The brain of the chunk system. Placed once in the level, it:
 *   - Tracks the player's position each tick
 *   - Determines which chunks should be loaded/unloaded
 *   - Generates chunk data (procedural heightmap + biome)
 *   - Applies change overlays (player modifications)
 *   - Spawns/despawns AFPMChunkActor instances
 *   - Manages LOD transitions
 *   - Spawns a water plane at sea level
 *   - Propagates BiomePCGConfig to chunks for vegetation spawning
 *
 * DESIGN PHILOSOPHY:
 *   - Chunks are NEVER stored permanently - they are regenerated from seed
 *   - Only player modifications (overlays) are persisted
 *   - The same seed always produces the same terrain
 *   - Chunk loading is spread across frames to avoid hitches
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMWorldChunkManager : public AActor {
  GENERATED_BODY()

public:
  AFPMWorldChunkManager();

  /** Find or spawn the WorldChunkManager for the given world.
   *  Guarantees exactly one per UWorld. Assets loaded from known paths. */
  static AFPMWorldChunkManager *GetOrCreate(UWorld *World);

  virtual void BeginPlay() override;
  virtual void Tick(float DeltaTime) override;

  // --- Configuration (set in Blueprint or level instance) ---

  /** World seed for procedural generation (replicated from server) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated,
            Category = "FPM|World")
  int32 WorldSeed = 42;

  /** If true, generate a random seed each time Play is pressed */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|World")
  bool bRandomizeSeed = false;

  /** Maximum chunks to generate per frame (prevents hitching) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|World")
  int32 MaxChunksPerFrame = 1;

  /** How often to check for chunk updates (seconds) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|World")
  float UpdateInterval = 0.25f;

  /** Material to apply to all chunk terrain meshes */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|World")
  UMaterialInterface *TerrainMaterial;

  // --- Water Configuration ---

  /** World-space Z height for the water plane. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  float WaterZHeight = 400.0f;

  /** Optional material for the water plane. If null, a default translucent
   *  blue material is created at runtime. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  UMaterialInterface *WaterMaterial;

  // --- Biome Population (PCG) ---

  /** Data asset configuring which meshes to spawn for trees/rocks.
   *  Create via: Content Browser -> Add -> Data Asset -> FPMBiomePCGConfig.
   *  If null, chunks will have terrain but no vegetation. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|PCG")
  UFPMBiomePCGConfig *BiomePCGConfig = nullptr;

  /** Enable debug visualization of chunk boundaries */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Debug")
  bool bDrawDebugChunkBounds = false;

  // --- Blueprint-callable Functions ---

  /** Force regenerate all loaded chunks (e.g. after seed change) */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  void RegenerateAllChunks();

  /** Get the number of currently loaded chunks */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  int32 GetLoadedChunkCount() const { return LoadedChunks.Num(); }

  /** Get the biome at a world position */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  EFPMBiome GetBiomeAtWorldPos(FVector WorldPos);

  /** Manually trigger a chunk update (useful for editor testing) */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  void ForceChunkUpdate();

  /** Immediately load the chunk (and neighbors) at a world position with
   * collision. Call this BEFORE spawning a player to prevent fall-through. */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  void EnsureChunkLoadedAtWorldPos(FVector WorldPos);

protected:
  // --- Internal State ---

  /** Map of currently loaded chunk actors, keyed by chunk coordinate.
   *  Transient: each side spawns its own chunks from the shared seed. */
  UPROPERTY(Transient)
  TMap<FFPMChunkCoord, AFPMChunkActor *> LoadedChunks;

  /** Cached overlays for loaded chunks */
  TMap<FFPMChunkCoord, FFPMChunkOverlay> LoadedOverlays;

  /** Queue of chunks waiting to be generated/loaded */
  TArray<FFPMChunkCoord> ChunkLoadQueue;

  /** Queue of chunks waiting to be unloaded */
  TArray<FFPMChunkCoord> ChunkUnloadQueue;

  /** Last chunk coord the player was in (to detect movement) */
  FFPMChunkCoord LastPlayerChunk;

  /** Timer for throttled updates */
  float TimeSinceLastUpdate = 0.0f;

  /** Whether initial load has completed */
  bool bInitialLoadDone = false;

  /** The spawned water plane mesh component (local only, transient) */
  UPROPERTY(Transient)
  UStaticMeshComponent *WaterPlaneMesh = nullptr;

private:
  /** Spawn the water plane covering the starter island at WaterZHeight. */
  void SpawnWaterPlane();

  /**
   * Gather the set of chunks that should be loaded based on player position.
   * @param PlayerChunk The chunk the player is currently in
   * @param OutDesiredChunks Set of all chunks that should be loaded
   */
  void GatherDesiredChunks(const FFPMChunkCoord &PlayerChunk,
                           TSet<FFPMChunkCoord> &OutDesiredChunks) const;

  /**
   * Determine the LOD level for a chunk based on distance from player.
   * @param ChunkCoord The chunk to evaluate
   * @param PlayerChunk The player's current chunk
   * @return Appropriate LOD level
   */
  EFPMChunkLOD DetermineLOD(const FFPMChunkCoord &ChunkCoord,
                            const FFPMChunkCoord &PlayerChunk) const;

  /**
   * Load a single chunk: generate data, apply overlay, spawn actor.
   * @param Coord The chunk to load
   * @param LOD LOD level to initialize at
   */
  void LoadChunk(const FFPMChunkCoord &Coord, EFPMChunkLOD LOD);

  /**
   * Unload a single chunk: despawn actor, free memory.
   * @param Coord The chunk to unload
   */
  void UnloadChunk(const FFPMChunkCoord &Coord);

  /**
   * Process the load/unload queues, limited by MaxChunksPerFrame.
   */
  void ProcessQueues();

  /**
   * Get the player's current world position.
   * @return Player position, or zero vector if no player
   */
  FVector GetPlayerPosition() const;

  /**
   * Draw debug visualization for chunk boundaries.
   */
  void DrawDebugChunks() const;

  // --- Console Commands ---
  static FAutoConsoleCommand CmdGenerateWorld;
  static FAutoConsoleCommand CmdRegenChunks;

  void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;
};
