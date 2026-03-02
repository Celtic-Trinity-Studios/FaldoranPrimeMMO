// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// Water flow simulation using the pipe model (cellular automaton).
// This is the core engine that prevents flooding via:
//   1. Evaporation — water slowly disappears everywhere
//   2. Ocean drain — Coast/Ocean biomes absorb all water
//   3. Flow range limit — water decays beyond N hops from source
//   4. Max water depth cap — prevents unrealistic pooling
//   5. Volume conservation — can't outflow more than you have

#pragma once

#include "CoreMinimal.h"
#include "World/FPMChunkData.h"
#include "World/FPMWaterChunkData.h"

/**
 * FPMWaterSimulation
 *
 * Stateless utility class that runs the pipe-model water flow simulation.
 *
 * The pipe model works by:
 *   1. For each cell, compute the total water surface height (terrain + water
 * depth)
 *   2. For each neighbor, compute height difference
 *   3. Height difference drives flow through "pipes" (virtual connections)
 *   4. Apply volume conservation: can't outflow more than available
 *   5. Update water depths based on net flow
 *   6. Apply evaporation and anti-flood decay
 *
 * The simulation runs at a fixed rate (default 15 Hz), not every frame.
 * Only chunks with water sources or active flow are simulated.
 *
 * Reference: "Fast Hydraulic Erosion Simulation and Visualization on GPU"
 *            (Mei, Decaudin, Hu, 2007)
 */
class FALDORANPRIMEMMO_API FPMWaterSimulation {
public:
  // =================================================================
  //  Core Simulation
  // =================================================================

  /**
   * Simulate one tick of water flow for a single chunk.
   *
   * @param Water        Per-chunk water data (modified in place)
   * @param Terrain      Chunk's terrain heightmap (read-only, for surface
   * heights)
   * @param BiomeData    Biome at each terrain vertex (for ocean drain
   * detection)
   * @param DeltaTime    Simulation time step (seconds)
   */
  static void SimulateChunk(FFPMChunkWaterData &Water,
                            const FFPMChunkHeightmapData &Terrain,
                            float DeltaTime);

  /**
   * Inject water from sources into a chunk's water grid.
   * Call this each simulation tick before SimulateChunk.
   *
   * @param Water        Per-chunk water data (modified in place)
   * @param Sources      Array of source definitions active in this chunk
   * @param DeltaTime    Simulation time step
   */
  static void InjectSources(FFPMChunkWaterData &Water,
                            const TArray<FFPMWaterSourceDef> &Sources,
                            float DeltaTime);

  /**
   * Handle water flow across chunk boundaries.
   * Called after all per-chunk simulations to resolve edge cells.
   *
   * @param ChunkWaterMap  Map of all active chunk water data
   */
  static void ResolveCrossChunkFlow(
      TMap<FFPMChunkCoord, FFPMChunkWaterData> &ChunkWaterMap);

  // =================================================================
  //  Procedural Source Placement
  // =================================================================

  /**
   * Deterministically place water sources within a chunk based on
   * terrain height and biome data.
   *
   * Sources are placed at high-elevation points in Mountain/Snow/Alpine
   * biomes using a noise-based selection that's deterministic from seed.
   *
   * @param Coord        Chunk coordinate
   * @param WorldSeed    World generation seed
   * @param Terrain      Chunk's heightmap data
   * @param OutSources   Output array of placed sources
   */
  static void PlaceProceduralSources(const FFPMChunkCoord &Coord,
                                     int32 WorldSeed,
                                     const FFPMChunkHeightmapData &Terrain,
                                     TArray<FFPMWaterSourceDef> &OutSources);

  // =================================================================
  //  Utility
  // =================================================================

  /**
   * Sample terrain height at a water grid cell position.
   * The water grid (33x33) is lower resolution than terrain (129x129),
   * so this bilinearly interpolates the terrain heightmap.
   *
   * @param Terrain  Chunk terrain heightmap
   * @param WaterX   Water grid X coordinate (0 to WaterResolution-1)
   * @param WaterY   Water grid Y coordinate (0 to WaterResolution-1)
   * @return Terrain surface height in world Z (cm)
   */
  static float SampleTerrainHeight(const FFPMChunkHeightmapData &Terrain,
                                   int32 WaterX, int32 WaterY);

  /**
   * Get the biome at a water grid cell position.
   * Nearest-neighbor lookup from the higher-res terrain biome grid.
   *
   * @param Terrain  Chunk terrain data
   * @param WaterX   Water grid X coordinate
   * @param WaterY   Water grid Y coordinate
   * @return Biome at this position
   */
  static EFPMBiome SampleBiome(const FFPMChunkHeightmapData &Terrain,
                               int32 WaterX, int32 WaterY);

  /**
   * Compute water grid neighbor cell index.
   * Returns -1 if the neighbor is outside the chunk boundary
   * (cross-chunk flow handled separately).
   *
   * @param X    Current cell X
   * @param Y    Current cell Y
   * @param Dir  Direction (0=N, 1=E, 2=S, 3=W)
   * @return Neighbor cell index, or -1 if out of bounds
   */
  static int32 GetNeighborIdx(int32 X, int32 Y, int32 Dir);

  /**
   * Compute flow direction and speed from pipe values.
   * Used to set FlowDirection and FlowSpeed for rendering.
   *
   * @param Water  Water data to update flow vectors for
   */
  static void ComputeFlowVectors(FFPMChunkWaterData &Water);

  /**
   * Load water simulation settings from WorldGen.ini.
   * Called once during WorldChunkManager::BeginPlay.
   *
   * @param IniPath  Full path to WorldGen.ini
   */
  static void LoadSettingsFromINI(const FString &IniPath);
};
