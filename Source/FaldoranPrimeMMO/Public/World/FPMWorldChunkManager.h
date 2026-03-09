// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/FPMBiomePCGConfig.h"
#include "World/FPMChunkActor.h"
#include "World/FPMChunkData.h"
#include "World/FPMChunkOverlay.h"
#include "World/FPMWaterChunkData.h"
#include "World/FPMWaterSimulation.h"
#include "FPMWorldChunkManager.generated.h"


struct FFPMVoxelMeshData;

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
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

  /** Auto-created fallback material used when TerrainMaterial is null.
   *  Reads mesh vertex colours so biome tints are always visible.
   *  Created once on first chunk spawn, reused for all subsequent chunks. */
  UPROPERTY(Transient)
  UMaterialInstanceDynamic *AutoTerrainMaterial = nullptr;

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

  // --- Flowing Water System ---

  /** Whether the flowing water simulation is active.
   *  When false, uses the legacy flat water plane only.
   *  Configurable via WorldGen.ini [WaterSimulation] bEnableFlowingWater */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  bool bEnableFlowingWater = true;

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

  /**
   * Build (or rebuild) a coarse safety-floor collision mesh centred on
   * WorldPos.  The mesh is a Grid x Grid heightfield sampled from
   * TerrainSurfaceZ, offset SinkCm below real terrain, so the player
   * always has something solid to land on even before chunks generate.
   *
   * Called automatically by the HUD biome-teleport, but also exposed
   * so Blueprint / other systems can call it.
   *
   * @param WorldPos  Centre of the area to cover.
   * @param HalfExtentCm  Half-size of the square coverage area (cm).
   * @param GridSteps Number of grid subdivisions per axis.
   * @param SinkCm    How far below real terrain to place the floor.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|World")
  void BuildSafetyFloorAt(FVector WorldPos,
                          float HalfExtentCm = 1000000.f, // 10 km half
                          int32 GridSteps = 20, float SinkCm = 500.f);

  // --- Terraforming ---

  /**
   * Modify voxel density in a sphere around a world-space point.
   * Positive Strength = dig (remove material), negative = fill (add material).
   * Re-meshes affected chunks immediately.
   *
   * @param WorldPos  Center of the terraforming sphere
   * @param Radius    Radius of effect in cm (default 200 = 2m)
   * @param Strength  Terraforming power: + = dig, - = fill (default 1.0)
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Terraform")
  void TerraformAtPoint(FVector WorldPos, float Radius = 200.f,
                        float Strength = 1.0f);

  /**
   * Execute a terraform operation via line trace from the player camera.
   * Finds the first terrain hit point and calls TerraformAtPoint there.
   *
   * @param Radius    Radius of effect in cm
   * @param Strength  Terraforming power: + = dig, - = fill
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Terraform")
  void TerraformFromCamera(float Radius = 200.f, float Strength = 1.0f);

  /**
   * Clear all terraforming modifications (reset to procedural terrain).
   * Re-meshes all affected chunks.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Terraform")
  void ResetAllTerraforming();

  /**
   * Get the density delta at a voxel position (for use by GenerateAndMesh).
   * Thread-safe read-only access.
   *
   * @param Coord     Chunk coordinate
   * @param VoxelKey  Quantized voxel position (grid-space integer coords)
   * @return Density delta (positive = more solid, negative = more air)
   */
  float GetVoxelDelta(const FFPMChunkCoord &Coord,
                      const FIntVector &VoxelKey) const;

  /** Check if a chunk has any voxel modifications. */
  bool HasVoxelDeltas(const FFPMChunkCoord &Coord) const;

