// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMCharacterCreationWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class USlider;
class UTextBlock;

/**
 * UFPMCharacterCreationWidget
 *
 * C++ backing class for the WBP_CharacterCreation Widget Blueprint.
 * Provides BindWidget links to UI elements and forwards the submit
 * action to the owning AFPMPlayerController, which sends a Server RPC.
 *
 * The Blueprint must contain widgets with these exact names:
 *   - NameInput (EditableTextBox)
 *   - BodyTypeSlider (Slider)
 *   - SkinRedSlider, SkinGreenSlider, SkinBlueSlider (Sliders)
 *   - HairStyleComboBox (ComboBoxString)
 *   - HairColorRedSlider, HairColorGreenSlider, HairColorBlueSlider (Sliders)
 *   - SubmitButton (Button)
 *   - BackButton (Button)
 *   - ResultText (TextBlock)
 *
 * PROTOTYPE NOTE: No 3D character preview yet (Mutable/CC5 deferred).
 * Sliders provide basic customization; visual feedback is text-only.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterCreationWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /** Update the result text shown to the user (e.g., error or success). */
  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetResultMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;

  // --- Bound Widgets (must match names in Blueprint) ---

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UEditableTextBox> NameInput;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> BodyTypeSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> SkinRedSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> SkinGreenSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> SkinBlueSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UComboBoxString> HairStyleComboBox;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> HairColorRedSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> HairColorGreenSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<USlider> HairColorBlueSlider;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> SubmitButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> BackButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UTextBlock> ResultText;

private:
  /** Called when the Submit button is clicked. Gathers UI state and sends. */
  UFUNCTION()
  void OnSubmitClicked();

  /** Called when the Back button is clicked. Returns to login screen. */
  UFUNCTION()
  void OnBackClicked();

  /** Populate the hair style combo box with available options. */
  void PopulateHairStyles();
};
