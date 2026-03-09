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
#include "Gameplay/FPMInventoryComponent.h"
#include "Player/FPMPlayerCharacter.h"
#include "UI/FPMCharacterCreationWidget.h"
#include "UI/FPMCharacterSelectWidget.h"
#include "UI/FPMEscMenuWidget.h"
#include "UI/FPMHUD.h"
#include "UI/FPMLoginWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "World/FPMChunkActor.h"
#include "World/FPMChunkData.h"
#include "World/FPMNexusManager.h"
#include "World/FPMNoise.h"
#include "World/FPMPlanetTraversal.h"
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

  static ConstructorHelpers::FClassFinder<UFPMEscMenuWidget> EscMenuBP(
      TEXT("/Game/UI/WBP_EscMenu"));
  if (EscMenuBP.Succeeded()) {
    EscMenuWidgetClass = EscMenuBP.Class;
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

void AFPMPlayerController::SetupInputComponent() {
  Super::SetupInputComponent();
  if (InputComponent) {
    InputComponent->BindAction(TEXT("OpenEscMenu"), IE_Pressed, this,
                               &AFPMPlayerController::ToggleEscMenu);
    InputComponent->BindAction(TEXT("ToggleRiftRunner"), IE_Pressed, this,
                               &AFPMPlayerController::OnToggleRiftRunner);
    InputComponent->BindAction(TEXT("CycleRiftSpeed"), IE_Pressed, this,
                               &AFPMPlayerController::OnCycleRiftSpeed);
    InputComponent->BindAction(TEXT("ToggleInventory"), IE_Pressed, this,
                               &AFPMPlayerController::OnToggleInventory);
  }
}

void AFPMPlayerController::OnPossess(APawn *InPawn) {
  Super::OnPossess(InPawn);
  UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Server: Possessed pawn %s"),
         *GetNameSafe(InPawn));

  // Pass world seed to PlanetTraversal component for terrain altitude lookups.
  // The component lives on the character (created in its constructor) so it
  // exists on both client and server — we just need to supply the seed here.
  if (InPawn) {
    if (UFPMPlanetTraversal *Traversal =
            InPawn->FindComponentByClass<UFPMPlanetTraversal>()) {
      if (AFPMWorldChunkManager *WCM =
              AFPMWorldChunkManager::GetOrCreate(GetWorld())) {
        Traversal->SetWorldSeed(WCM->WorldSeed);
      }
    }
  }
}

void AFPMPlayerController::OnUnPossess() {
  // Save position NOW while the pawn is still guaranteed to be valid.
  // This fires: on explicit logout, on PIE stop, on disconnect, and when the
  // server demotes the controller for any reason.
  // GameMode::Logout() also saves as a belt-and-suspenders backup—
  // double-writing identical values to the DB is harmless.
  if (HasAuthority() && bIsAuthenticated && ActiveCharacterId.IsValid()) {
    if (APawn *P = GetPawn()) {
      const FVector Loc = P->GetActorLocation();
      UGameInstance *GI = GetGameInstance();
      UFPMDatabaseSubsystem *DB =
          GI ? GI->GetSubsystem<UFPMDatabaseSubsystem>() : nullptr;
      if (DB && DB->IsConnected()) {
        const FString CId =
            ActiveCharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);
        const FFPMDatabaseQueryResult R = DB->ExecuteQuery(
            TEXT("UPDATE characters "
                 "SET spawn_x = $1, spawn_y = $2, spawn_z = $3, "
                 "last_played = NOW() "
                 "WHERE character_id = $4"),
            {FString::SanitizeFloat(Loc.X), FString::SanitizeFloat(Loc.Y),
             FString::SanitizeFloat(Loc.Z), CId});
        if (R.bSuccess) {
          UE_LOG(LogFPMPlayerController, Log,
                 TEXT("FPM Server: OnUnPossess — saved position "
                      "(%.0f, %.0f, %.0f) for %s"),
                 Loc.X, Loc.Y, Loc.Z, *CId);
        } else {
          UE_LOG(LogFPMPlayerController, Warning,
                 TEXT("FPM Server: OnUnPossess — DB save failed for %s: %s"),
                 *CId, *R.ErrorMessage);
        }
      }
    }
  }
  Super::OnUnPossess();
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

// --- ESC Menu ---

