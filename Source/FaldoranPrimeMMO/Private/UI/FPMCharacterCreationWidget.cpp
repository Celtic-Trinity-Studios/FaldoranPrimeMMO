// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMCharacterCreationWidget.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "Character/Preview/FPMCharacterPreviewActor.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Player/FPMPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterCreationWidget, Log, All);

const FVector UFPMCharacterCreationWidget::PreviewSpawnOffset(-50000.0f,
                                                              -50000.0f,
                                                              -5000.0f);

// -------------------------------------------------------------------
// Constructor — make widget focusable so it receives mouse events
// -------------------------------------------------------------------

UFPMCharacterCreationWidget::UFPMCharacterCreationWidget(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer) {
  SetIsFocusable(true);
}

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::NativeConstruct() {
  Super::NativeConstruct();

  // --- Button bindings ---
  if (SubmitButton) {
    SubmitButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnSubmitClicked);
  }
  if (BackButton) {
    BackButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnBackClicked);
  }

  // --- Body type slider ---
  if (BodyTypeSlider) {
    BodyTypeSlider->SetMinValue(0.0f);
    BodyTypeSlider->SetMaxValue(3.0f);
    BodyTypeSlider->SetStepSize(1.0f);
    BodyTypeSlider->SetValue(0.0f);
    BodyTypeSlider->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnBodyTypeSliderChanged);
  }

  // --- Skin color sliders ---
  auto BindSkinSlider = [this](USlider *S, float Default) {
    if (!S)
      return;
    S->SetValue(Default);
    S->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnSkinSliderChanged);
  };
  BindSkinSlider(SkinRedSlider, 0.8f);
  BindSkinSlider(SkinGreenSlider, 0.6f);
  BindSkinSlider(SkinBlueSlider, 0.5f);

  // --- Hair color sliders ---
  auto BindHairSlider = [this](USlider *S, float Default) {
    if (!S)
      return;
    S->SetValue(Default);
    S->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnHairColorSliderChanged);
  };
  BindHairSlider(HairColorRedSlider, 0.2f);
  BindHairSlider(HairColorGreenSlider, 0.15f);
  BindHairSlider(HairColorBlueSlider, 0.1f);

  // --- Hair style combo box ---
  PopulateHairStyles();
  if (HairStyleComboBox) {
    HairStyleComboBox->OnSelectionChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnHairStyleChanged);
  }

  // --- Playstyle affinity sliders (pool 600, each 90-150, default 100) ---
  auto BindPlaystyleSlider = [this](USlider *S) {
    if (!S)
      return;
    S->SetMinValue(90.0f);
    S->SetMaxValue(150.0f);
    S->SetStepSize(1.0f);
    S->SetValue(100.0f);
    S->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnPlaystyleAffinityChanged);
  };
  BindPlaystyleSlider(MartialSlider);
  BindPlaystyleSlider(RangedSlider);
  BindPlaystyleSlider(MagicSlider);
  BindPlaystyleSlider(CraftingSlider);
  BindPlaystyleSlider(SocialSlider);
  BindPlaystyleSlider(SurvivalSlider);
  UpdatePlaystyleTotal();

  // --- Magical affinity sliders (pool 800, each 90-150, default 100) ---
  auto BindMagicalSlider = [this](USlider *S) {
    if (!S)
      return;
    S->SetMinValue(90.0f);
    S->SetMaxValue(150.0f);
    S->SetStepSize(1.0f);
    S->SetValue(100.0f);
    S->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnMagicalAffinityChanged);
  };
  BindMagicalSlider(FireSlider);
  BindMagicalSlider(WaterSlider);
  BindMagicalSlider(EarthSlider);
  BindMagicalSlider(AirSlider);
  BindMagicalSlider(LightSlider);
  BindMagicalSlider(ShadowSlider);
  BindMagicalSlider(NatureSlider);
  BindMagicalSlider(ArcaneSlider);
  UpdateMagicalTotal();

  // --- Optional camera buttons ---
  if (RotateLeftButton) {
    RotateLeftButton->OnPressed.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnRotateLeftClicked);
    RotateLeftButton->OnReleased.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnRotateLeftClicked);
  }
  if (RotateRightButton) {
    RotateRightButton->OnPressed.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnRotateRightClicked);
    RotateRightButton->OnReleased.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnRotateRightClicked);
  }
  if (ZoomInButton) {
    ZoomInButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnZoomInClicked);
  }
  if (ZoomOutButton) {
    ZoomOutButton->OnClicked.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnZoomOutClicked);
  }

  SetResultMessage(TEXT(""), false);
  SpawnPreviewActor();

  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: Widget constructed with 3D preview."));
}

