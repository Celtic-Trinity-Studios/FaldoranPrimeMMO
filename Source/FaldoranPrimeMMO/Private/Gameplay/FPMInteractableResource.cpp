// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Gameplay/FPMInteractableResource.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "TimerManager.h"

AFPMInteractableResource::AFPMInteractableResource() {
  PrimaryActorTick.bCanEverTick = false;
  bReplicates = true;

  // Create a default static mesh component as root
  ResourceMesh =
      CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
  RootComponent = ResourceMesh;

  // Enable overlap for interaction detection
  ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  ResourceMesh->SetCollisionResponseToAllChannels(
      ECollisionResponse::ECR_Block);
  ResourceMesh->SetGenerateOverlapEvents(true);
}

// -------------------------------------------------------------------
// IFPMInteractionInterface
// -------------------------------------------------------------------

bool AFPMInteractableResource::Interact_Implementation(AActor *Interactor) {
  if (!Interactor || bDepleted)
    return false;

  // Server-only: validate and process
  if (!HasAuthority())
    return false;

  // Find the inventory component on the interacting actor
  UFPMInventoryComponent *Inventory =
      Interactor->FindComponentByClass<UFPMInventoryComponent>();

  if (!Inventory) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM Resource: Interactor %s has no inventory component!"),
           *Interactor->GetName());
    return false;
  }

  // Add items to the player's inventory
  const int32 Added = Inventory->AddItem(GatherItemID, GatherAmount);

  if (Added <= 0) {
    UE_LOG(LogTemp, Log,
           TEXT("FPM Resource: Inventory full — could not add %s"),
           *GatherItemID.ToString());
    return false;
  }

  UE_LOG(LogTemp, Log, TEXT("FPM Resource: Player gathered %d x %s"), Added,
         *GatherItemID.ToString());

  if (bDestroyOnGather) {
    Destroy();
  } else {
    // Enter depleted state and start respawn timer
    bDepleted = true;
    ResourceMesh->SetVisibility(false);
    ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
                                    &AFPMInteractableResource::OnRespawn,
                                    RespawnCooldown, false);
  }

  return true;
}

FText AFPMInteractableResource::GetInteractText_Implementation() const {
  return InteractPrompt;
}

bool AFPMInteractableResource::CanInteract_Implementation(
    AActor *Interactor) const {
  if (bDepleted)
    return false;
  if (!Interactor)
    return false;

  // Range check
  const float Dist =
      FVector::Dist(GetActorLocation(), Interactor->GetActorLocation());
  return Dist <= InteractionRange;
}

// -------------------------------------------------------------------
// Respawn
// -------------------------------------------------------------------

void AFPMInteractableResource::OnRespawn() {
  bDepleted = false;
  ResourceMesh->SetVisibility(true);
  ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  UE_LOG(LogTemp, Log, TEXT("FPM Resource: %s has respawned."), *GetName());
}
