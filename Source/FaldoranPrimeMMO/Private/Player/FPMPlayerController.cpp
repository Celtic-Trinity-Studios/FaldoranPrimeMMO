// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerController.h"
#include "Account/FPMAccountSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Character/FPMCharacterCreationSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Player/FPMPlayerCharacter.h"
#include "UI/FPMCharacterCreationWidget.h"
#include "UI/FPMCharacterSelectWidget.h"
#include "UI/FPMLoginWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "World/FPMChunkActor.h"
#include "World/FPMChunkData.h"
#include "World/FPMVoxelChunk.h"
#include "World/FPMWorldChunkManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMPlayerController, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

AFPMPlayerController::AFPMPlayerController() {
  bShowMouseCursor = true;

  static ConstructorHelpers::FClassFinder<UFPMLoginWidget> LoginWidgetBP(
      TEXT("/Game/UI/WBP_LoginScreen"));
  if (LoginWidgetBP.Succeeded()) {
    LoginWidgetClass = LoginWidgetBP.Class;
  }

  static ConstructorHelpers::FClassFinder<UFPMCharacterCreationWidget>
      CharCreateBP(TEXT("/Game/UI/WBP_CharacterCreation"));
  if (CharCreateBP.Succeeded()) {
    CharacterCreationWidgetClass = CharCreateBP.Class;
  }

  static ConstructorHelpers::FClassFinder<UFPMCharacterSelectWidget>
      CharSelectBP(TEXT("/Game/UI/WBP_CharacterSelect"));
  if (CharSelectBP.Succeeded()) {
    CharacterSelectWidgetClass = CharSelectBP.Class;
  }
}

void AFPMPlayerController::BeginPlay() {
  Super::BeginPlay();

  if (HasAuthority()) {
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM: Player connected"));
  }

  if (IsLocalController()) {
    ShowLoginWidget();
  }
}

void AFPMPlayerController::OnPossess(APawn *InPawn) {
  Super::OnPossess(InPawn);
  UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Server: Possessed pawn %s"),
         *GetNameSafe(InPawn));
}

