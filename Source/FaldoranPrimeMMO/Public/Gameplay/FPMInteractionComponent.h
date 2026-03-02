// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"


#include "FPMInteractionComponent.generated.h"

class IFPMInteractionInterface;

/**
 * Delegate fired when the interaction target changes (player looks at
 * a new interactable, or looks away from one).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFPMOnInteractTargetChanged,
                                            AActor *, NewTarget);

/**
 * UFPMInteractionComponent
 *
 * Attached to AFPMPlayerCharacter. Every tick, performs a short line
 * trace from the camera to detect interactable actors in front of the
 * player. When the player presses the interact key (E), calls
 * Interact() on the current target.
 *
 * The HUD binds to OnInteractTargetChanged to show/hide the prompt.
 *
 * See MasterPlan.md Phase IV, Step 4.1.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FALDORANPRIMEMMO_API UFPMInteractionComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UFPMInteractionComponent();

  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

  /**
   * Called by the input system when the player presses the interact key.
   * Attempts to interact with the current target.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Interaction")
  void TryInteract();

  /**
   * Get the currently focused interactable actor (may be null).
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Interaction")
  AActor *GetCurrentTarget() const { return CurrentTarget.Get(); }

  /**
   * Get the interaction prompt text for the current target.
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Interaction")
  FText GetCurrentPromptText() const;

  // --- Events ---

  /** Fired when the interaction target changes. Bind HUD to this. */
  UPROPERTY(BlueprintAssignable, Category = "FPM|Interaction")
  FFPMOnInteractTargetChanged OnInteractTargetChanged;

protected:
  /** Maximum interaction trace distance (cm). */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|Interaction")
  float TraceDistance = 400.0f;

  /** Trace channel to use for interaction detection. */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|Interaction")
  TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
  /** The actor currently under the player's crosshair. */
  TWeakObjectPtr<AActor> CurrentTarget;

  /** Perform the line trace and update CurrentTarget. */
  void UpdateTrace();
};
