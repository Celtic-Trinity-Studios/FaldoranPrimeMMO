// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "FPMNexusManager.generated.h"

/**
 * FFPMNexusDefinition
 *
 * Defines a single Nexus — the safe starter city hub for one continent.
 * Nexus positions are deterministic (seed + continent ID) and stored in
 * WorldGen.ini [Nexus] for reproducibility.
 *
 * The Nexus is:
 *   - The guaranteed first-spawn location for new characters
 *   - A creature-free safe zone (NPC spawner checks this radius)
 *   - An empty city area (no PCG vegetation inside the radius)
 *   - The respawn anchor for the continent (future: death respawn here)
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMNexusDefinition {
  GENERATED_BODY()

  /** Human-readable continent/nexus name, e.g. "Aelvorn Nexus" */
  UPROPERTY(BlueprintReadOnly)
  FString NexusName;

  /** World-space center of this Nexus (XY only; Z is resolved at runtime
   *  via TerrainSurfaceZ so it's always valid even if terrain changes). */
  UPROPERTY(BlueprintReadOnly)
  FVector2D WorldCenter;

  /** Radius around WorldCenter that is creature/vegetation free (cm).
   *  Default: 51200cm = 512m — roughly a small town footprint.
   *  Config key: NexusSafeRadius in [Nexus] section. */
  UPROPERTY(BlueprintReadOnly)
  float SafeRadius = 51200.f;

  /** Continent index this nexus belongs to (0 = starter island). */
  UPROPERTY(BlueprintReadOnly)
  int32 ContinentId = 0;
};

/**
 * AFPMNexusManager
 *
 * Singleton actor that holds the authoritative list of all Nexus locations.
 * Loaded from Config/WorldGen.ini [Nexus] on BeginPlay.
 *
 * KEY RESPONSIBILITIES:
 *   1. Provide the spawn location for new characters (GetNewCharacterSpawnPos)
 *   2. Tell the PCG spawner whether a world position is inside any Nexus
 *      safe zone (IsInNexusSafeZone)
 *   3. Future: track which Nexus a player is bound to for respawning
 *
 * NEXUS POSITION DERIVATION:
 *   Nexus XY coordinates are stored directly in WorldGen.ini.
 *   Z is resolved at spawn time via FPMVoxelGenerator::TerrainSurfaceZ().
 *   This means terrain changes don't invalidate the config.
 *
 * CURRENTLY (starter island): one Nexus at (0, 0) in world space.
 *   Future: one Nexus per continent, defined as [Nexus1], [Nexus2], etc.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMNexusManager : public AActor {
  GENERATED_BODY()

public:
  AFPMNexusManager();

  virtual void BeginPlay() override;

  // -----------------------------------------------------------------------
  // Singleton access
  // -----------------------------------------------------------------------

  /**
   * Get the AFPMNexusManager for the world, creating one if needed.
   * Guarantees exactly one per UWorld (same pattern as WorldChunkManager).
   */
  static AFPMNexusManager *GetOrCreate(UWorld *World);

  // -----------------------------------------------------------------------
  // Nexus Queries
  // -----------------------------------------------------------------------

  /**
   * Get the world-space spawn point for a brand-new character.
   *
   * Selects Nexus[0] (the starter island Nexus). The returned FVector has:
   *   - XY = Nexus center
   *   - Z  = terrain surface at that XY (via TerrainSurfaceZ)
   *
   * @param WorldSeed  The active world seed (needed for terrain sampling).
   * @return           World-space spawn position (terrain surface).
   */
  FVector GetNewCharacterSpawnPos(int32 WorldSeed) const;

  /**
   * Returns true if WorldPos (XY only) lies inside any Nexus safe zone.
   * Used by FPMBiomePCGSpawner to suppress vegetation and by the future
   * creature AI system to suppress spawns.
   *
   * @param WorldPos   World-space position (Z ignored).
   * @return           true if inside a safe zone.
   */
  bool IsInNexusSafeZone(const FVector &WorldPos) const;

  /**
   * Returns true if a 2D point is inside any Nexus safe zone.
   *
   * @param WorldXY    World XY position.
   * @return           true if inside a safe zone.
   */
  bool IsInNexusSafeZone(const FVector2D &WorldXY) const;

  /** Read-only access to all defined Nexus locations. */
  const TArray<FFPMNexusDefinition> &GetAllNexuses() const { return Nexuses; }

  /** Get the safe radius of the starter Nexus (for debug / HUD). */
  float GetStarterNexusSafeRadius() const;

  // -----------------------------------------------------------------------
  //  Configuration (editable on placed actor in level, loaded from ini)
  // -----------------------------------------------------------------------

  /** All nexus definitions, populated from WorldGen.ini [Nexus] on
   * BeginPlay. Can be viewed/edited in the Details panel but the ini is
   * the true source of truth. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPM|Nexus")
  TArray<FFPMNexusDefinition> Nexuses;

private:
  /** Parse [Nexus] section of WorldGen.ini and fill Nexuses array. */
  void LoadFromConfig();

  /** Whether we've loaded config yet. */
  bool bConfigLoaded = false;
};