void AFPMPlayerController::ShowEscMenu() {
  // Don't open ESC menu if we're not in-game (still on login/char create)
  if (!GetPawn())
    return;
  if (EscMenuWidget)
    return; // already open — toggle will close it
  if (!EscMenuWidgetClass)
    return;

  EscMenuWidget = CreateWidget<UFPMEscMenuWidget>(this, EscMenuWidgetClass);
  if (EscMenuWidget) {
    EscMenuWidget->AddToViewport(200); // high Z so it's always on top
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(EscMenuWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM: ESC menu opened."));
  }
}

void AFPMPlayerController::HideEscMenu() {
  if (EscMenuWidget) {
    EscMenuWidget->RemoveFromParent();
    EscMenuWidget = nullptr;
  }
  // Restore game-only input only if the player is in-world
  if (GetPawn()) {
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
  }
  UE_LOG(LogFPMPlayerController, Log, TEXT("FPM: ESC menu closed."));
}

void AFPMPlayerController::ToggleEscMenu() {
  // Only respond to ESC while actively playing (pawn possessed)
  if (!GetPawn())
    return;
  if (EscMenuWidget) {
    HideEscMenu();
  } else {
    ShowEscMenu();
  }
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

void AFPMPlayerController::RequestSaveAndLogout() {
  // Client-side: fire the server RPC that will persist position then disconnect
  ServerSaveAndLogout();
}

void AFPMPlayerController::ExecuteLogout() {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Client: Executing logout — returning to login."));

  HideEscMenu();

  // Travel back to the same server URL. This properly tears down the
  // current net connection and immediately reconnects, giving us a fresh
  // PlayerController on the server and showing the login screen on the
  // client. Works in both PIE (listen server) and packaged dedicated server.
  //
  // Using ClientTravel rather than ConsoleCommand("disconnect") because:
  //   - "disconnect" in PIE leaves the client with no server connection,
  //     making the subsequent login RPC unreachable.
  //   - ClientTravel reconnects to the same address, so the login UI
  //     can immediately send RPCs without restarting PIE.
  const FString ServerURL = GetWorld()->GetAddressURL();
  UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Client: Travelling back to %s"),
         *ServerURL);
  ClientTravel(ServerURL, TRAVEL_Relative);
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

  const FString SQL = TEXT(
      "SELECT character_id, character_name, body_type, species, last_played "
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
      if (const FString *V = Row.Find(TEXT("species")))
        S.Species = static_cast<EFPMSpecies>(FCString::Atoi(**V));
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

  // Validate character ownership (also fetch saved world position)
  const FString SQL =
      TEXT("SELECT character_name, species, body_type, "
           "skin_color_r, skin_color_g, skin_color_b, "
           "eye_color_r, eye_color_g, eye_color_b, "
           "hair_color_r, hair_color_g, hair_color_b, "
           "morph_jaw, morph_nose, morph_brow, morph_lips, "
           "spawn_x, spawn_y, spawn_z "
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
  uint8 Species = 0;
  uint8 BT = 0;
  FLinearColor Skin(0.8f, 0.6f, 0.5f, 1.0f);
  FLinearColor Eye(0.3f, 0.5f, 0.8f, 1.0f);
  FLinearColor Hair(0.2f, 0.15f, 0.1f, 1.0f);
  float MorphJaw = 0.5f, MorphNose = 0.5f;
  float MorphBrow = 0.5f, MorphLips = 0.5f;

  if (const FString *V = Row.Find(TEXT("character_name")))
    Name = *V;
  if (const FString *V = Row.Find(TEXT("species")))
    Species = static_cast<uint8>(FCString::Atoi(**V));
  if (const FString *V = Row.Find(TEXT("body_type")))
    BT = static_cast<uint8>(FCString::Atoi(**V));
  if (const FString *V = Row.Find(TEXT("skin_color_r")))
    Skin.R = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("skin_color_g")))
    Skin.G = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("skin_color_b")))
    Skin.B = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("eye_color_r")))
    Eye.R = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("eye_color_g")))
    Eye.G = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("eye_color_b")))
    Eye.B = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_r")))
    Hair.R = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_g")))
    Hair.G = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("hair_color_b")))
    Hair.B = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("morph_jaw")))
    MorphJaw = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("morph_nose")))
    MorphNose = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("morph_brow")))
    MorphBrow = FCString::Atof(**V);
  if (const FString *V = Row.Find(TEXT("morph_lips")))
    MorphLips = FCString::Atof(**V);

  // Fetch saved spawn position from DB (NULL on first login)
  float SavedX = 0.f, SavedY = 0.f, SavedZ = 0.f;
  bool bHasSavedPos = false;

  // NOTE: FPMDatabaseSubsystem stores PostgreSQL NULLs as the literal
  // string "NULL" (uppercase) — see FPMDatabaseSubsystem.cpp line ~286.
  auto IsNullOrEmpty = [](const FString &S) {
    return S.IsEmpty() || S.Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
  };

  const FString *VX = Row.Find(TEXT("spawn_x"));
  const FString *VY = Row.Find(TEXT("spawn_y"));
  const FString *VZ = Row.Find(TEXT("spawn_z"));

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM DB spawn pos: spawn_x='%s' spawn_y='%s' spawn_z='%s'"),
         VX ? **VX : TEXT("<missing>"), VY ? **VY : TEXT("<missing>"),
         VZ ? **VZ : TEXT("<missing>"));

  if (VX && VY && VZ && !IsNullOrEmpty(*VX) && !IsNullOrEmpty(*VY) &&
      !IsNullOrEmpty(*VZ)) {
    SavedX = FCString::Atof(**VX);
    SavedY = FCString::Atof(**VY);
    SavedZ = FCString::Atof(**VZ);
    // (0,0,0) exactly is almost certainly an unset value, not a real position
    if (!FMath::IsNearlyZero(SavedX) || !FMath::IsNearlyZero(SavedY)) {
      bHasSavedPos = true;
    } else {
      UE_LOG(LogFPMPlayerController, Log,
             TEXT("FPM: spawn_x/y are both ~0 — treating as unset."));
    }
  }

  // Spawn character on server
  UWorld *World = GetWorld();
  if (!World) {
    ClientEnterWorldFailed(TEXT("Internal server error."));
    return;
  }

  int32 ActualWorldSeed = 42;
  if (AFPMWorldChunkManager *WCM = AFPMWorldChunkManager::GetOrCreate(World)) {
    ActualWorldSeed = WCM->WorldSeed;
  }

  // Ensure NexusManager exists (singleton — safe to call every time)
  AFPMNexusManager *NexusMgr = AFPMNexusManager::GetOrCreate(World);

  // -----------------------------------------------------------------------
  //  SPAWN LOCATION RESOLUTION
  //
  //  Priority:
  //    1. Saved position (returning character) — validated: must be on land
  //    2. Nexus spawn point (new character, or invalid saved position)
  //    3. Absolute fallback: world origin (should never hit this)
  // -----------------------------------------------------------------------
  FVector SpawnLoc(0.0f, 0.0f, 500.0f);
  FRotator SpawnRot = FRotator(0.0f, 0.0f, 0.0f);

  if (bHasSavedPos) {
    // Validate: confirm saved position is on land using TerrainSurfaceZ
    // (the single source of truth that includes river/lake carving).
    // Sea level is Z=0. Any terrain surface > 50cm is land.
    const float SurfaceZ =
        FPMVoxelGenerator::TerrainSurfaceZ(SavedX, SavedY, ActualWorldSeed);
    const bool bIsLand = (SurfaceZ > 50.0f);
    if (bIsLand) {
      // Use the higher of saved Z and terrain surface Z:
      //  - Prevents spawning underground if terrain was regenerated
      //  - Respects saved Z when player was on a structure above terrain
      const float RestoreZ = FMath::Max(SavedZ, SurfaceZ);
      SpawnLoc = FVector(SavedX, SavedY, RestoreZ);
      UE_LOG(LogFPMPlayerController, Log,
             TEXT("FPM: Returning character '%s' — restoring to saved pos "
                  "(%.0f, %.0f, %.0f) [savedZ=%.0f, surfaceZ=%.0f]"),
             *Name, SavedX, SavedY, RestoreZ, SavedZ, SurfaceZ);
    } else {
      UE_LOG(LogFPMPlayerController, Warning,
             TEXT("FPM: Saved position (%.0f, %.0f) surfaceZ=%.0f is over "
                  "water/invalid — redirecting to Nexus."),
             SavedX, SavedY, SurfaceZ);
      bHasSavedPos = false; // fall through to Nexus
    }
  }

  if (!bHasSavedPos) {
    // Brand-new character or invalid saved pos → always start at the Nexus
    if (NexusMgr) {
      SpawnLoc = NexusMgr->GetNewCharacterSpawnPos(ActualWorldSeed);
      UE_LOG(LogFPMPlayerController, Log,
             TEXT("FPM: New/reset character '%s' — spawning at Nexus "
                  "(%.0f, %.0f, %.0f)"),
             *Name, SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);
    } else {
      // Last-resort fallback: world origin
      const float TerrainZ =
          FPMVoxelGenerator::TerrainSurfaceZ(0.f, 0.f, ActualWorldSeed);
      SpawnLoc = FVector(0.f, 0.f, TerrainZ);
      UE_LOG(LogFPMPlayerController, Warning,
             TEXT("FPM: NexusManager unavailable — falling back to origin "
                  "(0,0,%.0f)"),
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

  Char->InitializeAppearance(Name, Species, BT, Skin, Eye, Hair, MorphJaw,
                             MorphNose, MorphBrow, MorphLips);
  Possess(Char);
  ActiveCharacterId = CharacterId;

  // Load this character's inventory from the database.
  // Done immediately after Possess so the grid is populated before
  // ClientEnterWorldSuccess fires and the client can display the UI.
  if (UFPMInventoryComponent *Inv = Char->GetInventoryComponent()) {
    Inv->LoadFromDB(DB, CharacterId);
  }

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
        // Trace down 5km to ensure we find the ground in this new 20km world
        const FVector End = Start - FVector(0.0f, 0.0f, 500000.0f);
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
    // One character per account: request character list to check
    // if a character exists. If yes, auto-enter. If no, create one.
    RequestCharacterList();
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
    // One character per account: after creation, auto-enter world.
    // Request character list which will trigger auto-enter.
    HideCharacterCreationWidget();
    RequestCharacterList();
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
    // One character per account: no character yet, go to creation.
    HideCharacterSelectWidget();
    ShowCharacterCreationWidget();
    return;
  }

  // One character per account: auto-enter world with the single character.
  // No character select screen needed.
  HideCharacterSelectWidget();
  RequestEnterWorld(Characters[0].CharacterId);
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
      // CRITICAL: Must match server's trace distance (5km) and timeout
      // (20 polls = 5s). The client has independently async-cooking
      // collision, so we can NOT rely on the server's replicated movement
      // mode — we must wait for OUR OWN collision to be ready.
      TWeakObjectPtr<ACharacter> WeakChar = Char;
      TWeakObjectPtr<UWorld> WeakWorld = World;
      TSharedPtr<int32> ClientPollCount = MakeShared<int32>(0);
      TSharedPtr<FTimerHandle> ClientGravityTimerPtr =
          MakeShared<FTimerHandle>();

      World->GetTimerManager().SetTimer(
          *ClientGravityTimerPtr,
          [WeakChar, WeakWorld, ClientPollCount, ClientGravityTimerPtr]() {
            if (!WeakChar.IsValid() || !WeakWorld.IsValid()) {
              if (WeakWorld.IsValid())
                WeakWorld->GetTimerManager().ClearTimer(*ClientGravityTimerPtr);
              return;
            }
            ACharacter *C = WeakChar.Get();
            UCharacterMovementComponent *MC = C->GetCharacterMovement();
            if (!MC) {
              WeakWorld->GetTimerManager().ClearTimer(*ClientGravityTimerPtr);
              return;
            }

            ++(*ClientPollCount);

            // Line trace downward 5km (matching server) to find terrain
            FHitResult Hit;
            const FVector Start = C->GetActorLocation();
            const FVector End = Start - FVector(0.0f, 0.0f, 500000.0f);
            FCollisionQueryParams QParams;
            QParams.AddIgnoredActor(C);

            const bool bHit = WeakWorld->LineTraceSingleByChannel(
                Hit, Start, End, ECC_WorldStatic, QParams);

            if (bHit || *ClientPollCount >= 20) { // 20 * 0.25s = 5s max
              MC->SetMovementMode(MOVE_Walking);
              MC->GravityScale = 1.0f;
              UE_LOG(LogTemp, Log,
                     TEXT("FPM Client: Gravity re-enabled after %d polls "
                          "(collision %s)"),
                     *ClientPollCount,
                     bHit ? TEXT("confirmed") : TEXT("timeout"));
              WeakWorld->GetTimerManager().ClearTimer(*ClientGravityTimerPtr);
            } else {
              // Force Flying mode in case server replication overwrote it
              if (MC->MovementMode != MOVE_Flying) {
                MC->SetMovementMode(MOVE_Flying);
                MC->GravityScale = 0.0f;
                MC->Velocity = FVector::ZeroVector;
                UE_LOG(LogTemp, Warning,
                       TEXT("FPM Client: Re-forced Flying mode (poll %d, "
                            "server replicated %d)"),
                       *ClientPollCount, static_cast<int32>(MC->MovementMode));
              }
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
// Save & Logout RPCs
// -------------------------------------------------------------------

void AFPMPlayerController::ServerSaveAndLogout_Implementation() {
  if (!bIsAuthenticated || !ActiveCharacterId.IsValid()) {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Server: SaveAndLogout ignored — not authenticated or "
                "no active character."));
    // Still tell client to disconnect so it doesn't hang
    ClientSaveComplete(false);
    return;
  }

  // Grab current pawn position
  FVector PawnLoc = FVector::ZeroVector;
  if (APawn *P = GetPawn()) {
    PawnLoc = P->GetActorLocation();
  }

  const FString CId =
      ActiveCharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);

  UGameInstance *GI = GetGameInstance();
  UFPMDatabaseSubsystem *DB =
      GI ? GI->GetSubsystem<UFPMDatabaseSubsystem>() : nullptr;
  bool bSaved = false;

  if (DB && DB->IsConnected()) {
    // Write spawn position and last_played
    const FString XStr = FString::SanitizeFloat(PawnLoc.X);
    const FString YStr = FString::SanitizeFloat(PawnLoc.Y);
    const FString ZStr = FString::SanitizeFloat(PawnLoc.Z);

    const FFPMDatabaseQueryResult R =
        DB->ExecuteQuery(TEXT("UPDATE characters "
                              "SET spawn_x = $1, spawn_y = $2, spawn_z = $3, "
                              "last_played = NOW() "
                              "WHERE character_id = $4"),
                         {XStr, YStr, ZStr, CId});

    bSaved = R.bSuccess;
    if (bSaved) {
      UE_LOG(LogFPMPlayerController, Log,
             TEXT("FPM Server: Saved position (%.0f, %.0f, %.0f) for "
                  "character %s"),
             PawnLoc.X, PawnLoc.Y, PawnLoc.Z, *CId);
    } else {
      UE_LOG(LogFPMPlayerController, Warning,
             TEXT("FPM Server: DB save failed for character %s — %s"), *CId,
             *R.ErrorMessage);
    }
  } else {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Server: DB unavailable — position not saved for %s"),
           *CId);
  }

  // Notify client (regardless of success — allow logout to proceed)
  ClientSaveComplete(bSaved);
}

void AFPMPlayerController::ClientSaveComplete_Implementation(bool bSuccess) {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Client: Save complete — success=%d"), bSuccess ? 1 : 0);

  if (EscMenuWidget) {
    if (bSuccess) {
      EscMenuWidget->SetStatusMessage(
          TEXT("\u2713 Saved \u2014 disconnecting\u2026"), false);
    } else {
      EscMenuWidget->SetStatusMessage(
          TEXT("Could not save position. Disconnecting\u2026"), true);
    }
  }

  // Give the player a moment to read the message, then disconnect.
  // The ESC widget countdown (2.5s) is already running as a safety net.
  // This timer fires at 1.2s — whichever fires first wins (widget sets
  // bLogoutPending on first call, subsequent ones are ignored).
  UWorld *World = GetWorld();
  if (World) {
    TWeakObjectPtr<AFPMPlayerController> WeakSelf = this;
    World->GetTimerManager().SetTimerForNextTick([WeakSelf]() {
      if (AFPMPlayerController *Self = WeakSelf.Get()) {
        // Use a proper timer for the 1.2s delay
        FTimerHandle Handle;
        Self->GetWorld()->GetTimerManager().SetTimer(
            Handle,
            [WeakSelf]() {
              if (AFPMPlayerController *S = WeakSelf.Get()) {
                S->ExecuteLogout();
              }
            },
            1.2f, false);
      }
    });
  }
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

// -------------------------------------------------------------------
// Rift Runner Handlers
// -------------------------------------------------------------------

void AFPMPlayerController::OnToggleRiftRunner() {
  if (GetPawn()) {
    if (UFPMPlanetTraversal *T =
            GetPawn()->FindComponentByClass<UFPMPlanetTraversal>()) {
      T->ToggleRiftRunner();
    }
  }
}

void AFPMPlayerController::OnCycleRiftSpeed() {
  if (GetPawn()) {
    if (UFPMPlanetTraversal *T =
            GetPawn()->FindComponentByClass<UFPMPlanetTraversal>()) {
      T->CycleSpeedTier();
    }
  }
}

void AFPMPlayerController::OnToggleInventory() {
  if (AFPMPlayerCharacter *Char = Cast<AFPMPlayerCharacter>(GetPawn())) {
    Char->ToggleInventory();
  }
}

// -------------------------------------------------------------------
// World Map Handler
// -------------------------------------------------------------------

void AFPMPlayerController::OnOpenWorldMap() {
  // Only available in-game (pawn possessed)
  if (!GetPawn())
    return;

  // Get the HUD and toggle the map
  if (AFPMHUD *HUD = Cast<AFPMHUD>(GetHUD())) {
    // Pass world seed from WorldChunkManager
    int32 Seed = 42;
    if (AFPMWorldChunkManager *WCM =
            AFPMWorldChunkManager::GetOrCreate(GetWorld())) {
      Seed = WCM->WorldSeed;
    }
    HUD->ToggleWorldMap(Seed);
  }
}

// -------------------------------------------------------------------
// Debug Commands
// -------------------------------------------------------------------

void AFPMPlayerController::Debug_SpawnTestItems() {
  // Client-side: fire the Server RPC so it runs where authority lives.
  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
                                     TEXT("Spawning test items..."));
  }
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Debug: Debug_SpawnTestItems — sending Server RPC."));
  Server_Debug_SpawnTestItems();

  // Delay opening inventory to allow the Server RPC to process and
  // the items to replicate back to this client.
  // Without this delay, the widget opens with 0 items because
  // replication hasn't happened yet in the same frame.
  TWeakObjectPtr<AFPMPlayerController> WeakSelf = this;
  GetWorld()->GetTimerManager().SetTimer(
      DebugSpawnTimerHandle,
      [WeakSelf]() {
        if (AFPMPlayerController *Self = WeakSelf.Get()) {
          if (AFPMPlayerCharacter *Char =
                  Cast<AFPMPlayerCharacter>(Self->GetPawn())) {
            Char->ToggleInventory();
          }
        }
      },
      0.5f, false);
}

