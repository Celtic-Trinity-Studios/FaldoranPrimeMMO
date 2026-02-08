// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"


#include "FPMLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/**
 * UFPMLoginWidget
 *
 * C++ backing class for the WBP_LoginScreen Widget Blueprint.
 * Provides BindWidget links to UI elements and forwards button clicks
 * to the owning AFPMPlayerController, which sends Server RPCs.
 *
 * The Blueprint must contain widgets with these exact names:
 *   - UsernameInput (EditableTextBox)
 *   - PasswordInput (EditableTextBox)
 *   - LoginButton (Button)
 *   - CreateAccountButton (Button)
 *   - ResultText (TextBlock)
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

  // --- Bound Widgets (must match names in Blueprint) ---

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UEditableTextBox> UsernameInput;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UEditableTextBox> PasswordInput;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> LoginButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> CreateAccountButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UTextBlock> ResultText;

private:
  /** Called when the Login button is clicked. */
  UFUNCTION()
  void OnLoginClicked();

  /** Called when the Create Account button is clicked. */
  UFUNCTION()
  void OnCreateAccountClicked();
};
