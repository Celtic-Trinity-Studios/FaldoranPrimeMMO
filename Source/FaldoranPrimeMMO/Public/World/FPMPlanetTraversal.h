// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "World/FPMChunkData.h"


class AFPMTerrainShell;

#include "FPMPlanetTraversal.generated.h"

/**
 * UFPMPlanetTraversal  —  Unified Flight System
 *
 * G key  = Toggle flight on / off
 * H key  = Cycle speed tier while flying
 *
 * Tier 0 | Hover       |       0 km/h  | Stationary, holds altitude above
 * terrain Tier 1 | Glide       |   ~150 km/h   | Manual CMC steering, no
 * auto-propulsion Tier 2 | Boost       |    500 km/h   | Auto-propels forward
 * in look direction Tier 3 | Hypersonic  |  5,000 km/h   | Auto-propels Tier 4
 * | Rift Speed  | 50,000 km/h   | Auto-propels  (~15 min world loop)
 *
 * LOD terrain shell spawns at Tier 2+ so you can see the landscape
 * during high-speed travel.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FALDORANPRIMEMMO_API UFPMPlanetTraversal : public UActorComponent {
  GENERATED_BODY()

public:
  UFPMPlanetTraversal();

  virtual void BeginPlay() override;
  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

  // -----------------------------------------------------------------------
  //  Input API  (called by PlayerController bindings)
  // -----------------------------------------------------------------------

  /** G key: toggle flight on/off. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  void ToggleFlight();

  /** H key: cycle through speed tiers 0-4. Wraps back to 0. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  void CycleSpeedTier();

  /** Legacy alias so existing PlayerController binding compiles without
   * changes. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  void ToggleRiftRunner() { ToggleFlight(); }

  // -----------------------------------------------------------------------
  //  State queries
  // -----------------------------------------------------------------------
  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  bool IsRiftRunnerActive() const { return bIsFlying; }

  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  bool IsFlying() const { return bIsFlying; }

  UFUNCTION(BlueprintCallable, Category = "FPM|Flight")
  int32 GetSpeedTier() const { return CurrentSpeedTier; }

  void SetWorldSeed(int32 InSeed) { WorldSeed = InSeed; }

  // -----------------------------------------------------------------------
  //  Configurable speeds  (cm/s)
  // -----------------------------------------------------------------------

  /** Tier 1 — Glide: gentle manual flight. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Flight|Speeds")
  float SpeedGlide = 4167.0f; // ~150 km/h

  /** Tier 2 — Boost: fast aircraft, auto-propelled. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Flight|Speeds")
  float SpeedBoost = 13889.0f; // ~500 km/h

  /** Tier 3 — Hypersonic, auto-propelled. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Flight|Speeds")
  float SpeedHypersonic = 138889.0f; // ~5,000 km/h

  /** Tier 4 — Rift Speed, auto-propelled. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Flight|Speeds")
  float SpeedRift = 1388889.0f; // ~50,000 km/h

  /** Height above terrain surface to hold during hover/boost/rift (cm). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Flight")
  float FlightAltitudeCm = 50000.0f;

private:
  bool bIsFlying = false;
  int32 CurrentSpeedTier = 0; // starts at Hover on first G press
  int32 WorldSeed = 0;

  // Distance / loop tracking
  FVector StartPosition = FVector::ZeroVector;
  FVector PreviousPosition = FVector::ZeroVector;
  float TotalDistanceCm = 0.0f;
  float FurthestFromStartCm = 0.0f;
  bool bCompletedLoop = false;
  bool bPassedQuarter = false;

  TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;

  /** Z position locked for Hover tier — captured when flight activates. */
  float HoverLockedZ = 0.f;

  /** LOD terrain shell — spawned at Tier 2+, destroyed below that. */
  TWeakObjectPtr<AFPMTerrainShell> TerrainShell;

  // ---- Helpers ---
  float GetCurrentSpeed() const;
  FString GetTierName() const;
  FString CalcCompassHeading(const FVector &Dir) const;

  void SpawnShellIfNeeded(ACharacter *Char);
  void DestroyShell();
};
