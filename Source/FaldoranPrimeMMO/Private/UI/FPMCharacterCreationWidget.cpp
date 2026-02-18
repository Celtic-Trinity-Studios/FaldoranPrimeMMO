// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMCharacterCreationWidget.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "Character/Preview/FPMCharacterPreviewActor.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Player/FPMPlayerController.h"
#include "Styling/SlateColor.h"


DEFINE_LOG_CATEGORY_STATIC(LogFPMCharCreate, Log, All);

const FVector UFPMCharacterCreationWidget::PreviewSpawnOffset(-50000, -50000,
                                                              -5000);
const FName UFPMCharacterCreationWidget::MorphNames[4] = {
    FName("Jaw_Width"), FName("Nose_Bridge"), FName("Brow_Ridge"),
    FName("Lip_Fullness")};

// =====================================================================
UFPMCharacterCreationWidget::UFPMCharacterCreationWidget(
    const FObjectInitializer &OI)
    : Super(OI) {}

// =====================================================================
// LIFECYCLE
// =====================================================================

void UFPMCharacterCreationWidget::NativeConstruct() {
  Super::NativeConstruct();
  BuildUI();
  SpawnPreviewActor();
  UpdateGenderButtons();
  PopulateHairStyles();
  UpdatePlaystyleTotal();
  UpdateMagicalTotal();
  UE_LOG(LogFPMCharCreate, Log,
         TEXT("FPM CharCreate: Widget built with Glass & Gold theme."));
}

void UFPMCharacterCreationWidget::NativeDestruct() {
  DestroyPreviewActor();
  Super::NativeDestruct();
}

void UFPMCharacterCreationWidget::NativeTick(const FGeometry &G, float Dt) {
  Super::NativeTick(G, Dt);
  if (!PreviewActor)
    return;
  if (bRotatingLeft)
    PreviewActor->AddYawRotation(-ButtonRotateSpeed * Dt);
  if (bRotatingRight)
    PreviewActor->AddYawRotation(ButtonRotateSpeed * Dt);
}

// =====================================================================
// PREVIEW ACTOR
// =====================================================================

