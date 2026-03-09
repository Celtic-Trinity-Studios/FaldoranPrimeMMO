// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/FPMChunkData.h"

// ===================================================================
//  Voxel Constants
// ===================================================================

namespace FPMVoxelConstants {
/** Size of each voxel in centimeters (40m - world sim scale) */
constexpr float VoxelSizeCm = 4000.0f;

/** Number of voxels per chunk in XY (must divide ChunkWorldSize evenly) */
constexpr int32 ChunkVoxelsXY =
    static_cast<int32>(FPMChunkConstants::HexWidth / VoxelSizeCm); // 32

/** Overlap margin in voxels on each side of the chunk. */
constexpr int32 OverlapMargin = 2;

/** Total voxel grid size in XY including overlap margins on both sides */
constexpr int32 ChunkVoxelsXY_Total = ChunkVoxelsXY + OverlapMargin * 2; // 36

/** Number of voxels in the vertical (Z) dimension.
 *  500 * 40m = 20km total verticality coverage. */
constexpr int32 ChunkVoxelsZ = 500;

/** Density grid corners (voxels + 1 in each dimension for MC) */
constexpr int32 GridX = ChunkVoxelsXY_Total + 1; // 37
constexpr int32 GridY = ChunkVoxelsXY_Total + 1; // 37
constexpr int32 GridZ = ChunkVoxelsZ + 1;        // 501
constexpr int32 TotalCorners = GridX * GridY * GridZ;

/** World-space offset of the overlap margin */
constexpr float OverlapOffsetCm = OverlapMargin * VoxelSizeCm;

/** World Z base of the voxel volume - matches MinWorldZ (-11km) */
constexpr float WorldZBase = FPMChunkConstants::MinWorldZ;

/** World Z top of the voxel volume - matches MaxWorldZ (+9km) */
constexpr float WorldZTop = WorldZBase + ChunkVoxelsZ * VoxelSizeCm;

// --- Fine-resolution terraform overlay ---

/** Voxel size for terraform overlay tiles (50cm per cell, matches Enshrouded). */
constexpr float TerraformVoxelSizeCm = 50.0f;

/** Number of voxels per terraform tile axis (32 = 16m tile at 50cm voxels). */
constexpr int32 TerraformTileVoxels = 32;

/** World size of a terraform tile in cm. */
constexpr float TerraformTileWorldSize =
    TerraformTileVoxels * TerraformVoxelSizeCm; // 1600 cm = 16m

/** Grid corners for terraform tile MC (voxels + 1). */
constexpr int32 TerraformGridN = TerraformTileVoxels + 1; // 33
constexpr int32 TerraformTotalCorners =
    TerraformGridN * TerraformGridN * TerraformGridN; // 35937

} // namespace FPMVoxelConstants

// ===================================================================
//  Voxel Mesh Output - ready to feed to ProceduralMeshComponent
// ===================================================================

struct FFPMVoxelMeshData {
  TArray<FVector> Vertices;
  TArray<int32> Triangles;
  TArray<FVector> Normals;
  TArray<FVector2D> UVs;
  TArray<FColor> Colors;

  void Reset() {
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UVs.Empty();
    Colors.Empty();
  }
};

// ===================================================================
//  Voxel Generator - density field + Marching Cubes meshing
// ===================================================================

class FPMVoxelGenerator {
public:
  /**
   * Generate a voxel density field for a chunk and extract a triangle
   * mesh via Marching Cubes.
   *
   * @param Coord     Chunk coordinate in the world grid
   * @param WorldSeed World seed for deterministic noise
   * @param OutMesh   Output mesh data (vertices, triangles, normals, etc.)
   */
  static void GenerateAndMesh(const FFPMChunkCoord &Coord, int32 WorldSeed,
                              FFPMVoxelMeshData &OutMesh);

  /** Compute the terrain surface Z in world units at a given world XY.
   *  Reuses the same noise/biome logic as the heightmap system. */
  static float TerrainSurfaceZ(float WorldX, float WorldY, int32 WorldSeed);

  /** Return the biome at a world XY position (same logic as heightmap). */
  static EFPMBiome BiomeAtWorldXY(float WorldX, float WorldY, int32 WorldSeed,
                                  float NormalizedHeight);

private:
  /** Linearly interpolate a vertex along an MC edge where density=0. */
  static FVector InterpolateEdge(const FVector &P1, const FVector &P2, float D1,
                                 float D2);

public:
  /** Map biome enum to vertex color for material splatting. */
  static FColor BiomeToVertexColor(EFPMBiome Biome);

  /**
   * Generate a fine-resolution terraform tile.
   * Creates a 32×32×32 density field at 200cm resolution (64m tile)
   * centered on TileOrigin, applying both coarse terrain and fine deltas.
   *
   * @param TileOrigin  World-space origin (min corner) of the tile
   * @param WorldSeed   World seed for terrain generation
   * @param FineDeltas  Map of fine voxel keys ? density deltas
   * @param OutMesh     Output mesh data
   */
  static void GenerateTerraformTile(const FVector &TileOrigin, int32 WorldSeed,
                                    const TMap<FIntVector, float> &FineDeltas,
                                    FFPMVoxelMeshData &OutMesh,
                                    bool bForceBaseSurface = false);
};

