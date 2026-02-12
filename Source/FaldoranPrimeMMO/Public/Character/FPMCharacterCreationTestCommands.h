// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPMCharacterCreationTestCommands.generated.h"

/**
 * UFPMCharacterCreationTestCommands
 *
 * Registers console commands for testing the character creation subsystem:
 *   FPM.TestCreateCharacter <name> — Create a character with default
 * appearance.
 *
 * These commands only work on a dedicated server (or PIE server world).
 * They use a hardcoded test account ID for convenience.
 * Will be stripped from shipping builds (see Production Security Roadmap).
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterCreationTestCommands : public UObject {
  GENERATED_BODY()

public:
  /** Register all FPM character creation console commands. */
  static void RegisterCommands();

  /** Unregister all FPM character creation console commands. */
  static void UnregisterCommands();

private:
  static TArray<IConsoleObject *> RegisteredCommands;

  /**
   * Handle: FPM.TestCreateCharacter <name>
   * Creates a character with the given name and default appearance values.
   * Uses the first account in the database as the test account.
   */
  static void HandleTestCreateCharacter(const TArray<FString> &Args,
                                        UWorld *World);

  /**
   * Handle: FPM.TestCreateCharacterWithAffinities
   * Creates a character with default appearance and a test affinity
   * distribution (non-default) for both playstyle and magical pools.
   */
  static void
  HandleTestCreateCharacterWithAffinities(const TArray<FString> &Args,
                                          UWorld *World);
};