void UFPMCharacterCreationWidget::NativeDestruct() {
  DestroyPreviewActor();
  Super::NativeDestruct();
}

void UFPMCharacterCreationWidget::NativeTick(const FGeometry &MyGeometry,
                                             float DeltaTime) {
  Super::NativeTick(MyGeometry, DeltaTime);
  if (PreviewActor) {
    if (bRotatingLeft) {
      PreviewActor->AddYawRotation(-ButtonRotateSpeed * DeltaTime);
    }
    if (bRotatingRight) {
      PreviewActor->AddYawRotation(ButtonRotateSpeed * DeltaTime);
    }
  }
}

// -------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::SetResultMessage(const FString &Message,
                                                   bool bIsError) {
  if (!ResultText)
    return;
  ResultText->SetText(FText::FromString(Message));
  const FSlateColor Color =
      bIsError ? FSlateColor(FLinearColor::Red)
               : FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f));
  ResultText->SetColorAndOpacity(Color);
}

// -------------------------------------------------------------------
// Button Handlers
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::OnSubmitClicked() {
  if (!NameInput)
    return;

  const FString Name = NameInput->GetText().ToString();
  if (Name.IsEmpty()) {
    SetResultMessage(TEXT("Please enter a character name."), true);
    return;
  }

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

  // --- Playstyle affinities ---
  auto AddPlaystyle = [&](EFPMPlaystyleAffinity Aff, USlider *S) {
    if (!S)
      return;
    FFPMPlaystyleAffinityEntry Entry;
    Entry.Affinity = Aff;
    Entry.Points = FMath::RoundToInt32(S->GetValue());
    Request.PlaystyleAffinities.Add(Entry);
  };
  AddPlaystyle(EFPMPlaystyleAffinity::Martial, MartialSlider);
  AddPlaystyle(EFPMPlaystyleAffinity::Ranged, RangedSlider);
  AddPlaystyle(EFPMPlaystyleAffinity::Magic, MagicSlider);
  AddPlaystyle(EFPMPlaystyleAffinity::Crafting, CraftingSlider);
  AddPlaystyle(EFPMPlaystyleAffinity::Social, SocialSlider);
  AddPlaystyle(EFPMPlaystyleAffinity::Survival, SurvivalSlider);

  // --- Magical affinities ---
  auto AddMagical = [&](EFPMMagicalAffinity Aff, USlider *S) {
    if (!S)
      return;
    FFPMMagicalAffinityEntry Entry;
    Entry.Affinity = Aff;
    Entry.Points = FMath::RoundToInt32(S->GetValue());
    Request.MagicalAffinities.Add(Entry);
  };
  AddMagical(EFPMMagicalAffinity::Fire, FireSlider);
  AddMagical(EFPMMagicalAffinity::Water, WaterSlider);
  AddMagical(EFPMMagicalAffinity::Earth, EarthSlider);
  AddMagical(EFPMMagicalAffinity::Air, AirSlider);
  AddMagical(EFPMMagicalAffinity::Light, LightSlider);
  AddMagical(EFPMMagicalAffinity::Shadow, ShadowSlider);
  AddMagical(EFPMMagicalAffinity::Nature, NatureSlider);
  AddMagical(EFPMMagicalAffinity::Arcane, ArcaneSlider);

  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: Submitting — Playstyle entries=%d, Magical "
              "entries=%d"),
         Request.PlaystyleAffinities.Num(), Request.MagicalAffinities.Num());

  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestCreateCharacter(Request);
    SetResultMessage(TEXT("Creating character..."), false);
  } else {
    SetResultMessage(TEXT("Error: No player controller found."), true);
  }
}

void UFPMCharacterCreationWidget::OnBackClicked() {
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->TransitionToCharacterSelect();
  }
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::PopulateHairStyles() {
  if (!HairStyleComboBox)
    return;
  HairStyleComboBox->ClearOptions();
  static const TCHAR *HairStyleNames[] = {
      TEXT("Bald"),     TEXT("Short"),   TEXT("Medium"), TEXT("Long"),
      TEXT("Ponytail"), TEXT("Braided"), TEXT("Mohawk"), TEXT("Dreadlocks")};
  for (const TCHAR *StyleName : HairStyleNames) {
    HairStyleComboBox->AddOption(StyleName);
  }
  HairStyleComboBox->SetSelectedIndex(0);
}

