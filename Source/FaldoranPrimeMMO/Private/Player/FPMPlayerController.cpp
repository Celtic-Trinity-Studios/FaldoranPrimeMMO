// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerController.h"
#include "Account/FPMAccountSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/FPMLoginWidget.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMPlayerController, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

AFPMPlayerController::AFPMPlayerController() {
  // Mouse cursor is shown during login UI phase
  bShowMouseCursor = true;

  // Auto-load the login widget Blueprint so it works without a Blueprint
  // PlayerController subclass. Path must match Content/UI/WBP_LoginScreen.
  static ConstructorHelpers::FClassFinder<UFPMLoginWidget> LoginWidgetBP(
      TEXT("/Game/UI/WBP_LoginScreen"));
  if (LoginWidgetBP.Succeeded()) {
    LoginWidgetClass = LoginWidgetBP.Class;
  }
}

void AFPMPlayerController::BeginPlay() {
  Super::BeginPlay();

  if (HasAuthority()) {
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM: Player connected"));
  }

  // Show login UI on any local client (works in PIE and packaged builds).
  // ShowLoginWidget() guards against null LoginWidgetClass and double-creates.
  if (IsLocalController()) {
    ShowLoginWidget();
  }
}

// -------------------------------------------------------------------
// Login Widget Management
// -------------------------------------------------------------------

void AFPMPlayerController::ShowLoginWidget() {
  if (LoginWidget) {
    // Already showing
    return;
  }

  if (!LoginWidgetClass) {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM: LoginWidgetClass is not set! Cannot show login screen. "
                "Set it in the Blueprint or DefaultGame.ini."));
    return;
  }

  LoginWidget = CreateWidget<UFPMLoginWidget>(this, LoginWidgetClass);
  if (LoginWidget) {
    LoginWidget->AddToViewport(100);

    // Set input mode to UI-only so the player can interact with the widget
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(LoginWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);

    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM: Login widget displayed."));
  }
}

void AFPMPlayerController::HideLoginWidget() {
  if (LoginWidget) {
    LoginWidget->RemoveFromParent();
    LoginWidget = nullptr;

    // Switch back to game input
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    bShowMouseCursor = false;

    UE_LOG(LogFPMPlayerController, Log,
           TEXT("FPM: Login widget hidden, transitioning to game."));
  }
}

// -------------------------------------------------------------------
// Client-Side Helpers (called by UFPMLoginWidget)
// -------------------------------------------------------------------

void AFPMPlayerController::RequestLogin(const FString &Username,
                                        const FString &Password) {
  FFPMLoginRequest Request;
  Request.Username = Username;
  Request.Password = Password;

  // This calls the Server RPC — UE handles replication automatically
  ServerRequestLogin(Request);

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM: Login request sent for user '%s'."), *Username);
}

void AFPMPlayerController::RequestCreateAccount(const FString &Username,
                                                const FString &Password) {
  FFPMLoginRequest Request;
  Request.Username = Username;
  Request.Password = Password;

  // This calls the Server RPC
  ServerRequestCreateAccount(Request);

  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM: Create account request sent for user '%s'."), *Username);
}

// -------------------------------------------------------------------
// Server RPCs (executed on the dedicated server)
// -------------------------------------------------------------------

void AFPMPlayerController::ServerRequestLogin_Implementation(
    FFPMLoginRequest Request) {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Server: Login request received for user '%s'."),
         *Request.Username);

  // Get the Account Subsystem from the GameInstance
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    FFPMLoginResult FailResult;
    FailResult.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveLoginResult(FailResult);
    return;
  }

  UFPMAccountSubsystem *AccountSys = GI->GetSubsystem<UFPMAccountSubsystem>();
  if (!AccountSys) {
    FFPMLoginResult FailResult;
    FailResult.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveLoginResult(FailResult);
    return;
  }

  // Delegate to the Account Subsystem — it handles validation, hashing, DB
  FFPMLoginResult Result =
      AccountSys->Login(Request.Username, Request.Password);

  if (Result.bSuccess) {
    // Store the authenticated account on this controller (server-side only)
    AuthenticatedAccountId = Result.AccountId;
    bIsAuthenticated = true;

    UE_LOG(LogFPMPlayerController, Log,
           TEXT("FPM Server: Player authenticated — AccountId=%s"),
           *AuthenticatedAccountId.ToString());
  }

  // Send result back to the owning client
  ClientReceiveLoginResult(Result);
}

void AFPMPlayerController::ServerRequestCreateAccount_Implementation(
    FFPMLoginRequest Request) {
  UE_LOG(LogFPMPlayerController, Log,
         TEXT("FPM Server: Create account request received for user '%s'."),
         *Request.Username);

  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    FFPMLoginResult FailResult;
    FailResult.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveCreateAccountResult(FailResult);
    return;
  }

  UFPMAccountSubsystem *AccountSys = GI->GetSubsystem<UFPMAccountSubsystem>();
  if (!AccountSys) {
    FFPMLoginResult FailResult;
    FailResult.ErrorMessage = TEXT("Internal server error.");
    ClientReceiveCreateAccountResult(FailResult);
    return;
  }

  FFPMLoginResult Result =
      AccountSys->CreateAccount(Request.Username, Request.Password);

  // Send result back to the owning client
  ClientReceiveCreateAccountResult(Result);
}

// -------------------------------------------------------------------
// Client RPCs (executed on the owning client)
// -------------------------------------------------------------------

void AFPMPlayerController::ClientReceiveLoginResult_Implementation(
    FFPMLoginResult Result) {
  if (Result.bSuccess) {
    UE_LOG(LogFPMPlayerController, Log, TEXT("FPM Client: Login successful!"));

    // Hide the login widget and transition to the next screen
    HideLoginWidget();

    // TODO Phase 6: Transition to character select screen.
    // For now, log a placeholder message.
    UE_LOG(LogFPMPlayerController, Log,
           TEXT("FPM Client: Would transition to character select (Phase 6)."));
  } else {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Client: Login failed — %s"), *Result.ErrorMessage);

    // Update the widget with the error message
    if (LoginWidget) {
      LoginWidget->SetResultMessage(Result.ErrorMessage, true);
    }
  }
}

void AFPMPlayerController::ClientReceiveCreateAccountResult_Implementation(
    FFPMLoginResult Result) {
  if (Result.bSuccess) {
    UE_LOG(LogFPMPlayerController, Log,
           TEXT("FPM Client: Account created successfully!"));

    if (LoginWidget) {
      LoginWidget->SetResultMessage(
          TEXT("Account created! You can now log in."), false);
    }
  } else {
    UE_LOG(LogFPMPlayerController, Warning,
           TEXT("FPM Client: Account creation failed — %s"),
           *Result.ErrorMessage);

    if (LoginWidget) {
      LoginWidget->SetResultMessage(Result.ErrorMessage, true);
    }
  }
}
