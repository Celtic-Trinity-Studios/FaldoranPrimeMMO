// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Core/FPMGameMode.h"
#include "Player/FPMPlayerController.h"
#include "World/FPMWorldChunkManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMGameMode, Log, All);

AFPMGameMode::AFPMGameMode() {
  // No default pawn — players start in UI mode (login screen).
  // The server spawns AFPMPlayerCharacter when a character is selected.
  DefaultPawnClass = nullptr;

  // Route all connecting clients through our custom PlayerController
  PlayerControllerClass = AFPMPlayerController::StaticClass();
}

void AFPMGameMode::InitGame(const FString &MapName, const FString &Options,
                            FString &ErrorMessage) {
  Super::InitGame(MapName, Options, ErrorMessage);

  UE_LOG(LogFPMGameMode, Log,
         TEXT("FPM: Server started, waiting for connections"));

  // Spawn the WorldChunkManager for the server world.
  // This is the single authoritative instance. Clients spawn their own copy
  // independently via PlayerController::ClientEnterWorldSuccess.
  AFPMWorldChunkManager::GetOrCreate(GetWorld());
}

void AFPMGameMode::PostLogin(APlayerController *NewPlayer) {
  // Call Super but skip the auto-pawn-spawn by having DefaultPawnClass=nullptr.
  // The player stays in UI mode until they select a character.
  Super::PostLogin(NewPlayer);

  UE_LOG(LogFPMGameMode, Log,
         TEXT("FPM: Player logged in — awaiting character select "
              "(no pawn spawned)."));
}

UClass *AFPMGameMode::GetDefaultPawnClassForController_Implementation(
    AController *InController) {
  // Return nullptr to explicitly prevent pawn spawning on login.
  // AFPMPlayerCharacter is spawned by ServerRequestEnterWorld.
  return nullptr;
}
