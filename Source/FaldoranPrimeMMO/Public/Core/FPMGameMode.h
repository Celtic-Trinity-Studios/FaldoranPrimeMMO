// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPMGameMode.generated.h"

/**
 * AFPMGameMode
 *
 * Base GameMode for Faldoran Prime. Runs on the dedicated server only.
 * Responsible for server lifecycle, player login flow, and pawn spawning
 * policy.
 *
 * IMPORTANT: DefaultPawnClass is set to nullptr. Players start in UI mode
 * (login screen) and the server spawns AFPMPlayerCharacter manually via
 * ServerRequestEnterWorld RPC when a character is selected.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMGameMode : public AGameModeBase {
  GENERATED_BODY()

public:
  AFPMGameMode();

  /** Called when the game starts. Logs server readiness. */
  virtual void InitGame(const FString &MapName, const FString &Options,
                        FString &ErrorMessage) override;

  /**
   * Override to prevent auto-spawning a pawn on login.
   * Players start in UI (login screen). Pawn spawned on character select.
   */
  virtual void PostLogin(APlayerController *NewPlayer) override;

  /**
   * Override to return nullptr — pawn class is determined at spawn time.
   */
  virtual UClass *GetDefaultPawnClassForController_Implementation(
      AController *InController) override;
};
