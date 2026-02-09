// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerController.h"
#include "Account/FPMAccountSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Character/FPMCharacterCreationSubsystem.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Player/FPMPlayerCharacter.h"
#include "UI/FPMCharacterCreationWidget.h"
#include "UI/FPMCharacterSelectWidget.h"
#include "UI/FPMLoginWidget.h"
#include "UObject/ConstructorHelpers.h"

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

// -------------------------------------------------------------------
// Widget Management
// -------------------------------------------------------------------

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
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Server: Authenticated — %s"),
           *AuthenticatedAccountId.ToString());
  }
  ClientReceiveLoginResult(Result);
}

void AFPMPlayerController::ServerRequestCreateAccount_Implementation(
    FFPMLoginRequest Request) {
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

  FVector SpawnLoc(0.0f, 0.0f, 200.0f);
  FRotator SpawnRot = FRotator::ZeroRotator;
  if (AGameModeBase *GM = World->GetAuthGameMode()) {
    if (AActor *Start = GM->FindPlayerStart(this)) {
      SpawnLoc = Start->GetActorLocation();
      SpawnRot = Start->GetActorRotation();
    }
  }

  FActorSpawnParameters Params;
  Params.Owner = this;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

  AFPMPlayerCharacter *Char = World->SpawnActor<AFPMPlayerCharacter>(
      AFPMPlayerCharacter::StaticClass(), SpawnLoc, SpawnRot, Params);
  if (!Char) {
    ClientEnterWorldFailed(TEXT("Failed to spawn character."));
    return;
  }

  Char->InitializeAppearance(Name, BT, Skin, Hair);
  Possess(Char);
  ActiveCharacterId = CharacterId;

  // Update last_played
  DB->ExecuteQuery(TEXT("UPDATE characters SET last_played = NOW() "
                        "WHERE character_id = $1"),
                   {CId});

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Server: Spawned and possessed '%s'"), *Name);
  ClientEnterWorldSuccess();
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

void AFPMPlayerController::ClientEnterWorldSuccess_Implementation() {
  HideAllUIAndEnterGame();
}

void AFPMPlayerController::ClientEnterWorldFailed_Implementation(
    const FString &ErrorMessage) {
  UE_LOG(LogFPMPlayerController, Warning,
         TEXT("FPM Client: Enter world failed — %s"), *ErrorMessage);
  if (CharacterSelectWidget)
    CharacterSelectWidget->SetStatusMessage(ErrorMessage, true);
}