void UFPMCharacterCreationWidget::SpawnPreviewActor() {
  UWorld *W = GetWorld();
  if (!W || PreviewActor)
    return;
  FActorSpawnParameters P;
  P.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  PreviewActor = W->SpawnActor<AFPMCharacterPreviewActor>(
      AFPMCharacterPreviewActor::StaticClass(), PreviewSpawnOffset,
      FRotator::ZeroRotator, P);
  if (!PreviewActor) {
    UE_LOG(LogFPMCharCreate, Warning,
           TEXT("FPM CharCreate: Failed to spawn preview actor."));
    return;
  }
  if (PreviewImage) {
    UTextureRenderTarget2D *RT = PreviewActor->GetRenderTarget();
    if (RT)
      PreviewImage->SetBrushResourceObject(RT);
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
  if (SkinSliders.Num() >= 3)
    PreviewActor->SetSkinTone(FLinearColor(SkinSliders[0]->GetValue(),
                                           SkinSliders[1]->GetValue(),
                                           SkinSliders[2]->GetValue(), 1.f));
  if (EyeSliders.Num() >= 3)
    PreviewActor->SetEyeColor(FLinearColor(EyeSliders[0]->GetValue(),
                                           EyeSliders[1]->GetValue(),
                                           EyeSliders[2]->GetValue(), 1.f));
  if (HairColorSliders.Num() >= 3)
    PreviewActor->SetHairColor(FLinearColor(
        HairColorSliders[0]->GetValue(), HairColorSliders[1]->GetValue(),
        HairColorSliders[2]->GetValue(), 1.f));
  if (HairStyleComboBox)
    PreviewActor->SetHairStyle(HairStyleComboBox->GetSelectedIndex());
  for (int i = 0; i < FMath::Min(MorphSliders.Num(), 4); ++i)
    PreviewActor->SetMorphValue(MorphNames[i], MorphSliders[i]->GetValue());
}

void UFPMCharacterCreationWidget::PopulateHairStyles() {
  if (!HairStyleComboBox)
    return;
  HairStyleComboBox->ClearOptions();
  static const TCHAR *Names[] = {
      TEXT("Bald"),     TEXT("Short"),   TEXT("Medium"), TEXT("Long"),
      TEXT("Ponytail"), TEXT("Braided"), TEXT("Mohawk"), TEXT("Dreadlocks")};
  for (const TCHAR *N : Names)
    HairStyleComboBox->AddOption(N);
  HairStyleComboBox->SetSelectedIndex(0);
}

// =====================================================================
// RESULT TEXT
// =====================================================================

void UFPMCharacterCreationWidget::SetResultMessage(const FString &Msg,
                                                   bool bErr) {
  if (!ResultText)
    return;
  ResultText->SetText(FText::FromString(Msg));
  ResultText->SetColorAndOpacity(
      bErr ? FSlateColor(FLinearColor::Red)
           : FSlateColor(FLinearColor(0.2f, 1.f, 0.2f, 1.f)));
}

// =====================================================================
// CALLBACKS
// =====================================================================

void UFPMCharacterCreationWidget::OnGenderMaleClicked() {
  bIsFemale = false;
  UpdateGenderButtons();
  if (PreviewActor)
    PreviewActor->SetIsFemale(false);
}

void UFPMCharacterCreationWidget::OnGenderFemaleClicked() {
  bIsFemale = true;
  UpdateGenderButtons();
  if (PreviewActor)
    PreviewActor->SetIsFemale(true);
}

void UFPMCharacterCreationWidget::OnAppearanceChanged(float) {
  UpdatePreviewFromSliders();
}

void UFPMCharacterCreationWidget::OnHairStyleChanged(FString,
                                                     ESelectInfo::Type) {
  if (PreviewActor && HairStyleComboBox)
    PreviewActor->SetHairStyle(HairStyleComboBox->GetSelectedIndex());
}

void UFPMCharacterCreationWidget::OnPlaystyleChanged(float) {
  for (int i = 0; i < PlaystyleSliders.Num(); ++i)
    if (PlaystyleValues.IsValidIndex(i))
      PlaystyleValues[i]->SetText(FText::FromString(FString::FromInt(
          FMath::RoundToInt32(PlaystyleSliders[i]->GetValue()))));
  UpdatePlaystyleTotal();
}

void UFPMCharacterCreationWidget::OnMagicalChanged(float) {
  for (int i = 0; i < MagicalSliders.Num(); ++i)
    if (MagicalValues.IsValidIndex(i))
      MagicalValues[i]->SetText(FText::FromString(FString::FromInt(
          FMath::RoundToInt32(MagicalSliders[i]->GetValue()))));
  UpdateMagicalTotal();
}

void UFPMCharacterCreationWidget::UpdatePlaystyleTotal() {
  int32 T = 0;
  for (auto &S : PlaystyleSliders)
    T += FMath::RoundToInt32(S->GetValue());
  if (!PlaystyleTotalText)
    return;
  PlaystyleTotalText->SetText(
      FText::FromString(FString::Printf(TEXT("PLAYSTYLE  %d / 600"), T)));
  FLinearColor C(0.933f, 0.804f, 0.553f, 1.0f); // gold
  if (T == 600)
    C = FLinearColor(0.2f, 1.f, 0.2f, 1.f);
  else if (T > 600)
    C = FLinearColor::Red;
  PlaystyleTotalText->SetColorAndOpacity(FSlateColor(C));
}

void UFPMCharacterCreationWidget::UpdateMagicalTotal() {
  int32 T = 0;
  for (auto &S : MagicalSliders)
    T += FMath::RoundToInt32(S->GetValue());
  if (!MagicalTotalText)
    return;
  MagicalTotalText->SetText(
      FText::FromString(FString::Printf(TEXT("MAGICAL  %d / 800"), T)));
  FLinearColor C(0.933f, 0.804f, 0.553f, 1.0f); // gold
  if (T == 800)
    C = FLinearColor(0.2f, 1.f, 0.2f, 1.f);
  else if (T > 800)
    C = FLinearColor::Red;
  MagicalTotalText->SetColorAndOpacity(FSlateColor(C));
}

// =====================================================================
// SUBMIT / BACK
// =====================================================================

void UFPMCharacterCreationWidget::OnSubmitClicked() {
  if (!NameInput)
    return;
  const FString Name = NameInput->GetText().ToString();
  if (Name.IsEmpty() || Name.Len() < 3 || Name.Len() > 20) {
    SetResultMessage(TEXT("Name must be 3-20 characters."), true);
    return;
  }

  FFPMCharacterCreationRequest Req;
  Req.CharacterName = Name;
  Req.BodyType = bIsFemale ? 1 : 0;

  if (SkinSliders.Num() >= 3)
    Req.SkinTone =
        FLinearColor(SkinSliders[0]->GetValue(), SkinSliders[1]->GetValue(),
                     SkinSliders[2]->GetValue(), 1.f);
  if (HairStyleComboBox)
    Req.HairStyle = static_cast<uint8>(HairStyleComboBox->GetSelectedIndex());
  if (HairColorSliders.Num() >= 3)
    Req.HairColor = FLinearColor(HairColorSliders[0]->GetValue(),
                                 HairColorSliders[1]->GetValue(),
                                 HairColorSliders[2]->GetValue(), 1.f);

  // Playstyle affinities
  static const EFPMPlaystyleAffinity PSEnum[] = {
      EFPMPlaystyleAffinity::Martial, EFPMPlaystyleAffinity::Ranged,
      EFPMPlaystyleAffinity::Magic,   EFPMPlaystyleAffinity::Crafting,
      EFPMPlaystyleAffinity::Social,  EFPMPlaystyleAffinity::Survival};
  for (int i = 0; i < PlaystyleSliders.Num() && i < 6; ++i) {
    FFPMPlaystyleAffinityEntry E;
    E.Affinity = PSEnum[i];
    E.Points = FMath::RoundToInt32(PlaystyleSliders[i]->GetValue());
    Req.PlaystyleAffinities.Add(E);
  }

  // Magical affinities
  static const EFPMMagicalAffinity MAEnum[] = {
      EFPMMagicalAffinity::Fire,   EFPMMagicalAffinity::Water,
      EFPMMagicalAffinity::Earth,  EFPMMagicalAffinity::Air,
      EFPMMagicalAffinity::Light,  EFPMMagicalAffinity::Shadow,
      EFPMMagicalAffinity::Nature, EFPMMagicalAffinity::Arcane};
  for (int i = 0; i < MagicalSliders.Num() && i < 8; ++i) {
    FFPMMagicalAffinityEntry E;
    E.Affinity = MAEnum[i];
    E.Points = FMath::RoundToInt32(MagicalSliders[i]->GetValue());
    Req.MagicalAffinities.Add(E);
  }

  auto *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestCreateCharacter(Req);
    SetResultMessage(TEXT("Creating character..."), false);
  } else {
    SetResultMessage(TEXT("Error: No player controller."), true);
  }
}