protected:
  // --- Internal State ---

  /** Map of currently loaded chunk actors, keyed by chunk coordinate.
   *  Transient: each side spawns its own chunks from the shared seed. */
  UPROPERTY(Transient)
  TMap<FFPMChunkCoord, AFPMChunkActor *> LoadedChunks;

  /** Cached overlays for loaded chunks */
  TMap<FFPMChunkCoord, FFPMChunkOverlay> LoadedOverlays;

  /** 3D voxel density modifications per chunk (COARSE — legacy).
   *  Key: chunk coord ? inner map keyed by quantized voxel position.
   *  Values are density deltas — negative = dig (push toward air),
   *  positive = fill (push toward solid).
   *  Applied AFTER procedural density + cave noise. */
  TMap<FFPMChunkCoord, TMap<FIntVector, float>> VoxelOverlays;

  // --- Fine-grained terraform overlay (200cm resolution) ---

  /** Convert a world position to a terraform tile coordinate.
   *  Tiles are 64m (6400cm) cubes, keyed by integer tile indices. */
  static FIntVector WorldToTileCoord(const FVector &WorldPos);

  /** Fine-resolution voxel deltas per terraform tile.
   *  Key: tile coord (FIntVector) ? inner map of fine voxel keys ? delta.
   *  Fine voxel keys are quantized at TerraformVoxelSizeCm (200cm). */
  TMap<FIntVector, TMap<FIntVector, float>> FineTerraformOverlays;

  /** Spawned fine-resolution tile mesh actors. */
  UPROPERTY(Transient)
  TMap<FIntVector, AActor *> TerraformTileActors;

  /** Active 3D fine-tile replacement bubble around the player. */
  TSet<FIntVector> ActiveTerraformBubbleTiles;

  /** Last bubble center tile; used to avoid expensive per-tick rebuilds. */
  FIntVector LastTerraformBubbleCenter = FIntVector(INT32_MIN, INT32_MIN, INT32_MIN);
  bool bTerraformBubbleInitialized = false;


  /** Refresh fine-tile replacement bubble around player (2 tiles XYZ). */
  void UpdateTerraformPlayerBubble(const FVector &PlayerPos);

  /** Regenerate (or create) the mesh for a single terraform tile. */
  void RegenerateTerraformTile(const FIntVector &TileCoord,
                               bool bForceBaseSurface = false);

  /** Clip triangles from a coarse mesh that overlap active terraform tiles. */
  void ClipMeshForTerraformTiles(FFPMVoxelMeshData &MeshData,
                                 const FFPMChunkCoord &ChunkCoord);

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

  /**
   * Coarse heightfield collision mesh that acts as a safety floor.
   * Generated by BuildSafetyFloorAt() and rebuilt on each biome teleport.
   * Prevents the player falling through the world when destination chunks
   * haven't generated collision yet.
   */
  UPROPERTY(Transient)
  class UProceduralMeshComponent *SafetyFloorMesh = nullptr;

  // --- Flowing Water State ---

  /** Per-chunk water simulation data. Only allocated for chunks
   *  with water sources or active flow. */
  TMap<FFPMChunkCoord, FFPMChunkWaterData> ChunkWaterData;

  /** Per-chunk water source definitions (from procedural placement) */
  TMap<FFPMChunkCoord, TArray<FFPMWaterSourceDef>> ChunkWaterSources;

  /** Cached heightmap data for water simulation terrain lookups.
   *  Stored separately because voxel chunks don't retain heightmap data. */
  TMap<FFPMChunkCoord, FFPMChunkHeightmapData> CachedHeightmaps;

  /** Water simulation timer (accumulates until 1/SimulationRate) */
  float WaterSimTimer = 0.0f;
  float WaterMeshRebuildTimer = 0.0f;

  /** Whether water sources have been initialized for loaded chunks */
  bool bWaterSourcesInitialized = false;

  /** Deterministic river head (spring) positions in world space */
  TArray<FVector> RiverHeadPositions;

  /** Whether river heads have been generated */
  bool bRiverHeadsGenerated = false;

public:
  /** Get the nearest river head position to a given world location.
   *  Returns true if found, and fills OutPos and OutDist. */
  bool GetNearestRiverHead(const FVector &WorldPos, FVector &OutPos,
                           float &OutDist) const;

  /** Get all river head positions (for debug display) */
  const TArray<FVector> &GetRiverHeads() const { return RiverHeadPositions; }

private:
  /** Spawn the water plane covering the starter island at WaterZHeight. */
  void SpawnWaterPlane();

  /** Generate and spawn water source actors at river head positions */
  void SpawnRiverHeadSources();

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
   * Load a single chunk synchronously (generate data + spawn actor).
   * @param Coord The chunk to load
   * @param LOD LOD level to initialize at
   */
  void LoadChunkSync(const FFPMChunkCoord &Coord, EFPMChunkLOD LOD);

  /**
   * Finalize a chunk on the game thread: spawn actor, feed mesh data.
   * Called when an async generation task completes, or from LoadChunkSync.
   */
  void FinalizeChunk(const FFPMChunkCoord &Coord, FFPMVoxelMeshData &&MeshData);

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
   * Poll completed async tasks and finalize them on the game thread.
   */
  void FinalizePendingChunks();

  /**
   * Get the player's current world position.
   * @return Player position, or zero vector if no player
   */
  FVector GetPlayerPosition() const;

  /**
   * Draw debug visualization for chunk boundaries.
   */
  void DrawDebugChunks() const;

  // --- Flowing Water Methods ---

  /**
   * Initialize water sources for a newly loaded chunk.
   * Places procedural sources and allocates water data.
   *
   * @param Coord The chunk to initialize water for
   */
  void InitializeChunkWater(const FFPMChunkCoord &Coord);

  /**
   * Run one tick of the water simulation across all active chunks.
   * Called at SimulationRate Hz, not every frame.
   *
   * @param DeltaTime Simulation time step
   */
  void TickWaterSimulation(float DeltaTime);

  /**
   * Rebuild water meshes for chunks with dirty water data.
   * Called after simulation to update visuals.
   */
  void RebuildDirtyWaterMeshes();

  /**
   * Clean up water data for an unloaded chunk.
   *
   * @param Coord The chunk being unloaded
   */
  void CleanupChunkWater(const FFPMChunkCoord &Coord);

  // --- Console Commands ---
  static FAutoConsoleCommand CmdGenerateWorld;
  static FAutoConsoleCommand CmdRegenChunks;
  static FAutoConsoleCommand CmdTerraformDig;
  static FAutoConsoleCommand CmdTerraformFill;
  static FAutoConsoleCommand CmdTerraformReset;

  /** Re-generate and re-mesh a single loaded chunk in place. */
  void RegenerateChunk(const FFPMChunkCoord &Coord);

  void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;
};
