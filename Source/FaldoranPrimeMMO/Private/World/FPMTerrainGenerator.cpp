// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// LEGACY WRAPPER — see FPMChunkData.cpp, FPMWorldChunkManager.cpp
// for the actual implementation.

#include "World/FPMTerrainGenerator.h"
#include "EngineUtils.h"
#include "World/FPMWorldChunkManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// ===================================================================
//  Legacy Console Command
// ===================================================================

FAutoConsoleCommand FPMTerrainGenerator::CmdGenerateTerrain(
    TEXT("FPM.GenerateTerrain"),
    TEXT("[LEGACY] Generate island terrain. Delegates to chunk system. "
         "Use FPM.GenerateWorld [seed] instead."),
    FConsoleCommandWithArgsDelegate::CreateLambda(
        [](const TArray<FString> &Args) {
          int32 Seed = 42;
          if (Args.Num() > 0) {
            Seed = FCString::Atoi(*Args[0]);
          }

          UWorld *World = nullptr;

#if WITH_EDITOR
          if (GEditor) {
            World = GEditor->GetEditorWorldContext().World();
          }
#endif

          if (!World) {
            for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
              if (Context.World()) {
                World = Context.World();
                break;
              }
            }
          }

          if (World) {
            FPMTerrainGenerator::GenerateIslandTerrain(World, Seed);
          } else {
            UE_LOG(LogTemp, Error,
                   TEXT("FPM: No world found for terrain generation"));
          }
        }));

// ===================================================================
//  Legacy API — delegates to chunk system
// ===================================================================

void FPMTerrainGenerator::GenerateIslandTerrain(UWorld *World, int32 Seed) {
  UE_LOG(LogTemp, Warning,
         TEXT("FPM: [LEGACY] GenerateIslandTerrain called. "
              "Delegating to chunk-based world system (seed=%d)..."),
         Seed);

  // Find the chunk manager in the world
  AFPMWorldChunkManager *Manager = nullptr;
  for (TActorIterator<AFPMWorldChunkManager> It(World); It; ++It) {
    Manager = *It;
    break;
  }

  if (Manager) {
    Manager->WorldSeed = Seed;
    Manager->ForceChunkUpdate();
    UE_LOG(LogTemp, Warning,
           TEXT("FPM: Chunk manager found — world generation initiated."));
  } else {
    UE_LOG(LogTemp, Error,
           TEXT("FPM: No AFPMWorldChunkManager found in the level! "
                "Place one in your map to use the chunk system. "
                "The old monolithic terrain generator has been replaced."));
  }
}

EFPMBiome FPMTerrainGenerator::GetBiomeAt(float NormX, float NormY) {
  // Convert normalized island coords to world position
  const float HalfIsland = FPMChunkConstants::StarterIslandWorldSize * 0.5f;
  const FVector WorldPos(
      NormX * FPMChunkConstants::StarterIslandWorldSize - HalfIsland,
      NormY * FPMChunkConstants::StarterIslandWorldSize - HalfIsland, 0.0f);

  // Generate a quick sample using the chunk generator
  // This doesn't require a loaded chunk — it's deterministic
  FFPMChunkHeightmapData TempData;
  const FFPMChunkCoord Coord = FPMChunkGenerator::WorldToChunkCoord(WorldPos);
  FPMChunkGenerator::GenerateChunk(Coord, 42, TempData); // Default seed

  // Find the vertex in the chunk
  const FVector ChunkOrigin = FPMChunkGenerator::ChunkToWorldOrigin(Coord);
  const float LocalX =
      (WorldPos.X - ChunkOrigin.X) / FPMChunkConstants::HexWidth;
  const float LocalY =
      (WorldPos.Y - ChunkOrigin.Y) / FPMChunkConstants::HexWidth;

  const int32 Res = FPMChunkConstants::ChunkResolution;
  const int32 IX =
      FMath::Clamp(FMath::FloorToInt(LocalX * (Res - 1)), 0, Res - 1);
  const int32 IY =
      FMath::Clamp(FMath::FloorToInt(LocalY * (Res - 1)), 0, Res - 1);

  if (TempData.bIsValid) {
    return TempData.BiomeValues[IY * Res + IX];
  }

  return EFPMBiome::Meadows;
}
