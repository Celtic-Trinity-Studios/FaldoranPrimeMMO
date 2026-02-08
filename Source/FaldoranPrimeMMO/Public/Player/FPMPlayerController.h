// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Account/FPMAccountTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"


#include "FPMPlayerController.generated.h"

class UFPMLoginWidget;

/**
 * AFPMPlayerController
 *
 * Base PlayerController for Faldoran Prime. Handles:
 *   - Client UI management (login screen display)
 *   - Server RPCs for account creation and login
 *   - Authenticated account tracking (server-side only)
 *
 * The login flow:
 *   1. Client BeginPlay -> show login widget
 *   2. Player fills in credentials -> widget calls
 * RequestLogin/RequestCreateAccount
 *   3. Client sends Server RPC (ServerRequestLogin /
 * ServerRequestCreateAccount)
 *   4. Server processes via UFPMAccountSubsystem
 *   5. Server sends result back via Client RPC
 *   6. Client updates widget with result
 *
 * PROTOTYPE NOTE: Credentials are sent via RPC (unencrypted UE channel).
 * Production MUST use TLS/DTLS. See 00_Rules_and_Constraints.md.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMPlayerController : public APlayerController {
  GENERATED_BODY()

public:
  AFPMPlayerController();

  // --- Client-side helpers (called by UFPMLoginWidget) ---

  /** Package credentials and send login request to server via RPC. */
  void RequestLogin(const FString &Username, const FString &Password);

  /** Package credentials and send account creation request to server via RPC.
   */
  void RequestCreateAccount(const FString &Username, const FString &Password);

protected:
  virtual void BeginPlay() override;

  // --- Server RPCs (executed on the dedicated server) ---

  /** Client to Server: Request login authentication. */
  UFUNCTION(Server, Reliable)
  void ServerRequestLogin(FFPMLoginRequest Request);

  /** Client to Server: Request account creation. */
  UFUNCTION(Server, Reliable)
  void ServerRequestCreateAccount(FFPMLoginRequest Request);

  // --- Client RPCs (executed on the owning client) ---

  /** Server to Client: Deliver login result. */
  UFUNCTION(Client, Reliable)
  void ClientReceiveLoginResult(FFPMLoginResult Result);

  /** Server to Client: Deliver account creation result. */
  UFUNCTION(Client, Reliable)
  void ClientReceiveCreateAccountResult(FFPMLoginResult Result);

private:
  // --- Login Widget ---

  /**
   * The Widget Blueprint class to instantiate for the login screen.
   * Set in Blueprint defaults. Expects WBP_LoginScreen.
   */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|UI")
  TSubclassOf<UFPMLoginWidget> LoginWidgetClass;

  /** The active login widget instance. Null after login succeeds. */
  UPROPERTY()
  TObjectPtr<UFPMLoginWidget> LoginWidget;

  /** Show the login widget on the client viewport. */
  void ShowLoginWidget();

  /** Hide and destroy the login widget. */
  void HideLoginWidget();

  // --- Authenticated State (Server-Only) ---

  /**
   * The authenticated account UUID for this connection.
   * Set on the server after successful login. NOT replicated.
   * Used to authorize all subsequent requests (character creation, etc.).
   */
  FGuid AuthenticatedAccountId;

  /** Whether this connection has been authenticated. Server-side only. */
  bool bIsAuthenticated = false;
};