void AFPMPlayerController::ShowLoginWidget() {
  if (LoginWidget || !LoginWidgetClass)
    return;
  LoginWidget = CreateWidget<UFPMLoginWidget>(this, LoginWidgetClass);
  if (LoginWidget) {
    LoginWidget->AddToViewport(100);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(LoginWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
  }
}

void AFPMPlayerController::HideLoginWidget() {
  if (LoginWidget) {
    LoginWidget->RemoveFromParent();
    LoginWidget = nullptr;
  }
}

void AFPMPlayerController::ShowCharacterCreationWidget() {
  if (CharacterCreationWidget || !CharacterCreationWidgetClass)
    return;
  CharacterCreationWidget = CreateWidget<UFPMCharacterCreationWidget>(
      this, CharacterCreationWidgetClass);
  if (CharacterCreationWidget) {
    CharacterCreationWidget->AddToViewport(100);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(CharacterCreationWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
  }
}

void AFPMPlayerController::HideCharacterCreationWidget() {
  if (CharacterCreationWidget) {
    CharacterCreationWidget->RemoveFromParent();
    CharacterCreationWidget = nullptr;
  }
}

void AFPMPlayerController::ShowCharacterSelectWidget() {
  if (CharacterSelectWidget || !CharacterSelectWidgetClass)
    return;
  CharacterSelectWidget =
      CreateWidget<UFPMCharacterSelectWidget>(this, CharacterSelectWidgetClass);
  if (CharacterSelectWidget) {
    CharacterSelectWidget->AddToViewport(100);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(CharacterSelectWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
  }
}

void AFPMPlayerController::HideCharacterSelectWidget() {
  if (CharacterSelectWidget) {
    CharacterSelectWidget->RemoveFromParent();
    CharacterSelectWidget = nullptr;
  }
}

void AFPMPlayerController::HideAllUIAndEnterGame() {
  HideLoginWidget();
  HideCharacterCreationWidget();
  HideCharacterSelectWidget();
  SetInputMode(FInputModeGameOnly());
  bShowMouseCursor = false;
}

// -------------------------------------------------------------------
// Client-Side Helpers (called by widgets)
// -------------------------------------------------------------------

void AFPMPlayerController::RequestLogin(const FString &Username,
                                        const FString &Password) {
  FFPMLoginRequest Req;
  Req.Username = Username;
  Req.Password = Password;
  ServerRequestLogin(Req);
}

void AFPMPlayerController::RequestCreateAccount(const FString &Username,
                                                const FString &Password) {
  FFPMLoginRequest Req;
  Req.Username = Username;
  Req.Password = Password;
  ServerRequestCreateAccount(Req);
}

void AFPMPlayerController::RequestCreateCharacter(
    const FFPMCharacterCreationRequest &Request) {
  ServerRequestCreateCharacter(Request);
}

void AFPMPlayerController::RequestCharacterList() {
  ServerRequestCharacterList();
}

void AFPMPlayerController::RequestEnterWorld(const FGuid &CharacterId) {
  ServerRequestEnterWorld(CharacterId);
}

void AFPMPlayerController::ReturnToLogin() {
  HideCharacterCreationWidget();
  HideCharacterSelectWidget();
  ShowLoginWidget();
}

void AFPMPlayerController::TransitionToCharacterCreation() {
  HideCharacterSelectWidget();
  ShowCharacterCreationWidget();
}

void AFPMPlayerController::TransitionToCharacterSelect() {
  HideCharacterCreationWidget();
  RequestCharacterList();
  ShowCharacterSelectWidget();
}

// -------------------------------------------------------------------
// Server RPCs
// -------------------------------------------------------------------

void AFPMPlayerController::ServerRequestLogin_Implementation(
    FFPMLoginRequest Request) {
  // --- Lockout check ---
  if (bIsLockedOut) {
    FFPMLoginResult F;
    F.ErrorMessage = TEXT("Account locked. Too many failed attempts.");
    ClientReceiveLoginResult(F);
    return;
  }

  // --- Rate limit check ---
  if (IsRateLimited(LoginAttemptTimestamps, MaxLoginAttemptsPerWindow,
                    RateLimitWindowSeconds)) {
    FFPMLoginResult F;
    F.ErrorMessage = TEXT("Too many login attempts. Please wait.");
    ClientReceiveLoginResult(F);
    return;
  }

  // --- Input sanitization: clamp string lengths at RPC boundary ---
  Request.Username = ClampString(Request.Username, MaxRPCStringLength);
  Request.Password = ClampString(Request.Password, MaxRPCStringLength);

  UGameInstance *GI = GetGameInstance();
  UFPMAccountSubsystem *Acc =
      GI ? GI->GetSubsystem<UFPMAccountSubsystem>() : nullptr;
  if (!Acc) {
    FFPMLoginResult F;
    F.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveLoginResult(F);
    return;
  }

  FFPMLoginResult Result = Acc->Login(Request.Username, Request.Password);
  if (Result.bSuccess) {
    AuthenticatedAccountId = Result.AccountId;
    bIsAuthenticated = true;
    FailedLoginAttempts = 0; // Reset on success
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Server: Authenticated — %s"),
           *AuthenticatedAccountId.ToString());
  } else {
    ++FailedLoginAttempts;
    if (FailedLoginAttempts >= MaxLoginAttempts) {
      bIsLockedOut = true;
      UE_LOG(LogFPMPlayerController, Warning,
             TEXT("FPM Server: Connection locked out after %d failed login "
                  "attempts."),
             FailedLoginAttempts);
    }
  }
  ClientReceiveLoginResult(Result);
}

void AFPMPlayerController::ServerRequestCreateAccount_Implementation(
    FFPMLoginRequest Request) {
  // --- Rate limit check ---
  if (IsRateLimited(CreateAccountTimestamps, MaxCreateAccountPerWindow,
                    RateLimitWindowSeconds)) {
    FFPMLoginResult F;
    F.ErrorMessage = TEXT("Too many account creation attempts. Please wait.");
    ClientReceiveCreateAccountResult(F);
    return;
  }

  // --- Input sanitization ---
  Request.Username = ClampString(Request.Username, MaxRPCStringLength);
  Request.Password = ClampString(Request.Password, MaxRPCStringLength);

  UGameInstance *GI = GetGameInstance();
  UFPMAccountSubsystem *Acc =
      GI ? GI->GetSubsystem<UFPMAccountSubsystem>() : nullptr;
  if (!Acc) {
    FFPMLoginResult F;
    F.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveCreateAccountResult(F);
    return;
  }
  ClientReceiveCreateAccountResult(
      Acc->CreateAccount(Request.Username, Request.Password));
}

void AFPMPlayerController::ServerRequestCreateCharacter_Implementation(
    FFPMCharacterCreationRequest Request) {
  if (!bIsAuthenticated) {
    FFPMCharacterCreationResult F;
    F.ErrorCode = EFPMCharacterCreationError::ServerError;
    F.ErrorMessage = TEXT("Not authenticated.");
    ClientReceiveCreateCharacterResult(F);
    return;
  }

  UGameInstance *GI = GetGameInstance();
  UFPMCharacterCreationSubsystem *CSS =
      GI ? GI->GetSubsystem<UFPMCharacterCreationSubsystem>() : nullptr;
  if (!CSS) {
    FFPMCharacterCreationResult F;
    F.ErrorCode = EFPMCharacterCreationError::ServerError;
    F.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveCreateCharacterResult(F);
    return;
  }

  ClientReceiveCreateCharacterResult(
      CSS->SubmitCharacterCreation(AuthenticatedAccountId, Request));
}

void AFPMPlayerController::ServerRequestCharacterList_Implementation() {
  if (!bIsAuthenticated) {
    ClientReceiveCharacterList({});
    return;
  }

  UGameInstance *GI = GetGameInstance();
  UFPMDatabaseSubsystem *DB =
      GI ? GI->GetSubsystem<UFPMDatabaseSubsystem>() : nullptr;
  if (!DB || !DB->IsConnected()) {
    ClientReceiveCharacterList({});
    return;
  }

  const FString SQL =
      TEXT("SELECT character_id, character_name, body_type, last_played "
           "FROM characters WHERE account_id = $1 ORDER BY last_played DESC");
  const FString AccId =
      AuthenticatedAccountId.ToString(EGuidFormats::DigitsWithHyphensLower);

  FFPMDatabaseQueryResult DBR = DB->ExecuteQuery(SQL, {AccId});
  TArray<FFPMCharacterSummary> List;

  if (DBR.bSuccess) {
    for (const auto &Row : DBR.Rows) {
      FFPMCharacterSummary S;
      if (const FString *V = Row.Find(TEXT("character_id")))
        FGuid::Parse(*V, S.CharacterId);
      if (const FString *V = Row.Find(TEXT("character_name")))
        S.CharacterName = *V;
      if (const FString *V = Row.Find(TEXT("body_type")))
        S.BodyType = static_cast<uint8>(FCString::Atoi(**V));
      if (const FString *V = Row.Find(TEXT("last_played")))
        S.LastPlayed = *V;
      List.Add(S);
    }
  }

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Server: Sending %d characters."), List.Num());
  ClientReceiveCharacterList(List);
}

void AFPMPlayerController::ServerRequestEnterWorld_Implementation(
    FGuid CharacterId) {
  if (!bIsAuthenticated) {
    ClientEnterWorldFailed(TEXT("Not authenticated."));
    return;
  }

  UGameInstance *GI = GetGameInstance();
  UFPMDatabaseSubsystem *DB =
      GI ? GI->GetSubsystem<UFPMDatabaseSubsystem>() : nullptr;
  if (!DB || !DB->IsConnected()) {
    ClientEnterWorldFailed(TEXT("Internal server error."));
    return;
  }

  // Validate character ownership
  const FString SQL =
      TEXT("SELECT character_name, body_type, "
           "skin_color_r, skin_color_g, skin_color_b, "
           "hair_color_r, hair_color_g, hair_color_b "
           "FROM characters WHERE character_id = $1 AND account_id = $2");
  const FString CId =
      CharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);
  const FString AId =
      AuthenticatedAccountId.ToString(EGuidFormats::DigitsWithHyphensLower);

  FFPMDatabaseQueryResult DBR = DB->ExecuteQuery(SQL, {CId, AId});
  if (!DBR.bSuccess || DBR.Rows.Num() == 0) {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Server: Character ownership failed — char=%s, acc=%s"),
           *CId, *AId);
    ClientEnterWorldFailed(TEXT("Character not found or not owned."));
    return;
  }

  // Parse appearance
  const auto &Row = DBR.Rows[0];
  FString Name;
  uint8 BT = 0;
  FLinearColor Skin(0.8f, 0.6f, 0.5f, 1.0f);
  FLinearColor Hair(0.2f, 0.15f, 0.1f, 1.0f);

  if (const FString *V = Row.Find(TEXT("character_name")))
    Name = *V;
  if (const FString *V = Row.Find(TEXT("body_type")))
    BT = static_cast<uint8>(FCString::Atoi(**V));
  if (const FString *V = Row.Find(TEXT("skin_color_r")))
    Skin.R = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("skin_color_g")))
    Skin.G = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("skin_color_b")))
    Skin.B = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_r")))
    Hair.R = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_g")))
    Hair.G = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_b")))
    Hair.B = FCString::Atof(**V);

  // Spawn character on server
  UWorld *World = GetWorld();
  if (!World) {
    ClientEnterWorldFailed(TEXT("Internal server error."));
    return;
  }

  // --- Spawn near island center, searching for a suitable biome ---
  // Start at (0,0) and spiral outward to find a Forest or Meadows
  // location at a reasonable elevation.  Avoids Snow/Mountain peaks.
  FVector SpawnLoc(0.0f, 0.0f, 500.0f); // Overwritten below
  FRotator SpawnRot = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

  {
    int32 ActualWorldSeed = 42;
    if (AFPMWorldChunkManager *WCM =
            AFPMWorldChunkManager::GetOrCreate(World)) {
      ActualWorldSeed = WCM->WorldSeed;
    }

    bool bFoundSpawn = false;
    const float SearchStep = 1600.0f;   // ~1 hex chunk width
    constexpr int32 MaxSearchDist = 80; // covers most of the island

    for (int32 Ring = 0; Ring <= MaxSearchDist && !bFoundSpawn; ++Ring) {
      const int32 Samples = FMath::Max(1, Ring * 6);
      for (int32 S = 0; S < Samples && !bFoundSpawn; ++S) {
        const float Angle = (static_cast<float>(S) / Samples) * 2.0f * PI;
        const float TestX = Ring * SearchStep * FMath::Cos(Angle);
        const float TestY = Ring * SearchStep * FMath::Sin(Angle);

        const float SurfaceZ =
            FPMVoxelGenerator::TerrainSurfaceZ(TestX, TestY, ActualWorldSeed);

        if (SurfaceZ < 0.0f || SurfaceZ > 4000.0f) {
          continue;
        }

        const float NormH = (SurfaceZ - (-400.0f)) / 5000.0f;
        const EFPMBiome Biome = FPMVoxelGenerator::BiomeAtWorldXY(
            TestX, TestY, ActualWorldSeed, NormH);

        if (Biome == EFPMBiome::Forest || Biome == EFPMBiome::Meadows) {
          SpawnLoc = FVector(TestX, TestY, SurfaceZ);
          bFoundSpawn = true;
          UE_LOG(
              LogFPMPlayerController, Log,
              TEXT("FPM: Spawn found at (%.0f, %.0f, %.0f) Biome=%d Ring=%d"),
              TestX, TestY, SurfaceZ, static_cast<int32>(Biome), Ring);
        }
      }
    }

    if (!bFoundSpawn) {
      const float TerrainZ =
          FPMVoxelGenerator::TerrainSurfaceZ(0.0f, 0.0f, ActualWorldSeed);
      SpawnLoc = FVector(0.0f, 0.0f, TerrainZ);
      UE_LOG(
          LogFPMPlayerController, Warning,
          TEXT("FPM: No suitable spawn biome found, using origin (0,0,%.0f)"),
          SpawnLoc.Z);
    }
  }

  // Force-load chunks at spawn position BEFORE spawning the player
  // This prevents falling through the world while chunks async-load
  if (AFPMWorldChunkManager *WCM = AFPMWorldChunkManager::GetOrCreate(World)) {
    WCM->EnsureChunkLoadedAtWorldPos(SpawnLoc);
  }

  // Compute a safe vertical offset based on the character capsule (from the
  // CDO).
  float CapsuleHalfHeight = 88.0f; // fallback
  if (const AFPMPlayerCharacter *CDO =
          GetDefault<AFPMPlayerCharacter>(AFPMPlayerCharacter::StaticClass())) {
    if (const UCapsuleComponent *Capsule = CDO->GetCapsuleComponent()) {
      CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    }
  }

  // Spawn just above the terrain. The gravity-disable system handles
  // the gap while collision cooks.
  SpawnLoc.Z += CapsuleHalfHeight + 50.0f;

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM: Spawn Z adjusted to %.1f (capsuleHalf=%.1f, buffer=50)"),
         SpawnLoc.Z, CapsuleHalfHeight);

  FActorSpawnParameters Params;
  Params.Owner = this;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  AFPMPlayerCharacter *Char = World->SpawnActor<AFPMPlayerCharacter>(
      AFPMPlayerCharacter::StaticClass(), SpawnLoc, SpawnRot, Params);
  if (!Char) {
    ClientEnterWorldFailed(TEXT("Failed to spawn character."));
    return;
  }

  // Disable gravity and set Flying mode temporarily.
  // Terrain collision cooking is async — the mesh is created but collision
  // isn't ready for ~0.5s. Without this, the character falls through terrain.
  if (UCharacterMovementComponent *MoveComp = Char->GetCharacterMovement()) {
    MoveComp->SetMovementMode(MOVE_Flying);
    MoveComp->GravityScale = 0.0f;
    MoveComp->Velocity = FVector::ZeroVector;
  }

  Char->InitializeAppearance(Name, BT, Skin, Hair);
  Possess(Char);
  ActiveCharacterId = CharacterId;

  // Re-enable gravity only when terrain collision is ACTUALLY ready.
  // Poll every 0.25s with a downward line trace. If it hits the terrain,
  // collision cooking is done and it's safe to enable Walking mode.
  TWeakObjectPtr<AFPMPlayerCharacter> WeakChar = Char;
  TWeakObjectPtr<UWorld> WeakWorld = World;
  TSharedPtr<int32> PollCount = MakeShared<int32>(0);
  TSharedPtr<FTimerHandle> GravityTimerPtr = MakeShared<FTimerHandle>();
  World->GetTimerManager().SetTimer(
      *GravityTimerPtr,
      [WeakChar, WeakWorld, PollCount, GravityTimerPtr]() {
        if (!WeakWorld.IsValid()) {
          return;
        }
        AFPMPlayerCharacter *C = WeakChar.Get();
        if (!C) {
          // Character gone, clear timer safely
          WeakWorld->GetTimerManager().ClearTimer(*GravityTimerPtr);
          return;
        }

        ++(*PollCount);

        // Line trace downward from character to check terrain collision
        FHitResult Hit;
        const FVector Start = C->GetActorLocation();
        const FVector End = Start - FVector(0.0f, 0.0f, 2000.0f);
        const bool bHit = WeakWorld->LineTraceSingleByChannel(Hit, Start, End,
                                                              ECC_WorldStatic);

        if (bHit || *PollCount >= 20) { // 20 polls * 0.25s = 5s max wait
          if (UCharacterMovementComponent *MC = C->GetCharacterMovement()) {
            MC->GravityScale = 1.0f;
            MC->SetMovementMode(MOVE_Walking);
            UE_LOG(LogFPMPlayerController, Log,
                   TEXT("FPM: Gravity re-enabled after %d polls "
                        "(collision %s)"),
                   *PollCount, bHit ? TEXT("confirmed") : TEXT("timeout"));
          }
          // Stop this specific timer
          WeakWorld->GetTimerManager().ClearTimer(*GravityTimerPtr);
        }
      },
      0.25f, // Poll every 250ms
      true); // Looping

  // Update last_played
  DB->ExecuteQuery(TEXT("UPDATE characters SET last_played = NOW() "
                        "WHERE character_id = $1"),
                   {CId});

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Server: Spawned and possessed '%s'"), *Name);
  ClientEnterWorldSuccess(SpawnLoc);
}

