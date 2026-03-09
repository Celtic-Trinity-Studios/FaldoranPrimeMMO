// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Core/FPMGameMode.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "Player/FPMPlayerCharacter.h"
#include "Player/FPMPlayerController.h"
#include "UI/FPMHUD.h"
#include "World/FPMWorldChunkManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMGameMode, Log, All);

AFPMGameMode::AFPMGameMode() {
  // No default pawn — players start in UI mode (login screen).
  // The server spawns AFPMPlayerCharacter when a character is selected.
  DefaultPawnClass = nullptr;

  // Route all connecting clients through our custom PlayerController
  PlayerControllerClass = AFPMPlayerController::StaticClass();

  // Use our custom Canvas-drawn HUD
  HUDClass = AFPMHUD::StaticClass();
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

void AFPMGameMode::Logout(AController *Exiting) {
  // -----------------------------------------------------------------------
  //  AUTO-SAVE POSITION ON EVERY DISCONNECT
  //
  //  This fires when the player leaves for ANY reason:
  //    - Clicked "Logout & Save" in the ESC menu
  //    - Stopped PIE in the editor
  //    - Network timeout / connection drop
  //    - Alt+F4 / crash (best-effort, may not always fire on hard crash)
  //
  //  We save here AND in ServerSaveAndLogout so that the explicit logout
  //  path already has the position written before Logout() fires.
  //  Double-writing the same value is harmless.
  // -----------------------------------------------------------------------

  if (AFPMPlayerController *PC = Cast<AFPMPlayerController>(Exiting)) {
    // Only save if this controller had an authenticated character in-world
    if (PC->IsAuthenticated() && PC->GetActiveCharacterId().IsValid()) {
      APawn *OwningPawn = PC->GetPawn();
      if (OwningPawn) {
        const FVector PawnLoc = OwningPawn->GetActorLocation();
        UGameInstance *GI = GetGameInstance();
        UFPMDatabaseSubsystem *DB =
            GI ? GI->GetSubsystem<UFPMDatabaseSubsystem>() : nullptr;

        if (DB && DB->IsConnected()) {
          const FString CId = PC->GetActiveCharacterId().ToString(
              EGuidFormats::DigitsWithHyphensLower);
          const FString XStr = FString::SanitizeFloat(PawnLoc.X);
          const FString YStr = FString::SanitizeFloat(PawnLoc.Y);
          const FString ZStr = FString::SanitizeFloat(PawnLoc.Z);

          const FFPMDatabaseQueryResult R = DB->ExecuteQuery(
              TEXT("UPDATE characters "
                   "SET spawn_x = $1, spawn_y = $2, spawn_z = $3, "
                   "last_played = NOW() "
                   "WHERE character_id = $4"),
              {XStr, YStr, ZStr, CId});

          if (R.bSuccess) {
            UE_LOG(LogFPMGameMode, Log,
                   TEXT("FPM Logout: Saved position (%.0f, %.0f, %.0f) "
                        "for character %s"),
                   PawnLoc.X, PawnLoc.Y, PawnLoc.Z, *CId);

            // Save inventory alongside position so both are always in sync.
            if (AFPMPlayerCharacter *FPMChar =
                    Cast<AFPMPlayerCharacter>(OwningPawn)) {
              if (UFPMInventoryComponent *Inv =
                      FPMChar->GetInventoryComponent()) {
                if (!Inv->SaveToDB(DB, PC->GetActiveCharacterId())) {
                  UE_LOG(LogFPMGameMode, Warning,
                         TEXT("FPM Logout: Inventory save had failures for %s."),
                         *CId);
                }
              }
            }
          } else {
            UE_LOG(LogFPMGameMode, Warning,
                   TEXT("FPM Logout: DB save failed for %s — %s"), *CId,
                   *R.ErrorMessage);
          }
        } else {
          UE_LOG(LogFPMGameMode, Warning,
                 TEXT("FPM Logout: DB unavailable, position not saved."));
        }
      }
    }
  }

  Super::Logout(Exiting);
}
