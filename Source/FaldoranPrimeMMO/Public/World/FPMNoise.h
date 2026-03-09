#pragma once

#include "CoreMinimal.h"

/**
 * AAA noise pipeline for world-scale procedural terrain.
 *
 * - Simplex gradient noise (no directional artifacts)
 * - Domain warping (organic, non-grid-aligned features)
 * - Continuous terrain height (single source of truth)
 * - Voronoi cellular noise for macro biome regions
 * - Temperature/Moisture fields (macro-only + tiny detail)
 * - Diffusion-smoothed climate grids (kills all speckling)
 * - Talus erosion (eliminates cliffs)
 *
 * All coordinates are world-space centimeters.
 * Chunks are sampling windows — NO per-chunk seeding.
 */

/** Biome weight entry for soft membership blending. */
struct FBiomeWeight {
  uint8 BiomeIdx; // Index into the 9-biome climate grid (0-8)
  float Weight;   // 0-1 membership weight
};

/** Result of a soft biome query: up to 4 contributing biomes + confidence. */
struct FBiomeBlend {
  static constexpr int32 MaxBlend = 4;
  FBiomeWeight Entries[MaxBlend];
  int32 Count = 0;
  uint8 PrimaryIdx = 0; // Index of the winning biome (highest weight)
  float Confidence = 0; // How dominant the winner is (0=tied, 1=100%)
};

struct FPMNoise {
  // === 2D Primitives ===
  static float Simplex2D(float X, float Y, int32 Seed);
  static float FBM(float X, float Y, int32 Seed, int32 Octaves = 6,
                   float Gain = 0.45f, float Lacunarity = 2.0f);
  static float RidgeFBM(float X, float Y, int32 Seed, int32 Octaves = 5,
                        float Gain = 0.5f, float Lacunarity = 2.2f);
  static void DomainWarp(float &X, float &Y, int32 Seed,
                         float Strength = 50000.0f, float Freq = 0.000002f);

  // === 3D Primitives (for cave generation) ===
  /** 3D Simplex gradient noise. Returns value in [-1, 1]. */
  static float Simplex3D(float X, float Y, float Z, int32 Seed);

  /** 3D Fractal Brownian Motion. Returns [0, 1]. */
  static float FBM3D(float X, float Y, float Z, int32 Seed, int32 Octaves = 4,
                     float Gain = 0.45f, float Lacunarity = 2.0f);

  // === Cave System ===
  /** Compute cave carving density at a world-space 3D position.
   *  Returns a value in [0, 1] where:
   *    0 = no carving (solid rock)
   *    1 = fully carved (open air)
   *  Combines worm tunnels, grand caverns, and depth attenuation.
   *  Pure math, safe for any thread. */
  static float CaveDensity(float WorldX, float WorldY, float WorldZ, int32 Seed,
                           float SurfaceZ);

  // === Voronoi Cellular Noise ===
  /** 2D Voronoi: returns distance to nearest cell center, a per-cell hash
   *  (0-1), and distance to the nearest cell edge (0=on boundary). */
  static void Voronoi2D(float X, float Y, int32 Seed, float &OutMinDist,
                        float &OutCellHash, float &OutEdgeDist);

  // === Macro Biome Region ===
  /** Very low-frequency Voronoi region map.
   *  Returns per-region climate biases (TempBias, MoistBias) and
   *  EdgeBlend (0=on a boundary, 1=deep inside region). */
  static void BiomeRegion(float WorldX, float WorldY, int32 Seed,
                          float &OutTempBias, float &OutMoistBias,
                          float &OutEdgeBlend);

  // === Terrain Pipeline ===
  /** Returns normalized height [0,1]. Caller scales by HeightScale. */
  static float TerrainHeight(float WorldX, float WorldY, int32 Seed);

  /** Island mask [0,1]. 0=ocean, 1=interior. */
  static float IslandMask(float WorldX, float WorldY, int32 Seed);

  /** Temperature field [0,1] for biome selection.
   *  Macro-only base (2-3 octaves) + Voronoi region bias + 10% detail. */
  static float Temperature(float WorldX, float WorldY, int32 Seed);

  /** Moisture field [0,1] for biome selection.
   *  Macro-only base (2-3 octaves) + Voronoi region bias + 10% detail. */
  static float Moisture(float WorldX, float WorldY, int32 Seed);

  // === Climate Grid Smoothing ===
  /** Diffusion-smooth a 2D float grid in-place.
   *  Runs Passes iterations of: field = lerp(field, neighborAvg, Strength).
   *  Grid is Width × Height, row-major. */
  static void DiffusionSmooth(TArray<float> &Grid, int32 Width, int32 Height,
                              int32 Passes = 6, float Strength = 0.5f);

  // === Erosion ===
  static void TalusErosion(TArray<float> &Heights, int32 Res, float CellSpacing,
                           int32 Iters = 4, float MaxSlopeRad = 0.65f);

  /** World-space terrain surface Z at a given XY (cm).
   *  Pure math, safe for any thread. */
  static float TerrainSurfaceZ(float WorldX, float WorldY, int32 Seed);

private:
  static int32 GradHash(int32 X, int32 Y, int32 Seed);
  static float GradDot(int32 Hash, float X, float Y);

  // 3D gradient helpers
  static int32 GradHash3D(int32 X, int32 Y, int32 Z, int32 Seed);
  static float GradDot3D(int32 Hash, float X, float Y, float Z);

  /** Hash an integer cell coordinate to a float [0,1] */
  static float CellHash(int32 X, int32 Y, int32 Seed);
};
