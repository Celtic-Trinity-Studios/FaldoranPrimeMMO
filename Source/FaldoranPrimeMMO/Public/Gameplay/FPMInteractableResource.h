// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/FPMInteractionInterface.h"

#include "FPMInteractableResource.generated.h"

/**
 * AFPMInteractableResource
 *
 * Base class for world resources that the player can gather by
 * pressing the interact key (E). Examples: loose rocks, fallen
 * branches, herb nodes, ore deposits.
 *
 * When interacted with:
 *   1. Validates the player has an inventory component.
 *   2. Adds the configured item(s) to the player's inventory.
 *   3. Optionally destroys itself or transitions to a "depleted" state.
 *   4. Optionally respawns after a cooldown timer.
 *
 * Implements IFPMInteractionInterface so the player's interaction
 * system can detect and prompt it.
 *
 * See MasterPlan.md Phase IV, Step 4.1.
 */
UCLASS(Blueprintable)
class FALDORANPRIMEMMO_API AFPMInteractableResource
    : public AActor,
      public IFPMInteractionInterface {
  GENERATED_BODY()

public:
  AFPMInteractableResource();

  // --- IFPMInteractionInterface ---

  virtual bool Interact_Implementation(AActor *Interactor) override;
  virtual FText GetInteractText_Implementation() const override;
  virtual bool CanInteract_Implementation(AActor *Interactor) const override;

protected:
  /** The item ID to give the player on interaction. */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  FName GatherItemID = TEXT("Item_Rock");

  /** Number of items given per interaction. */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  int32 GatherAmount = 1;

  /** Interaction prompt text (shown on HUD). */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  FText InteractPrompt = FText::FromString(TEXT("Pick up Rock"));

  /** Whether to destroy the actor after gathering. */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  bool bDestroyOnGather = true;

  /** If not destroyed, cooldown before the resource can be gathered again. */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource",
            meta = (EditCondition = "!bDestroyOnGather"))
  float RespawnCooldown = 30.0f;

  /** Maximum interaction range (cm). */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  float InteractionRange = 300.0f;

  /** Static mesh representing the resource in the world. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPM|Resource")
  TObjectPtr<UStaticMeshComponent> ResourceMesh;

private:
  /** Whether this resource is currently on cooldown (depleted). */
  bool bDepleted = false;

  /** Handle for respawn timer. */
  FTimerHandle RespawnTimerHandle;

  /** Called when the respawn timer fires. */
  void OnRespawn();
};
