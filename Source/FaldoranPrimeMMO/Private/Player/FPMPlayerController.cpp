// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerController.h"

AFPMPlayerController::AFPMPlayerController() {
  // bShowMouseCursor will be enabled in Phase 4 when login UI is added.
}

void AFPMPlayerController::BeginPlay() {
  Super::BeginPlay();

  // Only log on the server to avoid duplicate messages from each client
  if (HasAuthority()) {
    UE_LOG(LogTemp, Log, TEXT("FPM: Player connected"));
  }
}
