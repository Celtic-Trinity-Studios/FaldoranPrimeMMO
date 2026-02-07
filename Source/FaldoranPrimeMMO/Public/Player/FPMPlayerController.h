// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPMPlayerController.generated.h"


/**
 * AFPMPlayerController
 *
 * Base PlayerController for Faldoran Prime. Handles client connection,
 * UI management, and server RPCs for account/character flow (later phases).
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMPlayerController : public APlayerController {
  GENERATED_BODY()

public:
  AFPMPlayerController();

protected:
  /** Logs player connection on the server. */
  virtual void BeginPlay() override;
};