// -------------------------------------------------------------------
// Preview Actor Management
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::SpawnPreviewActor() {
  UWorld *World = GetWorld();
  if (!World || PreviewActor)
    return;

  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  PreviewActor = World->SpawnActor<AFPMCharacterPreviewActor>(
      AFPMCharacterPreviewActor::StaticClass(), PreviewSpawnOffset,
      FRotator::ZeroRotator, Params);

  if (!PreviewActor) {
    UE_LOG(LogFPMCharacterCreationWidget, Warning,
           TEXT("FPM CharCreate: Failed to spawn preview actor."));
    return;
  }

  if (PreviewImage) {
    // Make PreviewImage hit-testable so right-click and scroll events
    // register on it and bubble up to our NativeOnMouse overrides.
    PreviewImage->SetVisibility(ESlateVisibility::Visible);

    UTextureRenderTarget2D *RT = PreviewActor->GetRenderTarget();
    if (RT) {
      PreviewImage->SetBrushResourceObject(RT);
      UE_LOG(LogFPMCharacterCreationWidget, Log,
             TEXT("FPM CharCreate: Render target bound to image "
                  "(Visibility=Visible for mouse input)."));
    }
  }
  UpdatePreviewFromSliders();
}

void UFPMCharacterCreationWidget::DestroyPreviewActor() {
  if (PreviewActor) {
    PreviewActor->Destroy();
    PreviewActor = nullptr;
  }
}

void UFPMCharacterCreationWidget::UpdatePreviewFromSliders() {
  if (!PreviewActor)
    return;
  if (SkinRedSlider && SkinGreenSlider && SkinBlueSlider) {
    PreviewActor->SetSkinTone(FLinearColor(SkinRedSlider->GetValue(),
                                           SkinGreenSlider->GetValue(),
                                           SkinBlueSlider->GetValue(), 1.0f));
  }
  if (HairColorRedSlider && HairColorGreenSlider && HairColorBlueSlider) {
    PreviewActor->SetHairColor(FLinearColor(
        HairColorRedSlider->GetValue(), HairColorGreenSlider->GetValue(),
        HairColorBlueSlider->GetValue(), 1.0f));
  }
  if (BodyTypeSlider) {
    PreviewActor->SetBodyType(
        static_cast<uint8>(FMath::RoundToInt32(BodyTypeSlider->GetValue())));
  }
  if (HairStyleComboBox) {
    PreviewActor->SetHairStyle(
        static_cast<uint8>(HairStyleComboBox->GetSelectedIndex()));
  }
}

// -------------------------------------------------------------------
// Appearance Slider Callbacks
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::OnBodyTypeSliderChanged(float Value) {
  const int32 BT = FMath::RoundToInt32(Value);
  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: BodyType slider = %d"), BT);
  if (PreviewActor) {
    PreviewActor->SetBodyType(static_cast<uint8>(BT));
  }
}

void UFPMCharacterCreationWidget::OnSkinSliderChanged(float /*Value*/) {
  if (!SkinRedSlider || !SkinGreenSlider || !SkinBlueSlider)
    return;
  const float R = SkinRedSlider->GetValue();
  const float G = SkinGreenSlider->GetValue();
  const float B = SkinBlueSlider->GetValue();
  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: Skin slider = (%.2f, %.2f, %.2f)"), R, G, B);
  if (PreviewActor) {
    PreviewActor->SetSkinTone(FLinearColor(R, G, B, 1.0f));
  }
}

void UFPMCharacterCreationWidget::OnHairColorSliderChanged(float /*Value*/) {
  if (!HairColorRedSlider || !HairColorGreenSlider || !HairColorBlueSlider)
    return;
  const float R = HairColorRedSlider->GetValue();
  const float G = HairColorGreenSlider->GetValue();
  const float B = HairColorBlueSlider->GetValue();
  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: HairColor slider = (%.2f, %.2f, %.2f)"), R, G,
         B);
  if (PreviewActor) {
    PreviewActor->SetHairColor(FLinearColor(R, G, B, 1.0f));
  }
}

