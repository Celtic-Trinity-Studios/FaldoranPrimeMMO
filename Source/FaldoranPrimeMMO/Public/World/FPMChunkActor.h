// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "World/FPMChunkData.h"

#include "FPMChunkActor.generated.h"

/**
 * AFPMChunkActor
 *
 * A single terrain chunk rendered via ProceduralMeshComponent.
 * Spawned/despawned by AFPMWorldChunkManager.
 *
 * Lifecycle:
 *   1. Manager spawns actor
 *   2. Manager calls InitializeChunk() with heightmap data + LOD
 *   3. BuildMesh() creates vertices, triangles, normals, collision
 *   4. SetChunkLOD() changes detail level as player moves
 *   5. Manager destroys actor when out of range
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMChunkActor : public AActor {
  GENERATED_BODY()

public:
  AFPMChunkActor();

  /** Initialize with generated heightmap data at the given LOD. */
  void InitializeChunk(const FFPMChunkHeightmapData &InData,
                       EFPMChunkLOD InLOD);

  /** Switch LOD level. Rebuilds mesh at new resolution. */
  void SetChunkLOD(EFPMChunkLOD NewLOD);

  /** Get chunk grid coordinate. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Chunk")
  FFPMChunkCoord GetChunkCoord() const { return ChunkData.Coord; }

  /** Get current LOD. */
  EFPMChunkLOD GetCurrentLOD() const { return CurrentLOD; }

  /** Get stored heightmap data (read-only). */
  const FFPMChunkHeightmapData &GetChunkData() const { return ChunkData; }

  /** Map a normalized height [0,1] to world-space Z (cm). */
  static float HeightToWorldZ(float NormalizedHeight);

protected:
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPM|Chunk")
  UProceduralMeshComponent *TerrainMesh;

  EFPMChunkLOD CurrentLOD = EFPMChunkLOD::Unloaded;
  FFPMChunkHeightmapData ChunkData;

private:
  /**
   * Build the procedural mesh from HeightValues at the given LOD.
   * @param LODStep  Vertex skip (1=full, 2=half, 4=quarter)
   * @param bCollision  Whether to generate per-poly collision
   */
  void BuildMesh(int32 LODStep, bool bCollision);

  /** Map a biome enum to a vertex color for material splatting. */
  static FLinearColor BiomeToVertexColor(EFPMBiome Biome);
};
