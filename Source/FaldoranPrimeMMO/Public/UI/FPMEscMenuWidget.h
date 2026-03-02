// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMEscMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UImage;
class UVerticalBox;
class USizeBox;
class UBorder;
class UOverlay;
class USpacer;

/**
 * UFPMEscMenuWidget
 *
 * The in-game ESC / pause menu. Displayed on top of the game world
 * with a frosted-glass blurred backdrop feel. The player retains full
 * game audio and the world keeps simulating — this is an overlay, not
 * a true pause (dedicated server requirement).
 *
 * DESIGN: "Glass & Gold" — matches the login screen aesthetic so the
 * UI language is consistent throughout the game.
 *
 * LAYOUT (centered panel):
 *   ┌────────────────────────────────┐
 *   │   ≡  FALDORAN PRIME  ≡        │
 *   │   IN WORLD  ─────────────────  │
 *   │                                │
 *   │   [ RESUME ]                   │
 *   │   [ LOGOUT & SAVE ]            │
 *   │                                │
 *   │   Status: "Saving..." → "✓"   │
 *   └────────────────────────────────┘
 *
 * NOTE: WBP_EscMenu Blueprint root must be a single empty Canvas Panel.
 * All children are built in C++ via NativeConstruct → BuildUI().
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMEscMenuWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /**
   * Update the status line shown at the bottom of the menu.
   * Used to display "Saving..." and then "✓ Saved" before logout.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetStatusMessage(const FString &Message, bool bIsError = false);

protected:
  virtual void NativeConstruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float InDeltaTime) override;

private:
  // -----------------------------------------------------------------------
  //  Widget references (programmatically created)
  // -----------------------------------------------------------------------

  /** Semi-transparent full-screen backdrop (dims the world). */
  UPROPERTY()
  TObjectPtr<UImage> BackdropImage;

  /** Title text ("FALDORAN PRIME"). */
  UPROPERTY()
  TObjectPtr<UTextBlock> TitleText;

  /** "IN WORLD" subtitle / session label. */
  UPROPERTY()
  TObjectPtr<UTextBlock> SubtitleText;

  /** Resume button — hides ESC menu and returns to game. */
  UPROPERTY()
  TObjectPtr<UButton> ResumeButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> ResumeButtonText;

  /** Logout & Save button — triggers save then disconnect. */
  UPROPERTY()
  TObjectPtr<UButton> LogoutButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> LogoutButtonText;

  /** Status / feedback text (e.g. "Saving…", "✓ Saved — disconnecting"). */
  UPROPERTY()
  TObjectPtr<UTextBlock> StatusText;

  /** Thin horizontal separator line (gold tinted). */
  UPROPERTY()
  TObjectPtr<UImage> SeparatorLine;

  // -----------------------------------------------------------------------
  //  Animation state
  // -----------------------------------------------------------------------

  /** Accumulated time, drives shimmer and backdrop pulse. */
  float AnimTime = 0.0f;

  /** True once the logout sequence has been initiated (lock buttons). */
  bool bLogoutPending = false;

  /** Tracks how long the "✓ Saved" message has been visible. */
  float LogoutCountdown = 0.0f;

  /** Whether we are counting down to disconnect. */
  bool bCountingDown = false;

  // -----------------------------------------------------------------------
  //  Helpers
  // -----------------------------------------------------------------------

  /** Build the entire UI tree. */
  void BuildUI();

  /** Shared helper: make a styled gold-text label above a button. */
  static UTextBlock *MakeLabel(UObject *Outer, const FString &Text, int32 Size);

  /** Shared helper: make a primary (filled gold) button. */
  UButton *MakePrimaryButton(const FString &Label,
                             TObjectPtr<UTextBlock> &OutTextRef);

  /** Shared helper: make a secondary (ghost) button. */
  UButton *MakeSecondaryButton(const FString &Label,
                               TObjectPtr<UTextBlock> &OutTextRef);

  // -----------------------------------------------------------------------
  //  Button callbacks
  // -----------------------------------------------------------------------

  UFUNCTION()
  void OnResumeClicked();

  UFUNCTION()
  void OnLogoutClicked();
};
