// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMCharacterCreationWidget.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Player/FPMPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterCreationWidget, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::NativeConstruct() {
  Super::NativeConstruct();

  // Bind button click events
  if (SubmitButton) {
    SubmitButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnSubmitClicked);
  }
  if (BackButton) {
    BackButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnBackClicked);
  }

  // Set slider ranges: body type 0-3 (integer steps), colors 0-1
  if (BodyTypeSlider) {
    BodyTypeSlider->SetMinValue(0.0f);
    BodyTypeSlider->SetMaxValue(3.0f);
    BodyTypeSlider->SetStepSize(1.0f);
    BodyTypeSlider->SetValue(0.0f);
  }

  // Color sliders default to a neutral skin tone
  if (SkinRedSlider) {
    SkinRedSlider->SetValue(0.8f);
  }
  if (SkinGreenSlider) {
    SkinGreenSlider->SetValue(0.6f);
  }
  if (SkinBlueSlider) {
    SkinBlueSlider->SetValue(0.5f);
  }

  // Hair color sliders default to dark brown
  if (HairColorRedSlider) {
    HairColorRedSlider->SetValue(0.2f);
  }
  if (HairColorGreenSlider) {
    HairColorGreenSlider->SetValue(0.15f);
  }
  if (HairColorBlueSlider) {
    HairColorBlueSlider->SetValue(0.1f);
  }

  PopulateHairStyles();

  // Clear any placeholder text
  SetResultMessage(TEXT(""), false);

  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: Widget constructed."));
}

// -------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::SetResultMessage(const FString &Message,
                                                   bool bIsError) {
  if (!ResultText) {
    return;
  }

  ResultText->SetText(FText::FromString(Message));

  // Red for errors, green for success
  const FSlateColor Color =
      bIsError ? FSlateColor(FLinearColor::Red)
               : FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f));
  ResultText->SetColorAndOpacity(Color);
}

// -------------------------------------------------------------------
// Button Handlers
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::OnSubmitClicked() {
  if (!NameInput) {
    return;
  }

  const FString Name = NameInput->GetText().ToString();
  if (Name.IsEmpty()) {
    SetResultMessage(TEXT("Please enter a character name."), true);
    return;
  }

  // Build the creation request from current UI state
  FFPMCharacterCreationRequest Request;
  Request.CharacterName = Name;

  if (BodyTypeSlider) {
    Request.BodyType =
        static_cast<uint8>(FMath::RoundToInt32(BodyTypeSlider->GetValue()));
  }

  if (SkinRedSlider && SkinGreenSlider && SkinBlueSlider) {
    Request.SkinTone =
        FLinearColor(SkinRedSlider->GetValue(), SkinGreenSlider->GetValue(),
                     SkinBlueSlider->GetValue(), 1.0f);
  }

  if (HairStyleComboBox) {
    Request.HairStyle =
        static_cast<uint8>(HairStyleComboBox->GetSelectedIndex());
  }

  if (HairColorRedSlider && HairColorGreenSlider && HairColorBlueSlider) {
    Request.HairColor = FLinearColor(HairColorRedSlider->GetValue(),
                                     HairColorGreenSlider->GetValue(),
                                     HairColorBlueSlider->GetValue(), 1.0f);
  }

  // Forward to the PlayerController which sends the Server RPC
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestCreateCharacter(Request);
    SetResultMessage(TEXT("Creating character..."), false);
  } else {
    SetResultMessage(TEXT("Error: No player controller found."), true);
  }
}

void UFPMCharacterCreationWidget::OnBackClicked() {
  // Return to character select (player is already authenticated)
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->TransitionToCharacterSelect();
  }
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::PopulateHairStyles() {
  if (!HairStyleComboBox) {
    return;
  }

  HairStyleComboBox->ClearOptions();

  // Hair style indices 0-7 (matches validator bounds in 5A)
  static const TCHAR *HairStyleNames[] = {
      TEXT("Bald"),     TEXT("Short"),   TEXT("Medium"), TEXT("Long"),
      TEXT("Ponytail"), TEXT("Braided"), TEXT("Mohawk"), TEXT("Dreadlocks")};

  for (const TCHAR *StyleName : HairStyleNames) {
    HairStyleComboBox->AddOption(StyleName);
  }

  HairStyleComboBox->SetSelectedIndex(0);
}
