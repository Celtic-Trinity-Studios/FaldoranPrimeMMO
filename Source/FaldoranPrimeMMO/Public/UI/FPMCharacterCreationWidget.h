// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMCharacterCreationWidget.generated.h"

class AFPMCharacterPreviewActor;
class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UImage;
class USlider;
class UTextBlock;
class UVerticalBox;

/**
 * UFPMCharacterCreationWidget
 *
 * Programmatically builds the character creation UI with the
 * "Glass & Gold" theme (matching login and character select).
 *
 * Three-column layout:
 *   Left   — Physical attributes (name, gender, appearance sliders)
 *   Center — 3D character preview (scene capture render target)
 *   Right  — Affinities (playstyle & magical point pools)
 *
 * The Widget Blueprint only needs an empty root CanvasPanel.
 * Everything else is created in BuildUI().
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterCreationWidget : public UUserWidget {
  GENERATED_BODY()

public:
  UFPMCharacterCreationWidget(const FObjectInitializer &ObjectInitializer);

  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetResultMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;
  virtual void NativeDestruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float DeltaTime) override;

  virtual FReply
  NativeOnMouseButtonDown(const FGeometry &InGeometry,
                          const FPointerEvent &InMouseEvent) override;
  virtual FReply
  NativeOnMouseButtonUp(const FGeometry &InGeometry,
                        const FPointerEvent &InMouseEvent) override;
  virtual FReply NativeOnMouseMove(const FGeometry &InGeometry,
                                   const FPointerEvent &InMouseEvent) override;
  virtual FReply NativeOnMouseWheel(const FGeometry &InGeometry,
                                    const FPointerEvent &InMouseEvent) override;
  virtual FReply
  NativeOnMouseButtonDoubleClick(const FGeometry &InGeometry,
                                 const FPointerEvent &InMouseEvent) override;

private:
  // --- UI Construction ---
  void BuildUI();

  /** Helper: add a gold section header label to a vertical box. */
  void AddSectionLabel(UVerticalBox *Parent, const FString &Text);
  /** Helper: add a muted sub-label to a vertical box. */
  void AddSubLabel(UVerticalBox *Parent, const FString &Text);
  /** Helper: create a slider row (label + slider) and return the slider. */
  USlider *AddSliderRow(UVerticalBox *Parent, const FString &Label, float Min,
                        float Max, float Default);
  /** Helper: create an affinity slider row with a value readout. */
  USlider *AddAffinityRow(UVerticalBox *Parent, const FString &Label, float Max,
                          float Default, UTextBlock *&OutValueText);
  /** Helper: create a styled button (gold filled or ghost outline). */
  UButton *MakeButton(const FString &Label, bool bGoldFilled);
  /** Helper: apply Glass & Gold theme to a combo box for readability. */
  void StyleComboBox(UComboBoxString *CB);

  // --- Core Widgets ---
  UPROPERTY() TObjectPtr<UImage> BackgroundImage;
  UPROPERTY() TObjectPtr<UEditableTextBox> NameInput;
  UPROPERTY() TObjectPtr<UButton> GenderMaleBtn;
  UPROPERTY() TObjectPtr<UButton> GenderFemaleBtn;
  UPROPERTY() TObjectPtr<USlider> BodyTypeSlider;
  UPROPERTY() TObjectPtr<UComboBoxString> HairStyleComboBox;
  UPROPERTY() TObjectPtr<UComboBoxString> SpeciesComboBox;
  UPROPERTY() TObjectPtr<UImage> PreviewImage;
  UPROPERTY() TObjectPtr<UTextBlock> ResultText;
  UPROPERTY() TObjectPtr<UButton> SubmitButton;
  UPROPERTY() TObjectPtr<UButton> BackButton;

  // --- Tab System ---
  UPROPERTY() TArray<TObjectPtr<UButton>> TabButtons;
  UPROPERTY() TArray<TObjectPtr<UVerticalBox>> TabPanels;
  int32 ActiveTabIndex = 0;
  void SwitchTab(int32 Index);
  void UpdateTabButtonStyles();
  UFUNCTION() void OnTabIdentityClicked();
  UFUNCTION() void OnTabBodyClicked();
  UFUNCTION() void OnTabFaceClicked();
  UFUNCTION() void OnTabHairClicked();

  // Color pickers: each array is [Hue, Saturation, Value]
  UPROPERTY() TArray<TObjectPtr<USlider>> SkinSliders;
  UPROPERTY() TObjectPtr<UImage> SkinColorPreview;
  UPROPERTY() TArray<TObjectPtr<USlider>> EyeSliders;
  UPROPERTY() TObjectPtr<UImage> EyeColorPreview;
  UPROPERTY() TArray<TObjectPtr<USlider>> HairColorSliders;
  UPROPERTY() TObjectPtr<UImage> HairColorPreview;

  /** Read HSV slider values and convert to linear RGB. */
  FLinearColor
  GetColorFromHSVSliders(const TArray<TObjectPtr<USlider>> &Sliders) const;
  /** Update a preview swatch Image from an HSV slider group. */
  void RefreshColorPreview(const TArray<TObjectPtr<USlider>> &Sliders,
                           UImage *Preview);

  // Morph sliders: [Jaw, Nose, Brow, Lips]
  UPROPERTY() TArray<TObjectPtr<USlider>> MorphSliders;

  // Playstyle affinity sliders & value labels (pool of 600)
  UPROPERTY() TArray<TObjectPtr<USlider>> PlaystyleSliders;
  UPROPERTY() TArray<TObjectPtr<UTextBlock>> PlaystyleValues;
  UPROPERTY() TObjectPtr<UTextBlock> PlaystyleTotalText;

  // Magical affinity sliders & value labels (pool of 800)
  UPROPERTY() TArray<TObjectPtr<USlider>> MagicalSliders;
  UPROPERTY() TArray<TObjectPtr<UTextBlock>> MagicalValues;
  UPROPERTY() TObjectPtr<UTextBlock> MagicalTotalText;

  // --- Callbacks ---
  UFUNCTION() void OnSubmitClicked();
  UFUNCTION() void OnBackClicked();
  UFUNCTION() void OnGenderMaleClicked();
  UFUNCTION() void OnGenderFemaleClicked();
  UFUNCTION() void OnAppearanceChanged(float Value);
  UFUNCTION()
  void OnHairStyleChanged(FString SelectedItem,
                          ESelectInfo::Type SelectionType);
  UFUNCTION() void OnPlaystyleChanged(float Value);
  UFUNCTION() void OnMagicalChanged(float Value);
  UFUNCTION()
  void OnSpeciesChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

  // --- Preview Actor ---
  void SpawnPreviewActor();
  void DestroyPreviewActor();
  void UpdatePreviewFromSliders();
  void UpdateGenderButtons();
  void UpdatePlaystyleTotal();
  void UpdateMagicalTotal();
  void PopulateHairStyles();

  UPROPERTY() TObjectPtr<AFPMCharacterPreviewActor> PreviewActor;

  bool bIsFemale = false;
  bool bIsOrbitDragging = false;
  bool bRotatingLeft = false;
  bool bRotatingRight = false;

  static constexpr float OrbitSensitivity = 0.5f;
  static constexpr float ButtonRotateSpeed = 90.0f;
  static constexpr float ButtonZoomStep = 2.0f;
  static const FVector PreviewSpawnOffset;

  // CC5 morph target names
  static const FName MorphNames[4];
};
