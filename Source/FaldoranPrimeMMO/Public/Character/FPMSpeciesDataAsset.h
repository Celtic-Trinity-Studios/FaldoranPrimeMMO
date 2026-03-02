// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "FPMSpeciesDataAsset.generated.h"

/**
 * UFPMSpeciesDataAsset
 *
 * Data-driven species definition. Each playable species (Human, Elf,
 * Dwarf, Giant, etc.) has one of these assets in the Content Browser.
 *
 * Replaces the old hardcoded FSpeciesScaling static array with a
 * designer-editable data asset. Changes take effect immediately
 * without recompiling.
 *
 * Usage:
 *   1. Create a DA_Species_Human (etc.) in Content Browser.
 *   2. Assign to AFPMWorldChunkManager or a global config.
 *   3. AFPMPlayerCharacter reads from this instead of the static array.
 *
 * See MasterPlan.md Phase III, Step 3.1.
 */
UCLASS(BlueprintType)
class FALDORANPRIMEMMO_API UFPMSpeciesDataAsset : public UPrimaryDataAsset {
  GENERATED_BODY()

public:
  // --- Identity ---

  /** Which species enum this asset defines. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species")
  EFPMSpecies SpeciesID = EFPMSpecies::Human;

  /** Display name shown in character creation UI. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species")
  FText DisplayName;

  /** Short lore description for the character creation screen. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species",
            meta = (MultiLine = true))
  FText LoreDescription;

  // --- Physical Scaling ---

  /** Uniform scale applied to the skeletal mesh. 1.0 = Human reference. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species|Scale")
  float MeshScale = 1.0f;

  /** Capsule half-height in cm. Default 90 matches Human. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species|Scale")
  float CapsuleHalfHeight = 90.0f;

  /** Capsule radius in cm. Default 34 matches Human. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species|Scale")
  float CapsuleRadius = 34.0f;

  // --- Movement ---

  /**
   * Base walking speed in cm/s.
   * Reference: average adult walk ≈ 1.3 m/s (3 mph).
   * Preferred range 1.10–1.65 m/s; 130 cm/s sits in the middle.
   * This is the speed when W is held without Shift.
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Movement")
  float BaseWalkSpeed = 130.0f;

  /**
   * Running speed in cm/s (hold Left Shift while on the ground).
   * Reference: typical ~3 mph walk → ~6 mph jog is ≈ 2× speed increase.
   * 260 cm/s = 2.6 m/s ≈ 6 mph. Skills and carry weight will modify this later.
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Movement")
  float BaseRunSpeed = 260.0f;

  /**
   * Multiplier applied to JumpZVelocity (300 cm/s base → ~46cm jump).
   * Reference: average adult vertical jump 41–50 cm.
   * Values > 1.0 jump higher (e.g. Halflings at 1.15 → ~60cm).
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Movement")
  float JumpMultiplier = 1.0f;

  // --- Camera ---

  /** Camera boom arm length. Larger species need a longer boom. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Camera")
  float CameraBoomLength = 400.0f;

  // --- Gameplay ---

  /** Base health multiplier (relative to Human = 1.0). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Gameplay")
  float BaseHealthMultiplier = 1.0f;

  /** Carry weight multiplier (relative to Human = 1.0). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Gameplay")
  float CarryWeightMultiplier = 1.0f;

  // --- Default Morph Targets ---

  /** Default morph target overrides for this species.
   *  Applied on top of player customization.
   *  Key = morph target name, Value = default value (0.0-1.0). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
            Category = "FPM|Species|Morphs")
  TMap<FName, float> DefaultMorphTargets;

  // --- UPrimaryDataAsset Interface ---

  virtual FPrimaryAssetId GetPrimaryAssetId() const override {
    return FPrimaryAssetId(TEXT("SpeciesData"), GetFName());
  }
};

/**
 * UFPMSpeciesRegistry
 *
 * Singleton-style data asset that holds references to ALL species
 * data assets. Assigned to the WorldChunkManager or GameInstance.
 * Provides O(1) lookup by EFPMSpecies enum.
 */
UCLASS(BlueprintType)
class FALDORANPRIMEMMO_API UFPMSpeciesRegistry : public UDataAsset {
  GENERATED_BODY()

public:
  /** All species data assets, indexed for quick lookup. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species")
  TArray<TObjectPtr<UFPMSpeciesDataAsset>> Species;

  /** Find species data by enum value. Returns nullptr if not found. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Species")
  UFPMSpeciesDataAsset *FindSpecies(EFPMSpecies SpeciesID) const {
    for (const auto &S : Species) {
      if (S && S->SpeciesID == SpeciesID) {
        return S;
      }
    }
    return nullptr;
  }
};
