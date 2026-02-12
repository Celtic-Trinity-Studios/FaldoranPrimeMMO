// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// DEPRECATED: This file is kept for backward compatibility.
// The terrain generation system has been replaced by the chunk-based system:
//   - FPMChunkData.h       (deterministic chunk generation)
//   - FPMChunkOverlay.h    (player modification persistence)
//   - FPMChunkActor.h      (in-world chunk rendering)
//   - FPMWorldChunkManager.h (orchestration & streaming)
//
// The old FPM.GenerateTerrain console command now delegates to the chunk
// system. Use FPM.GenerateWorld [seed] instead.

#pragma once

#include "CoreMinimal.h"
#include "World/FPMChunkData.h"

/**
 * FPMTerrainGenerator (LEGACY WRAPPER)
 *
 * Wraps the new chunk-based system for backward compatibility.
 * The old GenerateIslandTerrain() now triggers the chunk manager.
 *
 * NEW COMMANDS:
 *   FPM.GenerateWorld [seed]  — Initialize chunk-based world
 *   FPM.RegenChunks           — Regenerate all loaded chunks
 *
 * LEGACY COMMAND (still works):
 *   FPM.GenerateTerrain [seed] — Delegates to FPM.GenerateWorld
 */
class FALDORANPRIMEMMO_API FPMTerrainGenerator {
public:
  /**
   * LEGACY: Generate island terrain.
   * Now delegates to the chunk-based world system.
   * @param World - The world containing the landscape
   * @param Seed - Random seed for deterministic generation
   */
  static void GenerateIslandTerrain(UWorld *World, int32 Seed);

  /**
   * Get the biome at a world position.
   * Delegates to FPMChunkGenerator.
   * @param NormX - Normalized X position (0-1) across the island
   * @param NormY - Normalized Y position (0-1) across the island
   */
  static EFPMBiome GetBiomeAt(float NormX, float NormY);

private:
  // Legacy console command (delegates to new system)
  static FAutoConsoleCommand CmdGenerateTerrain;
};
