// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPMGameMode.generated.h"

/**
 * AFPMGameMode
 *
 * Base GameMode for Faldoran Prime. Runs on the dedicated server only.
 * Responsible for server lifecycle, player login flow, and pawn spawning policy.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPMGameMode();

	/** Called when the game starts. Logs server readiness. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
