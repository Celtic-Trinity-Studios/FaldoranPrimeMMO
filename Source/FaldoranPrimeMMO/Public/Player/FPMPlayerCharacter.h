// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

class UStaticMeshComponent;

#include "FPMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * AFPMPlayerCharacter
 *
 * The player's in-world character pawn. Spawned by the server when a
 * player selects a character and enters the world. Replicated properties
 * carry appearance data so all clients can render the correct look.
 *
 * Uses UE5 mannequin mesh (SKM_Manny) as placeholder until Mutable/CC5.
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

  /** Called by the server after spawning to apply loaded character data. */
  void InitializeAppearance(const FString &InName, uint8 InBodyType,
                            const FLinearColor &InSkinTone,
                            const FLinearColor &InHairColor);

protected:
  virtual void BeginPlay() override;

private:
  // --- Visible Body Mesh ---

  /** Simple sphere mesh for prototype visibility. Replaced by skeletal mesh
   * later. */
  UPROPERTY(VisibleAnywhere, Category = "FPM|Visual")
  TObjectPtr<UStaticMeshComponent> BodyMesh;

  // --- Camera Setup ---

  UPROPERTY(VisibleAnywhere, Category = "FPM|Camera")
  TObjectPtr<USpringArmComponent> CameraBoom;

  UPROPERTY(VisibleAnywhere, Category = "FPM|Camera")
  TObjectPtr<UCameraComponent> FollowCamera;

  // --- Replicated Appearance ---

  UPROPERTY(ReplicatedUsing = OnRep_CharacterName)
  FString CharacterName;

  UPROPERTY(ReplicatedUsing = OnRep_BodyType)
  uint8 BodyType = 0;

  UPROPERTY(ReplicatedUsing = OnRep_SkinTone)
  FLinearColor SkinTone = FLinearColor(0.8f, 0.6f, 0.5f, 1.0f);

  UPROPERTY(ReplicatedUsing = OnRep_HairColor)
  FLinearColor HairColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.0f);

  // --- OnRep Callbacks (client-side visual updates) ---

  UFUNCTION()
  void OnRep_CharacterName();

  UFUNCTION()
  void OnRep_BodyType();

  UFUNCTION()
  void OnRep_SkinTone();

  UFUNCTION()
  void OnRep_HairColor();

  /** Apply current appearance values to the mesh/materials. */
  void ApplyAppearance();

  // --- Input (local player only) ---

  /** Process WASD movement input. */
  void HandleMovementInput(float DeltaTime);
};
