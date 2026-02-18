// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/FPMBiomePCGConfig.h"
#include "World/FPMChunkData.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
struct FFPMVoxelMeshData;

/**
 * FPMBiomePCGSpawner
 *
 * Stateless utility that populates a chunk with vegetation/rocks
 * using Hierarchical Instanced Static Mesh Components (HISM).
 *
 * NOT a UObject — pure C++ utility. Called by AFPMChunkActor after
 * mesh + collision are ready.
 *
 * Uses FPMVoxelGenerator::TerrainSurfaceZ() and BiomeAtWorldXY()
 * directly so the Z positions match the actual voxel terrain surface.
 *
 * Why HISM instead of UPCGComponent?
 *   - PCG Graphs are asset-heavy and require editor-side setup
 *   - HISM gives us identical draw-call batching and LOD benefits
 *   - Full C++ control over biome-aware density logic
 *   - Deterministic: same seed = same tree placement every time
 */
class FALDORANPRIMEMMO_API FPMBiomePCGSpawner {
public:
  /**
   * Populate a chunk actor with vegetation and rocks.
   *
   * @param OwnerActor       The chunk actor to attach HISM components to
   * @param Config           Biome spawn configuration (meshes, densities)
   * @param ChunkCoord       Grid coordinate of the chunk
   * @param WorldSeed        Global world seed for deterministic placement
   */
  static void
  PopulateChunk(AActor *OwnerActor, const UFPMBiomePCGConfig *Config,
                const FFPMChunkCoord &ChunkCoord, int32 WorldSeed,
                const FFPMVoxelMeshData *MeshData = nullptr,
                const struct FFPMChunkHeightmapData *HeightData = nullptr);

  /** Remove all spawned HISM components from a chunk actor. */
  static void ClearSpawnedInstances(AActor *OwnerActor);

private:
  /**
   * Generate scatter points for a given biome within the chunk.
   * Uses FPMVoxelGenerator::TerrainSurfaceZ for correct Z placement
   * and FPMVoxelGenerator::BiomeAtWorldXY for biome queries.
   * Points are LOCAL to the chunk actor (X,Y relative to chunk origin).
   *
   * @param ChunkCoord   Grid coordinate of the chunk
   * @param TargetBiome  Which biome this scatter is for
   * @param Count        Number of points to attempt
   * @param ChunkSeed    Per-chunk seed
   * @param WorldSeed    Global world seed for terrain queries
   * @param OutPoints    Output: local-space transforms on the mesh surface
   * @param ScaleRange   Min/Max uniform scale for instances
   */
  static void GenerateScatterPoints(const FFPMChunkCoord &ChunkCoord,
                                    EFPMBiome TargetBiome, int32 Count,
                                    int32 ChunkSeed, int32 WorldSeed,
                                    TArray<FTransform> &OutPoints,
                                    FVector2D ScaleRange);

  /**
   * Create an HISM component on the actor for a given mesh, then add
   * instances at each transform.
   */
  static UHierarchicalInstancedStaticMeshComponent *
  SpawnHISMInstances(AActor *OwnerActor, UStaticMesh *Mesh,
                     const TArray<FTransform> &Transforms, FName ComponentName);
};