void UFPMCharacterCreationWidget::OnBackClicked() {
  auto *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC)
    PC->TransitionToCharacterSelect();
}

// =====================================================================
// MOUSE INPUT: orbit, zoom, double-click reset
// =====================================================================

FReply
UFPMCharacterCreationWidget::NativeOnMouseButtonDown(const FGeometry &G,
                                                     const FPointerEvent &E) {
  if (E.GetEffectingButton() == EKeys::RightMouseButton) {
    bIsOrbitDragging = true;
    return FReply::Handled().CaptureMouse(TakeWidget());
  }
  return Super::NativeOnMouseButtonDown(G, E);
}

FReply
UFPMCharacterCreationWidget::NativeOnMouseButtonUp(const FGeometry &G,
                                                   const FPointerEvent &E) {
  if (E.GetEffectingButton() == EKeys::RightMouseButton) {
    bIsOrbitDragging = false;
    return FReply::Handled().ReleaseMouseCapture();
  }
  return Super::NativeOnMouseButtonUp(G, E);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseMove(const FGeometry &G,
                                                      const FPointerEvent &E) {
  if (bIsOrbitDragging && PreviewActor)
    PreviewActor->AddYawRotation(E.GetCursorDelta().X * OrbitSensitivity);
  return Super::NativeOnMouseMove(G, E);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseWheel(const FGeometry &G,
                                                       const FPointerEvent &E) {
  if (PreviewActor) {
    PreviewActor->AddZoom(-E.GetWheelDelta());
    return FReply::Handled();
  }
  return Super::NativeOnMouseWheel(G, E);
}

FReply UFPMCharacterCreationWidget::NativeOnMouseButtonDoubleClick(
    const FGeometry &G, const FPointerEvent &E) {
  if (PreviewActor) {
    PreviewActor->ResetCameraToFront();
    return FReply::Handled();
  }
  return Super::NativeOnMouseButtonDoubleClick(G, E);
}
