// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMDatabaseTestCommands.generated.h"

/**
 * UFPMDatabaseTestCommands
 *
 * Registers console commands for testing the database subsystem:
 *   FPM.TestDBConnect  — Connects to the database and logs the result.
 *   FPM.TestDBWrite    — Inserts a test row into a db_test table.
 *   FPM.TestDBRead     — Reads all rows from db_test and logs them.
 *
 * These commands only work on a dedicated server.
 * Will be removed or disabled once the Account subsystem is operational.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMDatabaseTestCommands : public UObject {
  GENERATED_BODY()

public:
  /** Register all FPM.TestDB* console commands. Called once at subsystem init.
   */
  static void RegisterCommands();

  /** Unregister all FPM.TestDB* console commands. Called at subsystem shutdown.
   */
  static void UnregisterCommands();

private:
  static TArray<IConsoleObject *> RegisteredCommands;

  static void HandleTestDBConnect(const TArray<FString> &Args, UWorld *World);
  static void HandleTestDBWrite(const TArray<FString> &Args, UWorld *World);
  static void HandleTestDBRead(const TArray<FString> &Args, UWorld *World);
};
