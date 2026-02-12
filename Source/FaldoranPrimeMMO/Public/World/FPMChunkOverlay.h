// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/FPMChunkData.h"
#include "FPMChunkOverlay.generated.h"


/**
 * FFPMVertexModification
 *
 * A single vertex modification within a chunk.
 * Stores the delta from the procedurally generated base value.
 */
USTRUCT()
struct FFPMVertexModification {
  GENERATED_BODY()

  /** Index into the chunk's vertex array (Y * Resolution + X) */
  UPROPERTY()
  int32 VertexIndex = 0;

  /** Height delta: added to the procedural base height */
  UPROPERTY()
  float HeightDelta = 0.0f;

  /** If true, the biome at this vertex has been overridden by the player */
  UPROPERTY()
  bool bBiomeOverridden = false;

  /** The overridden biome (only valid if bBiomeOverridden) */
  UPROPERTY()
  EFPMBiome OverriddenBiome = EFPMBiome::Meadows;
};

/**
 * FFPMChunkOverlay
 *
 * Sparse overlay of player modifications for a single chunk.
 * Only modified vertices are stored — unmodified vertices use
 * the procedurally generated base values.
 *
 * This is persisted to disk and loaded when a chunk enters view.
 * The overlay is applied ON TOP of the deterministic chunk data,
 * so the base terrain never needs to be stored.
 *
 * Saving format: JSON per chunk, stored at:
 *   {SaveDir}/Chunks/{ChunkX}_{ChunkY}.json
 */
USTRUCT()
struct FFPMChunkOverlay {
  GENERATED_BODY()

  /** Chunk this overlay belongs to */
  UPROPERTY()
  FFPMChunkCoord Coord;

  /** Sparse list of modified vertices */
  UPROPERTY()
  TArray<FFPMVertexModification> Modifications;

  /** True if this overlay has any modifications */
  bool HasModifications() const { return Modifications.Num() > 0; }

  /** True if this overlay has been loaded from disk (or is a fresh empty one)
   */
  bool bIsLoaded = false;
};

/**
 * FPMChunkOverlayManager
 *
 * Manages loading, saving, and applying chunk overlays.
 * Overlays are stored per-chunk as small JSON files.
 */
class FALDORANPRIMEMMO_API FPMChunkOverlayManager {
public:
  /**
   * Initialize the overlay manager with a save directory.
   * @param SaveDirectory Root directory for chunk overlay files
   */
  static void Initialize(const FString &SaveDirectory);

  /**
   * Load an overlay from disk for a specific chunk.
   * Returns an empty overlay if no save file exists.
   * @param Coord Chunk coordinate
   * @param OutOverlay The loaded overlay
   * @return true if a save file existed and was loaded
   */
  static bool LoadOverlay(const FFPMChunkCoord &Coord,
                          FFPMChunkOverlay &OutOverlay);

  /**
   * Save an overlay to disk.
   * Only writes if the overlay has modifications.
   * @param Overlay The overlay to save
   * @return true if save succeeded
   */
  static bool SaveOverlay(const FFPMChunkOverlay &Overlay);

  /**
   * Apply an overlay to chunk heightmap data, modifying it in place.
   * @param Overlay The overlay with modifications
   * @param InOutData The chunk data to modify
   */
  static void ApplyOverlay(const FFPMChunkOverlay &Overlay,
                           FFPMChunkHeightmapData &InOutData);

  /**
   * Add a height modification to an overlay.
   * If the vertex was already modified, updates the delta.
   * @param Overlay The overlay to modify
   * @param VertexIndex Index in the chunk vertex array
   * @param HeightDelta Height change from base
   */
  static void AddHeightModification(FFPMChunkOverlay &Overlay,
                                    int32 VertexIndex, float HeightDelta);

  /**
   * Add a biome override to an overlay.
   * @param Overlay The overlay to modify
   * @param VertexIndex Index in the chunk vertex array
   * @param NewBiome The new biome to set
   */
  static void AddBiomeOverride(FFPMChunkOverlay &Overlay, int32 VertexIndex,
                               EFPMBiome NewBiome);

  /**
   * Delete overlay file from disk for a chunk (reset to procedural).
   * @param Coord Chunk coordinate
   */
  static void DeleteOverlay(const FFPMChunkCoord &Coord);

private:
  /** Get the file path for a chunk's overlay file */
  static FString GetOverlayFilePath(const FFPMChunkCoord &Coord);

  /** Root save directory */
  static FString SaveDir;
};
