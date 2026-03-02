// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// Generates procedural water surface meshes from per-chunk water data.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "World/FPMChunkData.h"
#include "World/FPMWaterChunkData.h"

/**
 * FPMWaterMeshBuilder
 *
 * Generates the visible water surface mesh for a chunk.
 *
 * The water mesh is a separate ProceduralMeshComponent overlaid
 * on top of the terrain. Only cells with water depth above the
 * minimum render threshold get geometry.
 *
 * Vertex attributes:
 *   - Position: terrain Z + water depth
 *   - UV0: Standard texture coords
 *   - UV1: Flow direction (for animated water material)
 *   - Color.R: Water depth (for opacity: shallow=clear, deep=opaque)
 *   - Color.G: Flow speed (for foam intensity)
 *   - Color.B: Distance from source (for color tinting)
 *   - Color.A: Reserved
 */
class FALDORANPRIMEMMO_API FPMWaterMeshBuilder {
public:
  /**
   * Build the water surface mesh for a chunk.
   *
   * @param Water          Per-chunk water simulation data
   * @param Terrain        Chunk terrain data (for surface heights)
   * @param ChunkCoord     Chunk coordinate (for world position)
   * @param OutVertices    Output vertex positions
   * @param OutTriangles   Output triangle indices
   * @param OutNormals     Output vertex normals
   * @param OutUVs         Output texture coordinates
   * @param OutColors      Output vertex colors (depth, flow, etc.)
   * @return true if any water geometry was generated
   */
  static bool
  BuildWaterMesh(const FFPMChunkWaterData &Water,
                 const FFPMChunkHeightmapData &Terrain,
                 const FFPMChunkCoord &ChunkCoord, TArray<FVector> &OutVertices,
                 TArray<int32> &OutTriangles, TArray<FVector> &OutNormals,
                 TArray<FVector2D> &OutUVs, TArray<FColor> &OutColors);

  /**
   * Apply the built mesh to a ProceduralMeshComponent.
   *
   * @param MeshComp   The PMC to update (section 0)
   * @param Water      Water data
   * @param Terrain    Terrain data
   * @param Coord      Chunk coordinate
   * @return true if mesh was created (had water), false if cleared
   */
  static bool UpdateWaterMeshComponent(UProceduralMeshComponent *MeshComp,
                                       const FFPMChunkWaterData &Water,
                                       const FFPMChunkHeightmapData &Terrain,
                                       const FFPMChunkCoord &Coord);
};
