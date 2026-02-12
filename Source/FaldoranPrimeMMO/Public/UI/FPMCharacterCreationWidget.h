// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "FPMCharacterCreationWidget.generated.h"

class AFPMCharacterPreviewActor;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UImage;
class USlider;
class UTextBlock;

/**
 * UFPMCharacterCreationWidget
 *
 * C++ backing class for the WBP_CharacterCreation Widget Blueprint.
 * Provides BindWidget links to UI elements and forwards the submit
 * action to the owning AFPMPlayerController, which sends a Server RPC.
 *
 * Manages a client-only AFPMCharacterPreviewActor that renders a 3D
 * mannequin preview. Slider changes call the preview actor's setters
 * in real-time. Mouse drag orbits the preview; mouse wheel zooms.
 *
 * Required widgets (BindWidget):
 *   - NameInput, BodyTypeSlider, SkinRedSlider, SkinGreenSlider,
 *     SkinBlueSlider, HairStyleComboBox, HairColorRedSlider,
 *     HairColorGreenSlider, HairColorBlueSlider, SubmitButton,
 *     BackButton, ResultText, PreviewImage
 *   - Affinity sliders: MartialSlider, RangedSlider, MagicSlider,
 *     CraftingSlider, SocialSlider, SurvivalSlider
 *   - Magical affinity sliders: FireSlider, WaterSlider, EarthSlider,
 *     AirSlider, LightSlider, ShadowSlider, NatureSlider, ArcaneSlider
 *   - PlaystyleTotalText, MagicalTotalText
 *
 * Optional widgets (BindWidgetOptional):
 *   - RotateLeftButton, RotateRightButton, ZoomInButton, ZoomOutButton
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterCreationWidget : public UUserWidget {
  GENERATED_BODY()

public:
  UFPMCharacterCreationWidget(const FObjectInitializer &ObjectInitializer);

  /** Update the result text shown to the user (e.g., error or success). */
  UFUNCTION(BlueprintCallable, Category = "FPM|UI")
  void SetResultMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;
  virtual void NativeDestruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float DeltaTime) override;

  // Mouse input for orbit/zoom over the preview
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

  // --- Required Bound Widgets: Appearance ---

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
  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UImage> PreviewImage;

  // --- Optional Bound Widgets: Playstyle Affinities (pool of 600) ---

  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> MartialSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> RangedSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> MagicSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> CraftingSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> SocialSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> SurvivalSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UTextBlock> PlaystyleTotalText;

  // --- Optional Bound Widgets: Magical Affinities (pool of 800) ---

  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> FireSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> WaterSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> EarthSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> AirSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> LightSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> ShadowSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> NatureSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<USlider> ArcaneSlider;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UTextBlock> MagicalTotalText;

  // --- Optional Camera Control Buttons ---

  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UButton> RotateLeftButton;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UButton> RotateRightButton;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UButton> ZoomInButton;
  UPROPERTY(meta = (BindWidgetOptional))
  TObjectPtr<UButton> ZoomOutButton;

private:
  UFUNCTION()
  void OnSubmitClicked();
  UFUNCTION()
  void OnBackClicked();

  void PopulateHairStyles();

  // --- Preview Actor Management ---
  void SpawnPreviewActor();
  void DestroyPreviewActor();
  void UpdatePreviewFromSliders();

  // --- Slider Callbacks ---
  UFUNCTION()
  void OnBodyTypeSliderChanged(float Value);
  UFUNCTION()
  void OnSkinSliderChanged(float Value);
  UFUNCTION()
  void OnHairColorSliderChanged(float Value);
  UFUNCTION()
  void OnHairStyleChanged(FString SelectedItem,
                          ESelectInfo::Type SelectionType);

  // --- Affinity Slider Callbacks ---
  UFUNCTION()
  void OnPlaystyleAffinityChanged(float Value);
  UFUNCTION()
  void OnMagicalAffinityChanged(float Value);

  /** Recalculate and display the playstyle points total. */
  void UpdatePlaystyleTotal();
  /** Recalculate and display the magical points total. */
  void UpdateMagicalTotal();

  // --- Camera Control Callbacks ---
  UFUNCTION()
  void OnRotateLeftClicked();
  UFUNCTION()
  void OnRotateRightClicked();
  UFUNCTION()
  void OnZoomInClicked();
  UFUNCTION()
  void OnZoomOutClicked();

  /** The client-only 3D character preview actor. */
  UPROPERTY()
  TObjectPtr<AFPMCharacterPreviewActor> PreviewActor;

  bool bIsOrbitDragging = false;
  bool bRotatingLeft = false;
  bool bRotatingRight = false;

  static constexpr float OrbitSensitivity = 0.5f;
  static constexpr float ButtonRotateSpeed = 90.0f;
  static constexpr float ButtonZoomStep = 2.0f;

  static const FVector PreviewSpawnOffset;
};
