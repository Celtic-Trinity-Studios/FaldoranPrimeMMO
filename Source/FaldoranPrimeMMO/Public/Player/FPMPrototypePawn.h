// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FPMPrototypePawn.generated.h"

/**
 * AFPMPrototypePawn
 *
 * Temporary visible pawn for prototype networking tests (Phase 2).
 * Uses ACharacter for UCharacterMovementComponent (replicated movement).
 * Adds a simple sphere mesh so players can see each other.
 * Replaced by AFPMPlayerCharacter in Phase 6.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMPrototypePawn : public ACharacter {
  GENERATED_BODY()

public:
  AFPMPrototypePawn();

  virtual void Tick(float DeltaTime) override;

private:
  /** Visible sphere so players can see each other during prototype testing. */
  UPROPERTY(VisibleAnywhere)
  TObjectPtr<UStaticMeshComponent> VisualMesh;
};
