// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Character/FPMCharacterCreationDataContract.h"
#include "Character/FPMSpeciesDataAsset.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "FPMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFPMInventoryComponent;
class UFPMInteractionComponent;

/**
 * AFPMPlayerCharacter
 *
 * The player's in-world character pawn. Spawned by the server when a
 * player selects a character and enters the world. Replicated properties
 * carry appearance data so all clients can render the correct look.
 *
 * Uses CC5 (Character Creator 5) skeletal meshes with morph target support.
 * Male/Female mesh selected based on BodyType (0=Male, 1=Female).
 *
 * Camera: Third-person spring arm + camera component.
 * Movement: Delegates to UCharacterMovementComponent (built into ACharacter).
 *
 * IMPORTANT: Only the server may spawn and possess this character.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMPlayerCharacter : public ACharacter {
  GENERATED_BODY()

public:
  AFPMPlayerCharacter();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  virtual void Tick(float DeltaTime) override;

  // --- Server-side setters ---

  // --- Species Data ---

  /**
   * Registry containing all species data assets.
   * Assign in the BP_PlayerCharacter class defaults (Details panel).
   * When null, the character falls back to hardcoded scaling values.
   *
   * Editor path: create DA_SpeciesRegistry in Content/Data/Species/,
   * populate it with one DA_Species_* asset per playable species,
   * then assign it here.
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Species")
  TObjectPtr<UFPMSpeciesRegistry> SpeciesRegistry;

  /** Called by the server after spawning to apply loaded character data. */
  void InitializeAppearance(const FString &InName, uint8 InSpecies,
                            uint8 InBodyType, const FLinearColor &InSkinTone,
                            const FLinearColor &InEyeColor,
                            const FLinearColor &InHairColor, float InMorphJaw,
                            float InMorphNose, float InMorphBrow,
                            float InMorphLips);

  /** True while in flight mode (F key toggle). Used by HUD and other systems.
   */
  UFUNCTION(BlueprintPure, Category = "FPM|Locomotion")
  bool IsFlying() const { return bIsFlying; }

  /**
   * Whether HUD cursor mode is active (Tab toggle).
   * Public so FPMHUD can read it without a function call overhead.
   */
  bool bHUDMouseMode = false;

  /** True while Left Shift is held and the character is on the ground. */
  UFUNCTION(BlueprintPure, Category = "FPM|Locomotion")
  bool IsRunning() const { return bIsRunning; }

protected:
  virtual void BeginPlay() override;

private:
  // --- CC5 Mesh Assets (loaded in constructor) ---

  /** Male CC5 skeletal mesh reference. */
  UPROPERTY()
  TObjectPtr<USkeletalMesh> CC5MaleMesh;

  /** Female CC5 skeletal mesh reference. */
  UPROPERTY()
  TObjectPtr<USkeletalMesh> CC5FemaleMesh;

  /** Idle animation played in single-node mode. */
  UPROPERTY()
  TObjectPtr<UAnimationAsset> CC5IdleAnimation;

  // Morph target names matching CC5 morph targets
  static const FName MorphName_Jaw;
  static const FName MorphName_Nose;
  static const FName MorphName_Brow;
  static const FName MorphName_Lips;

  // --- Camera Setup ---

  UPROPERTY(VisibleAnywhere, Category = "FPM|Camera")
  TObjectPtr<USpringArmComponent> CameraBoom;

  UPROPERTY(VisibleAnywhere, Category = "FPM|Camera")
  TObjectPtr<UCameraComponent> FollowCamera;

  // --- Gameplay Components ---

  /** Server-authoritative inventory (40-slot grid, replicated to owner). */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Gameplay")
  TObjectPtr<UFPMInventoryComponent> InventoryComponent;

  /** Interaction detector — line traces from camera to find interactables. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Gameplay")
  TObjectPtr<UFPMInteractionComponent> InteractionComponent;

  // --- Replicated Appearance ---

  UPROPERTY(ReplicatedUsing = OnRep_CharacterName)
  FString CharacterName;

  UPROPERTY(ReplicatedUsing = OnRep_BodyType)
  uint8 BodyType = 0;

  UPROPERTY(ReplicatedUsing = OnRep_Species)
  uint8 Species = 0;

  UPROPERTY(ReplicatedUsing = OnRep_SkinTone)
  FLinearColor SkinTone = FLinearColor(0.8f, 0.6f, 0.5f, 1.0f);

  UPROPERTY(ReplicatedUsing = OnRep_EyeColor)
  FLinearColor EyeColor = FLinearColor(0.3f, 0.5f, 0.8f, 1.0f);

  UPROPERTY(ReplicatedUsing = OnRep_HairColor)
  FLinearColor HairColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.0f);

  // Facial morph targets (0.0 to 1.0, 0.5 = neutral)
  UPROPERTY(ReplicatedUsing = OnRep_FacialMorphs)
  float MorphJaw = 0.5f;

  UPROPERTY(ReplicatedUsing = OnRep_FacialMorphs)
  float MorphNose = 0.5f;

  UPROPERTY(ReplicatedUsing = OnRep_FacialMorphs)
  float MorphBrow = 0.5f;

  UPROPERTY(ReplicatedUsing = OnRep_FacialMorphs)
  float MorphLips = 0.5f;

  // --- OnRep Callbacks (client-side visual updates) ---

  UFUNCTION()
  void OnRep_CharacterName();

  UFUNCTION()
  void OnRep_BodyType();

  UFUNCTION()
  void OnRep_Species();

  UFUNCTION()
  void OnRep_SkinTone();

  UFUNCTION()
  void OnRep_EyeColor();

  UFUNCTION()
  void OnRep_HairColor();

  UFUNCTION()
  void OnRep_FacialMorphs();

  /** Apply current appearance values to the mesh/materials. */
  void ApplyAppearance();

  /** Apply species-specific scaling (mesh, capsule, speed, camera). */
  void ApplySpeciesScaling();

  // --- Input (local player only) ---

  // --- Running State ---

  /**
   * True while Left Shift is held on the ground.
   * Speed toggles between CachedWalkSpeed and CachedRunSpeed.
   * Will be gated by carry weight and stamina in a future sprint.
   */
  bool bIsRunning = false;

  /**
   * Walk/run speeds cached from ApplySpeciesScaling() so we can switch
   * between them instantly without re-reading the data asset every frame.
   */
  float CachedWalkSpeed = 130.0f;
  float CachedRunSpeed = 260.0f;

  /** Whether the character is in flight mode (toggled by F key). */
  bool bIsFlying = false;

  /** Toggle flight mode on/off. */
  void ToggleFlight();

  /** Attempt to interact with the object under the crosshair (E key). */
  void TryInteract();

  /** Toggle inventory UI visibility (I key). */
  void ToggleInventory();

  /** Whether the inventory UI is currently visible. */
  bool bInventoryOpen = false;

  /** Process WASD movement input. */
  void HandleMovementInput(float DeltaTime);

  /** Cooldown for fall-through recovery (prevents frame-spam). */
  double LastFallRecoveryTime = 0.0;
};
