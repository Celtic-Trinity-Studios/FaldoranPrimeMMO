// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UCanvasPanel;
class UImage;
class UVerticalBox;
class USizeBox;
class UBorder;
class UOverlay;
class USpacer;

/**
 * UFPMLoginWidget
 *
 * Self-contained login screen widget for Faldoran Prime.
 *
 * Builds its entire UI programmatically in C++ with a full-screen
 * opaque dark background, so it completely hides whatever level is
 * loaded behind it. Features a themed "Glass & Gold" centered panel
 * with title, input fields, buttons, and status text.
 *
 * NOTE: The WBP_LoginScreen Blueprint should have its root be an
 * empty Canvas Panel (no children). All visual children are created
 * in NativeConstruct.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMLoginWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /** Update the result text shown to the user (e.g., error or success). */
  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetResultMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;

private:
  // --- Programmatically created widgets ---

  /** Full-screen dark background image that hides the world. */
  UPROPERTY()
  TObjectPtr<UImage> BackgroundImage;

  /** Title text ("FALDORAN PRIME"). */
  UPROPERTY()
  TObjectPtr<UTextBlock> TitleText;

  /** Subtitle / tagline text. */
  UPROPERTY()
  TObjectPtr<UTextBlock> SubtitleText;

  /** Server status indicator text. */
  UPROPERTY()
  TObjectPtr<UTextBlock> StatusText;

  UPROPERTY()
  TObjectPtr<UEditableTextBox> UsernameInput;

  UPROPERTY()
  TObjectPtr<UEditableTextBox> PasswordInput;

  UPROPERTY()
  TObjectPtr<UButton> LoginButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> LoginButtonText;

  UPROPERTY()
  TObjectPtr<UButton> CreateAccountButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> CreateAccountButtonText;

  UPROPERTY()
  TObjectPtr<UTextBlock> ResultText;

  UPROPERTY()
  TObjectPtr<UTextBlock> VersionText;

  UPROPERTY()
  TObjectPtr<UTextBlock> CopyrightText;

  // --- Helpers ---

  /** Build the entire UI tree and add it to the root canvas. */
  void BuildUI();

  /** Called when the Login button is clicked. */
  UFUNCTION()
  void OnLoginClicked();

  /** Called when the Create Account button is clicked. */
  UFUNCTION()
  void OnCreateAccountClicked();
};