// -------------------------------------------------------------------
// Client RPCs
// -------------------------------------------------------------------

void AFPMPlayerController::ClientReceiveLoginResult_Implementation(
    FFPMLoginResult Result) {
  if (Result.bSuccess) {
    HideLoginWidget();
    RequestCharacterList();
    ShowCharacterSelectWidget();
  } else {
    if (LoginWidget)
      LoginWidget->SetResultMessage(Result.ErrorMessage, true);
  }
}

void AFPMPlayerController::ClientReceiveCreateAccountResult_Implementation(
    FFPMLoginResult Result) {
  if (LoginWidget) {
    LoginWidget->SetResultMessage(
        Result.bSuccess ? TEXT("Account created! You can now log in.")
                        : Result.ErrorMessage,
        !Result.bSuccess);
  }
}

void AFPMPlayerController::ClientReceiveCreateCharacterResult_Implementation(
    FFPMCharacterCreationResult Result) {
  if (Result.bSuccess) {
    if (CharacterCreationWidget)
      CharacterCreationWidget->SetResultMessage(TEXT("Character created!"),
                                                false);
    TransitionToCharacterSelect();
  } else {
    if (CharacterCreationWidget)
      CharacterCreationWidget->SetResultMessage(Result.ErrorMessage, true);
  }
}

