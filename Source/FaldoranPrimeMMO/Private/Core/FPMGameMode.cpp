// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Core/FPMGameMode.h"
#include "Player/FPMPlayerController.h"
#include "Player/FPMPrototypePawn.h"


AFPMGameMode::AFPMGameMode() {
  // AFPMPrototypePawn: ACharacter subclass with a visible sphere mesh.
  // ACharacter provides UCharacterMovementComponent (replicated movement).
  // Replaced with AFPMPlayerCharacter in Phase 6.
  DefaultPawnClass = AFPMPrototypePawn::StaticClass();

  // Route all connecting clients through our custom PlayerController
  PlayerControllerClass = AFPMPlayerController::StaticClass();
}

void AFPMGameMode::InitGame(const FString &MapName, const FString &Options,
                            FString &ErrorMessage) {
  Super::InitGame(MapName, Options, ErrorMessage);

  UE_LOG(LogTemp, Log, TEXT("FPM: Server started, waiting for connections"));
}
