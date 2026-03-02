// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "FPMInteractionInterface.generated.h"

/**
 * IFPMInteractionInterface
 *
 * Interface for world actors that the player can interact with.
 * Any actor implementing this interface will respond to the player's
 * interaction input (default: E key).
 *
 * The interaction system performs a line trace from the camera,
 * checks for actors implementing this interface within range,
 * and calls Interact() on the best candidate.
 *
 * See MasterPlan.md Phase IV, Step 4.1.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UFPMInteractionInterface : public UInterface {
  GENERATED_BODY()
};

class FALDORANPRIMEMMO_API IFPMInteractionInterface {
  GENERATED_BODY()

public:
  /**
   * Called when the player presses the interact key on this actor.
   * @param Interactor The character performing the interaction.
   * @return true if the interaction was consumed, false to pass through.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
            Category = "FPM|Interaction")
  bool Interact(AActor *Interactor);

  /**
   * Get the text displayed on the interaction prompt HUD element.
   * e.g., "Pick up Rock", "Open Door", "Talk to NPC"
   * @return The interaction prompt text.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
            Category = "FPM|Interaction")
  FText GetInteractText() const;

  /**
   * Whether this actor can currently be interacted with.
   * Used to filter out depleted resources, locked doors, etc.
   * @param Interactor The character attempting interaction.
   * @return true if interaction is available.
   */
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
            Category = "FPM|Interaction")
  bool CanInteract(AActor *Interactor) const;
};
