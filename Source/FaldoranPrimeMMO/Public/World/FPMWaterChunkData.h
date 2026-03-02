// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// Per-chunk water data structures for the flowing water system.
// Water uses a LOWER resolution grid than terrain (33x33 vs 129x129)
// because water doesn't need 2m precision -- an ~8m cell is fine for
// for rivers and lakes, and it's 15x cheaper to simulate.

#pragma once

#include "CoreMinimal.h"
#include "World/FPMChunkData.h"

// =====================================================================
//  Water Source Type
// =====================================================================

// Note: EFPMWaterSourceType is defined here as a plain enum class
// rather than UENUM because this header is included transitively by
// other UCLASS headers, and having two .generated.h files in the
// include chain causes UHT errors. The enum can be exposed to
// Blueprint via a wrapper if needed.

enum class EFPMWaterSourceType : uint8 {
  /** Mountain spring -- spawns at high elevation */
  Spring,

  /** Rainfall zone -- broad, low-flow area source */
  Rainfall,

  /** Cave exit -- water emerging from underground */
  Cave,

  /** Artesian well -- pressurized upwelling */
  Artesian,

  MAX
};

// =====================================================================
//  Water Simulation Constants
// =====================================================================

namespace FPMWaterConstants {

/** Water grid resolution per chunk edge (33 = 32 quads + 1).
 *  At 256m chunk size this gives ~8m water cellspacing.
 *  Significantly cheaper than terrain's 129x129. */
constexpr int32 WaterResolution = 33;

/** Total water cells per chunk */
constexpr int32 WaterCellCount = WaterResolution * WaterResolution;

/** Number of cardinal flow directions (N, E, S, W) */
constexpr int32 NumFlowDirs = 4;

/** Minimum water depth to render (cm). Below this, water is invisible. */
inline float MinRenderDepth = 2.0f;

/** Maximum water depth per cell (cm). Caps unrealistic pooling. */
inline float MaxWaterDepth = 500.0f;

/** Evaporation rate (cm of water lost per second per cell).
 *  This is the PRIMARY anti-flood control.
 *  Higher = shorter rivers, lower = longer rivers. */
inline float EvaporationRate = 0.001f;

/** Gravity acceleration for flow calculation (cm/s^2) */
inline float FlowGravity = 980.0f;

/** Simulation tick rate (Hz). Water updates at this frequency, not every frame.
 */
inline float SimulationRate = 15.0f;

/** Maximum flow hops from any source. Water beyond this decays rapidly.
 *  At 8m cells, 200 hops = ~1600m max river length per source. */
inline int32 MaxFlowHops = 200;

/** Number of water sources per mountain biome chunk (procedural placement) */
inline int32 SourcesPerMountainChunk = 2;

/** Base flow rate for springs (cm^3/sec) */
inline float SpringFlowRate = 50.0f;

/** Minimum normalized terrain height for natural springs */
inline float MinSpringElevation = 0.25f;

/** Damping factor applied to pipe flow each step (0-1).
 *  Prevents oscillation. 1.0 = no damping. */
constexpr float PipeDamping = 0.98f;

/** Rapid decay multiplier for water beyond MaxFlowHops */
constexpr float BeyondRangeDecayMultiplier = 10.0f;

} // namespace FPMWaterConstants

// =====================================================================
//  Per-Chunk Water Data
// =====================================================================

/**
 * FFPMChunkWaterData
 *
 * Water simulation state for a single chunk.
 * Stored as a 2D heightfield (water surface height per XY column).
 *
 * This is NOT 3D voxel water -- for surface rivers/lakes, a 2D
 * column approach is sufficient and much cheaper.
 *
 * Memory: ~33x33 x (4+8+4+4+4x4) = ~36KB per active chunk.
 * Only chunks with water sources or active flow allocate this.
 *
 * Note: Not a USTRUCT -- these are C++ only data structures managed
 * by the WorldChunkManager. No Blueprint exposure needed for
 * simulation internals.
 */
struct FFPMChunkWaterData {

  /** The chunk this water data belongs to */
  FFPMChunkCoord Coord;

  /** Water depth at each XY column (cm above terrain surface).
   *  Indexed [Y * WaterResolution + X]. */
  TArray<float> WaterDepth;

  /** Flow direction per cell (normalized 2D vector).
   *  Used for UV animation on the water surface material. */
  TArray<FVector2D> FlowDirection;

  /** Flow speed magnitude per cell (cm/s).
   *  Used for foam intensity and sound volume. */
  TArray<float> FlowSpeed;

  /** Distance from nearest water source (in cell hops).
   *  Anti-flood mechanism: water beyond MaxFlowHops decays rapidly. */
  TArray<int32> SourceDistance;

  /** Pipe flow buffers -- outflow rate through each of 4 cardinal edges.
   *  N=0, E=1, S=2, W=3.
   *  Stored per-cell for the pipe model simulation. */
  TArray<float> PipeFlow; // Flat array: [Idx * 4 + Dir]

  /** Whether this chunk has any water (optimization: skip empty chunks) */
  bool bHasWater = false;

  /** Whether this chunk contains a water source */
  bool bHasSource = false;

  /** Whether this data has been allocated */
  bool bAllocated = false;

  /** Timestamp of last simulation tick (world seconds) */
  float LastSimTime = 0.0f;

  /** Whether the water mesh needs rebuilding */
  bool bMeshDirty = true;

  void Allocate() {
    const int32 N = FPMWaterConstants::WaterCellCount;
    WaterDepth.SetNumZeroed(N);
    FlowDirection.SetNumZeroed(N);
    FlowSpeed.SetNumZeroed(N);
    SourceDistance.Init(INT32_MAX, N);
    PipeFlow.SetNumZeroed(N * FPMWaterConstants::NumFlowDirs);
    bAllocated = true;
    bMeshDirty = true;
  }

  void Reset() {
    WaterDepth.Empty();
    FlowDirection.Empty();
    FlowSpeed.Empty();
    SourceDistance.Empty();
    PipeFlow.Empty();
    bAllocated = false;
    bHasWater = false;
    bHasSource = false;
    bMeshDirty = true;
    LastSimTime = 0.0f;
  }

  /** Check if any cell has renderable water */
  bool HasRenderableWater() const {
    if (!bAllocated)
      return false;
    for (int32 i = 0; i < WaterDepth.Num(); ++i) {
      if (WaterDepth[i] > FPMWaterConstants::MinRenderDepth) {
        return true;
      }
    }
    return false;
  }

  /** Get pipe flow for a cell and direction */
  float GetPipe(int32 CellIdx, int32 Dir) const {
    return PipeFlow[CellIdx * FPMWaterConstants::NumFlowDirs + Dir];
  }

  /** Set pipe flow for a cell and direction */
  void SetPipe(int32 CellIdx, int32 Dir, float Value) {
    PipeFlow[CellIdx * FPMWaterConstants::NumFlowDirs + Dir] = Value;
  }
};

// =====================================================================
//  Water Source Definition (for procedural placement)
// =====================================================================

/**
 * FFPMWaterSourceDef
 *
 * Lightweight definition of a water source within a chunk.
 * Used during procedural placement -- the actual simulation
 * just reads the WaterDepth array each tick.
 */
struct FFPMWaterSourceDef {

  /** Cell index within the water grid where this source emits */
  int32 CellIndex = 0;

  /** Flow rate in cm^3/sec */
  float FlowRate = 50.0f;

  /** Source type (for visual/gameplay differentiation) */
  EFPMWaterSourceType Type = EFPMWaterSourceType::Spring;

  /** Whether this source is infinite (vs depletable) */
  bool bInfinite = true;
};
