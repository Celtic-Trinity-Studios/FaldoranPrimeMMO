// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"


#include "FPMCharacterAnimInstance.generated.h"

/**
 * UFPMCharacterAnimInstance
 *
 * C++ AnimInstance base class for CC5 character Animation Blueprints.
 * Provides locomotion variables (Speed, Direction, IsJumping, IsFalling)
 * that the AnimGraph reads every frame to drive state machines and
 * blend spaces.
 *
 * Variables are updated in NativeUpdateAnimation() by reading from the
 * owning character's UCharacterMovementComponent. This ensures all
 * animation logic derives from server-authoritative movement state.
 *
 * The editor-side Animation Blueprint (ABP_CC5_Male / ABP_CC5_Female)
 * should use this class as its parent. The AnimGraph accesses these
 * properties directly — no Blueprint-side variable duplication needed.
 *
 * See Pillar_04_CC5_Character_Creation.md §3.4 — Animation Blueprint.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterAnimInstance : public UAnimInstance {
  GENERATED_BODY()

public:
  /** Called every frame to update animation variables from movement state. */
  virtual void NativeUpdateAnimation(float DeltaSeconds) override;

  /** Called when the anim instance is initialized (linked to a mesh). */
  virtual void NativeInitializeAnimation() override;

protected:
  // --- Locomotion Variables (read by AnimGraph) ---

  /** Current ground speed of the character (cm/s). Drives blend spaces. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  float Speed = 0.0f;

  /** Current movement direction relative to facing (-180 to 180 degrees). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  float Direction = 0.0f;

  /** True when the character is in a jump (ascending or at apex). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  bool bIsJumping = false;

  /** True when the character is falling (descending, not grounded). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  bool bIsFalling = false;

  /** True when the character is on the ground. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  bool bIsOnGround = true;

  /** True when the character has non-zero acceleration (player input). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Animation")
  bool bHasMovementInput = false;

private:
  /** Cached reference to the owning character (set in NativeInitialize). */
  UPROPERTY()
  TWeakObjectPtr<ACharacter> OwningCharacter;
};
