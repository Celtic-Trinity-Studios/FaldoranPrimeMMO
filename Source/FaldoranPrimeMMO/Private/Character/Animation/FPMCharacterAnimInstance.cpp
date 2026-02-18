// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/Animation/FPMCharacterAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterAnim, Log, All);

// -------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------

void UFPMCharacterAnimInstance::NativeInitializeAnimation() {
  Super::NativeInitializeAnimation();

  // Cache the owning character reference once at init rather than
  // looking it up every frame via TryGetPawnOwner().
  APawn *Pawn = TryGetPawnOwner();
  if (Pawn) {
    OwningCharacter = Cast<ACharacter>(Pawn);
  }

  if (!OwningCharacter.IsValid()) {
    UE_LOG(LogFPMCharacterAnim, Warning,
           TEXT("FPM: AnimInstance initialized without a valid ACharacter "
                "owner. Animation variables will not update."));
  }
}

// -------------------------------------------------------------------
// Per-Frame Update
// -------------------------------------------------------------------

void UFPMCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
  Super::NativeUpdateAnimation(DeltaSeconds);

  if (!OwningCharacter.IsValid()) {
    return;
  }

  ACharacter *Character = OwningCharacter.Get();
  UCharacterMovementComponent *Movement = Character->GetCharacterMovement();
  if (!Movement) {
    return;
  }

  // --- Speed (ground speed magnitude, ignoring vertical velocity) ---
  const FVector Velocity = Movement->Velocity;
  Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

  // --- Direction (angle between velocity and character facing) ---
  // Calculate the angle between the horizontal velocity vector and the
  // character's forward direction. Result is in degrees, -180 to +180.
  if (Speed > 1.0f) {
    const FVector Forward = Character->GetActorForwardVector();
    const FVector Right = Character->GetActorRightVector();
    const FVector VelocityNormal =
        FVector(Velocity.X, Velocity.Y, 0.0f).GetSafeNormal();

    const float ForwardDot = FVector::DotProduct(Forward, VelocityNormal);
    const float RightDot = FVector::DotProduct(Right, VelocityNormal);

    Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
  } else {
    Direction = 0.0f;
  }

  // --- Ground / Air State ---
  bIsOnGround = Movement->IsMovingOnGround();
  bIsFalling = Movement->IsFalling();

  // Jumping = in air AND still moving upward (ascending phase)
  bIsJumping = bIsFalling && (Velocity.Z > 0.0f);

  // --- Movement Input ---
  // Non-zero acceleration indicates the player is pressing a movement key.
  bHasMovementInput = Movement->GetCurrentAcceleration().SizeSquared() > 0.0f;
}