void UFPMCharacterCreationWidget::OnHairStyleChanged(
    FString SelectedItem, ESelectInfo::Type /*SelectionType*/) {
  UE_LOG(LogFPMCharacterCreationWidget, Log,
         TEXT("FPM CharCreate: HairStyle changed to '%s'"), *SelectedItem);
  if (PreviewActor && HairStyleComboBox) {
    PreviewActor->SetHairStyle(
        static_cast<uint8>(HairStyleComboBox->GetSelectedIndex()));
  }
}

// -------------------------------------------------------------------
// Affinity Slider Callbacks
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::OnPlaystyleAffinityChanged(float /*Value*/) {
  UpdatePlaystyleTotal();
}

void UFPMCharacterCreationWidget::OnMagicalAffinityChanged(float /*Value*/) {
  UpdateMagicalTotal();
}

void UFPMCharacterCreationWidget::UpdatePlaystyleTotal() {
  int32 Total = 0;
  auto Add = [&](USlider *S) {
    if (S)
      Total += FMath::RoundToInt32(S->GetValue());
  };
  Add(MartialSlider);
  Add(RangedSlider);
  Add(MagicSlider);
  Add(CraftingSlider);
  Add(SocialSlider);
  Add(SurvivalSlider);

  if (PlaystyleTotalText) {
    const FString Txt =
        FString::Printf(TEXT("Playstyle (Total: %d/600)"), Total);
    PlaystyleTotalText->SetText(FText::FromString(Txt));
    // Red if over budget, green if exactly 600, yellow otherwise
    FSlateColor Color = FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f));
    if (Total == 600)
      Color = FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f));
    else if (Total > 600)
      Color = FSlateColor(FLinearColor::Red);
    PlaystyleTotalText->SetColorAndOpacity(Color);
  }
}

void UFPMCharacterCreationWidget::UpdateMagicalTotal() {
  int32 Total = 0;
  auto Add = [&](USlider *S) {
    if (S)
      Total += FMath::RoundToInt32(S->GetValue());
  };
  Add(FireSlider);
  Add(WaterSlider);
  Add(EarthSlider);
  Add(AirSlider);
  Add(LightSlider);
  Add(ShadowSlider);
  Add(NatureSlider);
  Add(ArcaneSlider);

  if (MagicalTotalText) {
    const FString Txt = FString::Printf(TEXT("Magical (Total: %d/800)"), Total);
    MagicalTotalText->SetText(FText::FromString(Txt));
    FSlateColor Color = FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f));
    if (Total == 800)
      Color = FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f));
    else if (Total > 800)
      Color = FSlateColor(FLinearColor::Red);
    MagicalTotalText->SetColorAndOpacity(Color);
  }
}

// -------------------------------------------------------------------
// Camera Control Callbacks
// -------------------------------------------------------------------

void UFPMCharacterCreationWidget::OnRotateLeftClicked() {
  bRotatingLeft = !bRotatingLeft;
}
void UFPMCharacterCreationWidget::OnRotateRightClicked() {
  bRotatingRight = !bRotatingRight;
}
void UFPMCharacterCreationWidget::OnZoomInClicked() {
  if (PreviewActor)
    PreviewActor->AddZoom(-ButtonZoomStep);
}
void UFPMCharacterCreationWidget::OnZoomOutClicked() {
  if (PreviewActor)
    PreviewActor->AddZoom(ButtonZoomStep);
}

// -------------------------------------------------------------------
// Mouse Input — Camera Orbit and Zoom
// -------------------------------------------------------------------

FReply UFPMCharacterCreationWidget::NativeOnMouseButtonDown(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) {
    bIsOrbitDragging = true;
    UE_LOG(LogFPMCharacterCreationWidget, Log,
           TEXT("FPM CharCreate: Orbit drag started."));
    return FReply::Handled().CaptureMouse(TakeWidget());
  }
  return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseButtonUp(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) {
    bIsOrbitDragging = false;
    return FReply::Handled().ReleaseMouseCapture();
  }
  return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseMove(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  if (bIsOrbitDragging && PreviewActor) {
    const FVector2D Delta = InMouseEvent.GetCursorDelta();
    if (FMath::Abs(Delta.X) > 0.1f) {
      UE_LOG(LogFPMCharacterCreationWidget, Log,
             TEXT("FPM: MouseMove Delta=%.2f"), Delta.X);
    }
    PreviewActor->AddYawRotation(Delta.X * OrbitSensitivity);
  }
  return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseWheel(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  if (PreviewActor) {
    PreviewActor->AddZoom(-InMouseEvent.GetWheelDelta());
    return FReply::Handled();
  }
  return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
