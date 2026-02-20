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
 *
 * Networking:
 *   Reads [FPM.Server] from DefaultGame.ini. When bAutoConnect is true
 *   AND the game is running as a Client target (not editor/standalone),
 *   automatically performs ClientTravel to the configured server IP.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMGameInstance : public UGameInstance {
  GENERATED_BODY()

public:
  /** Called when the GameInstance is created. Logs initialization. */
  virtual void Init() override;

  // --- Server Connection Settings (read from DefaultGame.ini) ---

  /** Target dedicated server IP address */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Network")
  FString ServerIP = TEXT("127.0.0.1");

  /** Target dedicated server port */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Network")
  int32 ServerPort = 7777;

  /** Whether to auto-connect to the dedicated server on startup.
   *  Set to true for packaged client builds, false for PIE/editor. */
  UPROPERTY(BlueprintReadWrite, Category = "FPM|Network")
  bool bAutoConnect = false;

  /** Manually trigger connection to the dedicated server.
   *  Called automatically if bAutoConnect is true, or can be called
   *  from a UI "Connect" button. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Network")
  void ConnectToDedicatedServer();
};
