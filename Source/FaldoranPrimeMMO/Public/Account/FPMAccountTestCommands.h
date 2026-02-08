// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMAccountTestCommands.generated.h"

/**
 * UFPMAccountTestCommands
 *
 * Registers console commands for testing the account subsystem:
 *   FPM.CreateAccount <username> <password> — Create a new account.
 *   FPM.Login <username> <password>         — Log in to an existing account.
 *
 * These commands only work on a dedicated server.
 * Will be stripped from shipping builds (see Production Security Roadmap).
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMAccountTestCommands : public UObject {
  GENERATED_BODY()

public:
  /** Register all FPM account console commands. Called at subsystem init. */
  static void RegisterCommands();

  /** Unregister all FPM account console commands. Called at subsystem shutdown.
   */
  static void UnregisterCommands();

private:
  static TArray<IConsoleObject *> RegisteredCommands;

  static void HandleCreateAccount(const TArray<FString> &Args, UWorld *World);
  static void HandleLogin(const TArray<FString> &Args, UWorld *World);
};
