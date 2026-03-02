// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Account/FPMAccountTypes.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "FPMPlayerController.generated.h"

class UFPMCharacterCreationWidget;
class UFPMCharacterSelectWidget;
class UFPMEscMenuWidget;
class UFPMLoginWidget;

/**
 * AFPMPlayerController
 *
 * Base PlayerController for Faldoran Prime. Handles:
 *   - Client UI management (login, character creation)
 *   - Server RPCs for all account and character operations
 *   - Authenticated account tracking (server-side only)
 *   - Character spawn/possess flow (server-authoritative)
 *
 * UI flow (one character per account — no character select):
 *   1. Client BeginPlay -> show login widget
 *   2. Login success -> request character list
 *   3. Character list received:
 *      a. If character exists -> auto-enter world (skip select)
 *      b. If no character    -> show character creation
 *   4. Character created -> auto-enter world
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

  /**
   * Initiate the save-and-logout sequence.
   * Called by the ESC widget's "Logout & Save" button.
   * Fires ServerSaveAndLogout RPC which persists position then
   * confirms via ClientSaveComplete.
   */
  void RequestSaveAndLogout();

  /**
   * Perform the actual client disconnect.
   * Called by the ESC widget countdown OR by ClientSaveComplete after
   * the player has read the "✓ Saved" message.
   */
  void ExecuteLogout();

  /** Return to the login screen (from character creation Back button). */
  void ReturnToLogin();

  /** Transition from character select to character creation. */
  void TransitionToCharacterCreation();

  /** Transition from character creation back to character select. */
  void TransitionToCharacterSelect();

  // --- ESC Menu ---

  /** Show the in-game ESC / pause menu over the world. */
  void ShowEscMenu();

  /** Hide and destroy the ESC menu, restoring game input. */
  void HideEscMenu();

  /** Toggle the ESC menu open/closed. Bound to the ESC key action. */
  void ToggleEscMenu();

protected:
  virtual void BeginPlay() override;
  virtual void OnPossess(APawn *InPawn) override;
  virtual void OnUnPossess() override;

  /**
   * Bind the "OpenEscMenu" input action (mapped to ESC key in the
   * DefaultInput.ini [/Script/Engine.InputSettings] AxisMappings section).
   */
  virtual void SetupInputComponent() override;

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

  /**
   * Server: save current pawn position to the database, then
   * confirm via ClientSaveComplete.
   * Reliable: position MUST be persisted before logout.
   */
  UFUNCTION(Server, Reliable)
  void ServerSaveAndLogout();

  /**
   * Client: called when the server has finished (or failed) the DB save.
   * Tells the ESC widget to show "✓ Saved" and starts the disconnect.
   */
  UFUNCTION(Client, Reliable)
  void ClientSaveComplete(bool bSuccess);

public:
  // --- Server-side state accessors (read-only, for GameMode use) ---

  /** Returns true if this connection has been authenticated server-side. */
  bool IsAuthenticated() const { return bIsAuthenticated; }

  /** Returns the active character GUID (invalid if not in-world). */
  const FGuid &GetActiveCharacterId() const { return ActiveCharacterId; }

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

  // --- ESC Menu Widget ---

  UPROPERTY(EditDefaultsOnly, Category = "FPM|UI")
  TSubclassOf<UFPMEscMenuWidget> EscMenuWidgetClass;

  UPROPERTY()
  TObjectPtr<UFPMEscMenuWidget> EscMenuWidget;

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

  // --- Rate Limiting & Auth Lockout ---

  /** Maximum allowed string length for RPC string inputs. */
  static constexpr int32 MaxRPCStringLength = 256;

  /** Maximum login attempts before lockout. */
  static constexpr int32 MaxLoginAttempts = 10;

  /** Login rate limit: max attempts per window. */
  static constexpr int32 MaxLoginAttemptsPerWindow = 5;

  /** Account creation rate limit: max per window. */
  static constexpr int32 MaxCreateAccountPerWindow = 3;

  /** Rate limit window duration in seconds. */
  static constexpr double RateLimitWindowSeconds = 60.0;

  /** Failed login counter for lockout. */
  int32 FailedLoginAttempts = 0;

  /** Whether this connection is locked out from login. */
  bool bIsLockedOut = false;

  /** Timestamps of recent login attempts for rate limiting. */
  TArray<double> LoginAttemptTimestamps;

  /** Timestamps of recent account creation attempts. */
  TArray<double> CreateAccountTimestamps;

  /**
   * Check if an action is rate limited based on the given timestamps.
   * Prunes old timestamps and returns true if limit exceeded.
   */
  static bool IsRateLimited(TArray<double> &Timestamps, int32 MaxPerWindow,
                            double WindowSeconds);

  /** Clamp an FString to MaxLen characters. */
  static FString ClampString(const FString &Input, int32 MaxLen);
};
