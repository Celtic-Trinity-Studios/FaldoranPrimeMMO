// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "World/FPMChunkData.h"
#include "World/FPMVoxelChunk.h"

#include "FPMChunkActor.generated.h"

class UFPMBiomePCGConfig;

/**
 * AFPMChunkActor
 *
 * A single terrain chunk rendered via ProceduralMeshComponent.
 * Spawned/despawned by AFPMWorldChunkManager.
 *
 * Networking: Chunks are NOT replicated.  Both server and client run
 * their own WorldChunkManager and generate identical terrain from the
 * shared WorldSeed.  This avoids replication overhead for hundreds of
 * mesh actors.  The server only needs collision; the client needs visuals.
 *
 * Lifecycle:
 *   1. Manager spawns actor
 *   2. Manager calls InitializeChunk() with heightmap data + LOD
 *   3. BuildMesh() creates vertices, triangles, normals, collision
 *   4. PopulateBiome() spawns trees/rocks via HISM (Full LOD only)
 *   5. SetChunkLOD() changes detail level as player moves
 *   6. Manager destroys actor when out of range
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMChunkActor : public AActor {
  GENERATED_BODY()

public:
  AFPMChunkActor();

  /** Initialize with generated heightmap data at the given LOD. */
  void InitializeChunk(const FFPMChunkHeightmapData &InData,
                       EFPMChunkLOD InLOD);

  /** Initialize with pre-built voxel mesh data (Marching Cubes output). */
  void InitializeVoxelChunk(const FFPMVoxelMeshData &MeshData,
                            const FFPMChunkCoord &Coord);

  /** Switch LOD level. Rebuilds mesh at new resolution. */
  void SetChunkLOD(EFPMChunkLOD NewLOD);

  /** Get chunk grid coordinate. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Chunk")
  FFPMChunkCoord GetChunkCoord() const { return ChunkData.Coord; }

  /** Get current LOD. */
  EFPMChunkLOD GetCurrentLOD() const { return CurrentLOD; }

  /** True if this chunk was built with voxel (Marching Cubes) mesh. */
  bool IsVoxelChunk() const { return bIsVoxelChunk; }

  /** Get stored heightmap data (read-only). */
  const FFPMChunkHeightmapData &GetChunkData() const { return ChunkData; }

  /** Map a normalized height [0,1] to world-space Z (cm). */
  static float HeightToWorldZ(float NormalizedHeight);

  // ---- Biome Population ----

  /** Set the PCG config data asset (called by WorldChunkManager). */
  void SetBiomePCGConfig(const UFPMBiomePCGConfig *InConfig, int32 InWorldSeed);

private:
  /**
   * Build the procedural mesh from HeightValues at the given LOD.
   * @param LODStep  Vertex skip (1=full, 2=half, 4=quarter)
   * @param bCollision  Whether to generate per-poly collision
   */
  void BuildMesh(int32 LODStep, bool bCollision);

  /** Map a biome enum to a vertex color for material splatting. */
  static FColor BiomeToVertexColor(EFPMBiome Biome);

  /**
   * Trigger biome population (trees, rocks) after mesh + collision.
   * Only runs at Full LOD. Uses HISM components for GPU instancing.
   */
  void PopulateBiome();

  /** Clear all spawned biome instances (LOD change / unload). */
  void ClearBiome();

protected:
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPM|Chunk")
  UProceduralMeshComponent *TerrainMesh;

  EFPMChunkLOD CurrentLOD = EFPMChunkLOD::Unloaded;
  FFPMChunkHeightmapData ChunkData;

  /** Biome configuration data asset (set by WorldChunkManager) */
  const UFPMBiomePCGConfig *BiomePCGConfig = nullptr;

  /** Terrain material (loaded from M_ChunkTerrain asset). */
  UPROPERTY()
  UMaterialInterface *TerrainMaterial = nullptr;

  /** Cached world seed for deterministic placement */
  int32 CachedWorldSeed = 0;

  /** Whether biome instances have been spawned for this chunk */
  bool bBiomePopulated = false;

  /** True if chunk was initialized via InitializeVoxelChunk (Marching Cubes).
   *  Voxel chunks cannot be rebuilt by BuildMesh, so LOD transitions must
   *  keep the original mesh instead of clearing and regenerating. */
  bool bIsVoxelChunk = false;

  /** Cached voxel mesh data for accurate object placement Z lookup */
  FFPMVoxelMeshData CachedVoxelMesh;
};
