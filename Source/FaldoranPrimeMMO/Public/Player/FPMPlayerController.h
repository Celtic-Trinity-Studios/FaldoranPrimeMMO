// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Account/FPMAccountTypes.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "FPMPlayerController.generated.h"

class UFPMCharacterCreationWidget;
class UFPMCharacterSelectWidget;
class UFPMLoginWidget;

/**
 * AFPMPlayerController
 *
 * Base PlayerController for Faldoran Prime. Handles:
 *   - Client UI management (login, character select, character creation)
 *   - Server RPCs for all account and character operations
 *   - Authenticated account tracking (server-side only)
 *   - Character spawn/possess flow (server-authoritative)
 *
 * UI flow:
 *   1. Client BeginPlay -> show login widget
 *   2. Login success -> request character list
 *   3. Character list received -> show character select
 *      a. If no characters -> auto-transition to character creation
 *   4. Enter world -> server spawns + possesses AFPMPlayerCharacter
 *   5. Hide all UI, set input mode to Game
 *
 * PROTOTYPE NOTE: Credentials are sent via RPC (unencrypted UE channel).
 * Production MUST use TLS/DTLS. See 00_Rules_and_Constraints.md.
 */
UCLASS()
class FALDORANPRIMEMMO_API AFPMPlayerController : public APlayerController {
  GENERATED_BODY()

public:
  AFPMPlayerController();

  // --- Client-side helpers (called by widgets) ---

  /** Package credentials and send login request to server via RPC. */
  void RequestLogin(const FString &Username, const FString &Password);

  /** Package credentials and send account creation request to server. */
  void RequestCreateAccount(const FString &Username, const FString &Password);

  /** Package creation request and send to server via RPC. */
  void RequestCreateCharacter(const FFPMCharacterCreationRequest &Request);

  /** Request character list from the server (called after login). */
  void RequestCharacterList();

  /** Request entering the world with the given character. */
  void RequestEnterWorld(const FGuid &CharacterId);

  /** Return to the login screen (from character creation Back button). */
  void ReturnToLogin();

  /** Transition from character select to character creation. */
  void TransitionToCharacterCreation();

  /** Transition from character creation back to character select. */
  void TransitionToCharacterSelect();

protected:
  virtual void BeginPlay() override;
  virtual void OnPossess(APawn *InPawn) override;

  // --- Server RPCs (executed on the dedicated server) ---

  UFUNCTION(Server, Reliable)
  void ServerRequestLogin(FFPMLoginRequest Request);

  UFUNCTION(Server, Reliable)
  void ServerRequestCreateAccount(FFPMLoginRequest Request);

  UFUNCTION(Server, Reliable)
  void ServerRequestCreateCharacter(FFPMCharacterCreationRequest Request);

  UFUNCTION(Server, Reliable)
  void ServerRequestCharacterList();

  UFUNCTION(Server, Reliable)
  void ServerRequestEnterWorld(FGuid CharacterId);

  // --- Client RPCs (executed on the owning client) ---

  UFUNCTION(Client, Reliable)
  void ClientReceiveLoginResult(FFPMLoginResult Result);

  UFUNCTION(Client, Reliable)
  void ClientReceiveCreateAccountResult(FFPMLoginResult Result);

  UFUNCTION(Client, Reliable)
  void ClientReceiveCreateCharacterResult(FFPMCharacterCreationResult Result);

  UFUNCTION(Client, Reliable)
  void
  ClientReceiveCharacterList(const TArray<FFPMCharacterSummary> &Characters);

  /** Notify the client that world entry succeeded — hide UI. */
  UFUNCTION(Client, Reliable)
  void ClientEnterWorldSuccess(const FVector &InSpawnLocation);

  /** Notify the client that world entry failed. */
  UFUNCTION(Client, Reliable)
  void ClientEnterWorldFailed(const FString &ErrorMessage);

private:
  // --- Login Widget ---

  UPROPERTY(EditDefaultsOnly, Category = "FPM|UI")
  TSubclassOf<UFPMLoginWidget> LoginWidgetClass;

  UPROPERTY()
  TObjectPtr<UFPMLoginWidget> LoginWidget;

  void ShowLoginWidget();
  void HideLoginWidget();

  // --- Character Creation Widget ---

  UPROPERTY(EditDefaultsOnly, Category = "FPM|UI")
  TSubclassOf<UFPMCharacterCreationWidget> CharacterCreationWidgetClass;

  UPROPERTY()
  TObjectPtr<UFPMCharacterCreationWidget> CharacterCreationWidget;

  void ShowCharacterCreationWidget();
  void HideCharacterCreationWidget();

  // --- Character Select Widget ---

  UPROPERTY(EditDefaultsOnly, Category = "FPM|UI")
  TSubclassOf<UFPMCharacterSelectWidget> CharacterSelectWidgetClass;

  UPROPERTY()
  TObjectPtr<UFPMCharacterSelectWidget> CharacterSelectWidget;

  void ShowCharacterSelectWidget();
  void HideCharacterSelectWidget();

  /** Hide ALL UI widgets and switch input to game mode. */
  void HideAllUIAndEnterGame();

  // --- Authenticated State (Server-Only) ---

  /**
   * The authenticated account UUID for this connection.
   * Set on the server after successful login. NOT replicated.
   * Used to authorize all subsequent requests.
   */
  FGuid AuthenticatedAccountId;

  /** Whether this connection has been authenticated. Server-side only. */
  bool bIsAuthenticated = false;

  /**
   * The character UUID currently being played by this connection.
   * Server-side only. Used for disconnect cleanup.
   */
  FGuid ActiveCharacterId;
};
