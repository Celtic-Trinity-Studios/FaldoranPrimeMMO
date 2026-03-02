// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// Water source actor -- placed in the world (procedurally or manually)
// to emit water into the flowing water simulation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/FPMWaterChunkData.h"
#include "FPMWaterSource.generated.h"

class UStaticMeshComponent;


/**
 * AFPMWaterSource
 *
 * A water source actor that emits water into the chunk-based
 * flowing water simulation. Can be placed manually in the editor
 * or spawned procedurally during world generation.
 *
 * The water simulation reads the source's flow rate and injects
 * water into the corresponding water grid cell each tick.
 *
 * Sources can be:
 *   - Springs (mountain/highland, moderate flow)
 *   - Rainfall zones (broad area, low flow per cell)
 *   - Cave exits (underground water emerging)
 *   - Artesian wells (pressurized upwelling)
 */
UCLASS(BlueprintType, Blueprintable)
class FALDORANPRIMEMMO_API AFPMWaterSource : public AActor {
  GENERATED_BODY()

public:
  AFPMWaterSource();

  /** Visible pool mesh representing the spring water */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPM|Water")
  UStaticMeshComponent *PoolMesh = nullptr;

  // --- Configuration ---

  /** Flow rate: how much water this source emits (cm^3/sec).
   *  Higher = wider/deeper river downstream.
   *  Typical spring: 30-80, major river source: 100-200. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  float FlowRate = 50.0f;

  /** Water temperature (C). Future use for biome effects
   *  (hot springs, frozen rivers). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  float Temperature = 15.0f;

  /** Type of water source (affects visual and sound effects).
   *  0=Spring, 1=Rainfall, 2=Cave, 3=Artesian */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  uint8 SourceType = 0; // EFPMWaterSourceType::Spring

  /** If true, this source never runs dry. If false, it has a finite
   *  water supply that depletes over time. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  bool bInfiniteSource = true;

  /** Total water volume available if not infinite (cm^3).
   *  Depletes at FlowRate per second. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water",
            meta = (EditCondition = "!bInfiniteSource"))
  float TotalVolume = 100000.0f;

  /** Current remaining volume (runtime state) */
  UPROPERTY(Transient, BlueprintReadOnly, Category = "FPM|Water")
  float RemainingVolume = 100000.0f;

  /** Whether this source is currently active (emitting water) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Water")
  bool bActive = true;

  // --- Blueprint Functions ---

  /** Get the effective flow rate (accounts for depletion) */
  UFUNCTION(BlueprintCallable, Category = "FPM|Water")
  float GetEffectiveFlowRate() const;

  /** Activate/deactivate this water source */
  UFUNCTION(BlueprintCallable, Category = "FPM|Water")
  void SetActive(bool bNewActive);

protected:
  virtual void BeginPlay() override;
  virtual void Tick(float DeltaTime) override;
};