void AFPMPlayerController::ClientReceiveCharacterList_Implementation(
    const TArray<FFPMCharacterSummary> &Characters) {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Client: Received %d characters."), Characters.Num());

  if (Characters.Num() == 0) {
    HideCharacterSelectWidget();
    ShowCharacterCreationWidget();
    return;
  }
  if (CharacterSelectWidget)
    CharacterSelectWidget->PopulateCharacterList(Characters);
}

void AFPMPlayerController::ClientEnterWorldSuccess_Implementation(
    const FVector &InSpawnLocation) {
  // CRITICAL: Force-load the chunk at the spawn location on the CLIENT.
  UWorld *World = GetWorld();
  if (World) {
    if (AFPMWorldChunkManager *WCM =
            AFPMWorldChunkManager::GetOrCreate(World)) {
      UE_LOG(LogFPMPlayerController, Log,
             TEXT("FPM Client: Force-loading spawn chunk at %.0f, %.0f, %.0f"),
             InSpawnLocation.X, InSpawnLocation.Y, InSpawnLocation.Z);
      WCM->ForceChunkUpdate();
      WCM->EnsureChunkLoadedAtWorldPos(InSpawnLocation);
    }
  }

  if (APawn *MyPawn = GetPawn()) {
    MyPawn->SetActorLocation(InSpawnLocation);
    UE_LOG(LogFPMPlayerController, Log,
           TEXT("FPM Client: Teleported pawn to (%.0f, %.0f, %.0f)"),
           InSpawnLocation.X, InSpawnLocation.Y, InSpawnLocation.Z);

    // CRITICAL: Disable gravity on the CLIENT too.
    // The server already did this, but the replicated movement mode may not
    // arrive in time. Without this the client falls through collision-cooking
    // terrain.
    if (ACharacter *Char = Cast<ACharacter>(MyPawn)) {
      if (UCharacterMovementComponent *MoveComp =
              Char->GetCharacterMovement()) {
        MoveComp->SetMovementMode(MOVE_Flying);
        MoveComp->GravityScale = 0.0f;
        MoveComp->Velocity = FVector::ZeroVector;
        UE_LOG(LogFPMPlayerController, Log,
               TEXT("FPM Client: Gravity disabled, Flying mode set"));
      }

      // Poll for terrain collision readiness, then re-enable gravity.
      TWeakObjectPtr<ACharacter> WeakChar = Char;
      TWeakObjectPtr<UWorld> WeakWorld = World;
      FTimerHandle GravityPollHandle;

      World->GetTimerManager().SetTimer(
          GravityPollHandle,
          [WeakChar, WeakWorld]() {
            if (!WeakChar.IsValid() || !WeakWorld.IsValid())
              return;
            ACharacter *C = WeakChar.Get();
            UCharacterMovementComponent *MC = C->GetCharacterMovement();
            if (!MC || MC->MovementMode != MOVE_Flying)
              return;

            FHitResult Hit;
            const FVector Start = C->GetActorLocation();
            const FVector End = Start - FVector(0, 0, 500.0f);
            FCollisionQueryParams QParams;
            QParams.AddIgnoredActor(C);

            if (WeakWorld->LineTraceSingleByChannel(Hit, Start, End,
                                                    ECC_WorldStatic, QParams)) {
              MC->SetMovementMode(MOVE_Walking);
              MC->GravityScale = 1.0f;
              UE_LOG(LogTemp, Log,
                     TEXT("FPM Client: Terrain collision ready, gravity on"));
              WeakWorld->GetTimerManager().ClearAllTimersForObject(C);
            }
          },
          0.25f, true);
    }
  }

  HideAllUIAndEnterGame();
}
void AFPMPlayerController::ClientEnterWorldFailed_Implementation(
    const FString &ErrorMessage) {
  UE_LOG(LogFPMPlayerController, Warning,
         TEXT("FPM Client: Enter world failed — %s"), *ErrorMessage);
  if (CharacterSelectWidget)
    CharacterSelectWidget->SetStatusMessage(ErrorMessage, true);
}

// -------------------------------------------------------------------
// Utility Methods
// -------------------------------------------------------------------

bool AFPMPlayerController::IsRateLimited(TArray<double> &Timestamps,
                                         int32 MaxPerWindow,
                                         double WindowSeconds) {
  const double Now = FPlatformTime::Seconds();
  const double Cutoff = Now - WindowSeconds;

  // Prune expired entries
  Timestamps.RemoveAll(
      [Cutoff](double Timestamp) { return Timestamp < Cutoff; });

  if (Timestamps.Num() >= MaxPerWindow) {
    return true; // Rate limited
  }

  Timestamps.Add(Now);
  return false;
}

FString AFPMPlayerController::ClampString(const FString &Input, int32 MaxLen) {
  if (Input.Len() <= MaxLen) {
    return Input;
  }
  return Input.Left(MaxLen);
}
