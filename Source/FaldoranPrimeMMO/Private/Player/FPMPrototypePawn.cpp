// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPrototypePawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"

AFPMPrototypePawn::AFPMPrototypePawn() {
  // Attach a visible sphere to the capsule so other players can see this pawn
  VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
  VisualMesh->SetupAttachment(GetCapsuleComponent());

  static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(
      TEXT("/Engine/BasicShapes/Sphere.Sphere"));

  if (SphereMeshAsset.Succeeded()) {
    VisualMesh->SetStaticMesh(SphereMeshAsset.Object);
    VisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -30.0f));
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  }
}

void AFPMPrototypePawn::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // Only process input on the locally controlled client
  if (!IsLocallyControlled()) {
    return;
  }

  APlayerController *PC = Cast<APlayerController>(GetController());
  if (!PC) {
    return;
  }

  // --- WASD Movement ---
  // Feeds into UCharacterMovementComponent which handles server replication
  FVector MoveInput = FVector::ZeroVector;
  if (PC->IsInputKeyDown(EKeys::W))
    MoveInput.X += 1.0f;
  if (PC->IsInputKeyDown(EKeys::S))
    MoveInput.X -= 1.0f;
  if (PC->IsInputKeyDown(EKeys::D))
    MoveInput.Y += 1.0f;
  if (PC->IsInputKeyDown(EKeys::A))
    MoveInput.Y -= 1.0f;

  if (!MoveInput.IsZero()) {
    const FRotator ControlRot = PC->GetControlRotation();
    const FRotator YawOnlyRot(0.0f, ControlRot.Yaw, 0.0f);

    const FVector ForwardDir =
        FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDir, MoveInput.X);
    AddMovementInput(RightDir, MoveInput.Y);
  }

  // --- Mouse Look ---
  float MouseX = 0.0f;
  float MouseY = 0.0f;
  PC->GetInputMouseDelta(MouseX, MouseY);
  AddControllerYawInput(MouseX);
  AddControllerPitchInput(-MouseY);
}