void AFPMPlayerController::Server_Debug_SpawnTestItems_Implementation() {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Debug: Server_Debug_SpawnTestItems — executing on server."));

  AFPMPlayerCharacter *Char = Cast<AFPMPlayerCharacter>(GetPawn());
  if (!Char) {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Debug: No pawn — spawn test items failed."));
    return;
  }

  UFPMInventoryComponent *Inv = Char->GetInventoryComponent();
  if (!Inv) {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Debug: No inventory component."));
    return;
  }

  // --- Spawn a variety of test items ---
  // Each call: AddItem(ItemID, Count, SizeX, SizeY)

  // 1×1 stackable items
  Inv->AddItem(FName("Item_HealthPotion"), 5, 1, 1);
  Inv->AddItem(FName("Item_ManaPotion"), 3, 1, 1);
  Inv->AddItem(FName("Item_Gold_Coin"), 50, 1, 1);

  // 1×2 items (tall)
  Inv->AddItem(FName("Item_Dagger"), 1, 1, 2);

  // 1×3 items (tall weapon)
  Inv->AddItem(FName("Item_Iron_Sword"), 1, 1, 3);

  // 2×2 items (square)
  Inv->AddItem(FName("Item_Dragon_Scale"), 1, 2, 2);

  // 2×1 items (wide)
  Inv->AddItem(FName("Item_Scroll"), 1, 2, 1);

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Debug: Spawned 7 test items into the backpack."));
}