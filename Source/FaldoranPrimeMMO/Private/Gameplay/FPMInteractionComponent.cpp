// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Gameplay/FPMInteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/FPMInteractionInterface.h"


UFPMInteractionComponent::UFPMInteractionComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz is enough for traces
}

void UFPMInteractionComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Only local player needs interaction traces
  APawn *Owner = Cast<APawn>(GetOwner());
  if (!Owner || !Owner->IsLocallyControlled())
    return;

  UpdateTrace();
}

void UFPMInteractionComponent::UpdateTrace() {
  APawn *Owner = Cast<APawn>(GetOwner());
  if (!Owner)
    return;

  APlayerController *PC = Cast<APlayerController>(Owner->GetController());
  if (!PC)
    return;

  // Get camera viewpoint
  FVector CameraLoc;
  FRotator CameraRot;
  PC->GetPlayerViewPoint(CameraLoc, CameraRot);

  const FVector TraceEnd = CameraLoc + CameraRot.Vector() * TraceDistance;

  FHitResult Hit;
  FCollisionQueryParams Params;
  Params.AddIgnoredActor(Owner);

  AActor *NewTarget = nullptr;

  if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd,
                                           TraceChannel, Params)) {
    AActor *HitActor = Hit.GetActor();
    if (HitActor && HitActor->Implements<UFPMInteractionInterface>()) {
      // Check if the interactable allows interaction right now
      if (IFPMInteractionInterface::Execute_CanInteract(HitActor, Owner)) {
        NewTarget = HitActor;
      }
    }
  }

  // Fire delegate if target changed
  if (NewTarget != CurrentTarget.Get()) {
    CurrentTarget = NewTarget;
    OnInteractTargetChanged.Broadcast(NewTarget);
  }
}

void UFPMInteractionComponent::TryInteract() {
  AActor *Target = CurrentTarget.Get();
  if (!Target)
    return;

  APawn *Owner = Cast<APawn>(GetOwner());
  if (!Owner)
    return;

  // Call Interact on the target (handles server authority internally)
  IFPMInteractionInterface::Execute_Interact(Target, Owner);
}

FText UFPMInteractionComponent::GetCurrentPromptText() const {
  AActor *Target = CurrentTarget.Get();
  if (!Target)
    return FText::GetEmpty();

  return IFPMInteractionInterface::Execute_GetInteractText(Target);
}
