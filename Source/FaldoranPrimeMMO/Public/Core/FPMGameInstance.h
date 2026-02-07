// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FPMGameInstance.generated.h"

/**
 * UFPMGameInstance
 *
 * Custom GameInstance for Faldoran Prime. Persists across map transitions.
 * Houses game-wide subsystems (Database, Account, etc.) in later phases.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMGameInstance : public UGameInstance {
  GENERATED_BODY()

public:
  /** Called when the GameInstance is created. Logs initialization. */
  virtual void Init() override;
};
