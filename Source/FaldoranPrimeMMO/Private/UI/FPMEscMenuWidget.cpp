// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMEscMenuWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/FPMPlayerController.h"
#include "Styling/SlateColor.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMEscMenu, Log, All);

// ===========================================================================
//  Shared Color Palette (matches Login screen "Glass & Gold" theme)
// ===========================================================================
namespace FPMEscColors {
static const FLinearColor Backdrop(0.0f, 0.0f, 0.0f, 0.72f);
static const FLinearColor PanelBG(0.039f, 0.055f, 0.082f, 0.88f);
static const FLinearColor GoldPrimary(0.773f, 0.627f, 0.349f, 1.0f);
static const FLinearColor GoldLight(0.933f, 0.804f, 0.553f, 1.0f);
static const FLinearColor GoldDim(0.773f, 0.627f, 0.349f, 0.35f);
static const FLinearColor TextMain(0.878f, 0.878f, 0.878f, 1.0f);
static const FLinearColor TextMuted(0.627f, 0.627f, 0.627f, 0.7f);
static const FLinearColor ButtonBG(0.651f, 0.545f, 0.298f, 1.0f);
static const FLinearColor ButtonHover(0.718f, 0.612f, 0.369f, 1.0f);
static const FLinearColor ButtonDark(0.047f, 0.055f, 0.071f, 1.0f);
static const FLinearColor DangerRed(0.820f, 0.259f, 0.259f, 1.0f);
static const FLinearColor DangerHover(0.878f, 0.325f, 0.325f, 1.0f);
static const FLinearColor SuccessGreen(0.298f, 0.686f, 0.314f, 1.0f);
static const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
} // namespace FPMEscColors

// ===========================================================================
//  NativeConstruct
// ===========================================================================

void UFPMEscMenuWidget::NativeConstruct() {
  Super::NativeConstruct();
  BuildUI();

  if (ResumeButton) {
    ResumeButton->OnClicked.AddDynamic(this,
                                       &UFPMEscMenuWidget::OnResumeClicked);
  }
  if (LogoutButton) {
    LogoutButton->OnClicked.AddDynamic(this,
                                       &UFPMEscMenuWidget::OnLogoutClicked);
  }

  SetStatusMessage(TEXT(""), false);
  UE_LOG(LogFPMEscMenu, Log, TEXT("FPM ESC Menu: Widget constructed."));
}

// ===========================================================================
//  NativeTick — breathing glow on title + backdrop pulse
// ===========================================================================

void UFPMEscMenuWidget::NativeTick(const FGeometry &MyGeometry,
                                   float InDeltaTime) {
  Super::NativeTick(MyGeometry, InDeltaTime);
  AnimTime += InDeltaTime;

  // Gentle breathing glow on the title (same pattern as login screen)
  if (TitleText) {
    const float GlowAlpha = FMath::Lerp(
        0.10f, 0.50f, (FMath::Sin(AnimTime * (TWO_PI / 3.5f)) + 1.0f) * 0.5f);
    TitleText->SetShadowColorAndOpacity(
        FLinearColor(0.773f, 0.627f, 0.349f, GlowAlpha));
  }

  // Separator subtle shimmer
  if (SeparatorLine) {
    const float Sep = FMath::Lerp(
        0.2f, 0.55f, (FMath::Sin(AnimTime * (TWO_PI / 2.0f)) + 1.0f) * 0.5f);
    SeparatorLine->SetColorAndOpacity(
        FLinearColor(FPMEscColors::GoldPrimary.R, FPMEscColors::GoldPrimary.G,
                     FPMEscColors::GoldPrimary.B, Sep));
  }

  // Countdown to disconnect after saving
  if (bCountingDown) {
    LogoutCountdown -= InDeltaTime;
    if (LogoutCountdown <= 0.0f) {
      bCountingDown = false;
      // Now actually trigger the logout on the PlayerController
      if (AFPMPlayerController *PC =
              Cast<AFPMPlayerController>(GetOwningPlayer())) {
        PC->ExecuteLogout();
      }
    }
  }
}

// ===========================================================================
//  BuildUI — Programmatic widget construction
// ===========================================================================

void UFPMEscMenuWidget::BuildUI() {
  UCanvasPanel *RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
  if (!RootCanvas) {
    UE_LOG(LogFPMEscMenu, Warning,
           TEXT("FPM ESC Menu: Root is not a CanvasPanel."));
    return;
  }

  // =========================================================================
  // 1. FULL-SCREEN SEMI-TRANSPARENT BACKDROP
  //    Darkens the game world so the panel reads clearly.
  //    Slightly darker than a standard modal overlay because the game
  //    keeps running behind it.
  // =========================================================================
  BackdropImage = NewObject<UImage>(this);
  BackdropImage->SetColorAndOpacity(FPMEscColors::Backdrop);
  BackdropImage->SetBrushTintColor(FSlateColor(FPMEscColors::Backdrop));

  UCanvasPanelSlot *BdSlot = RootCanvas->AddChildToCanvas(BackdropImage);
  if (BdSlot) {
    BdSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    BdSlot->SetOffsets(FMargin(0.f));
    BdSlot->SetZOrder(0);
  }

  // =========================================================================
  // 2. CENTER OVERLAY — houses the main panel
  // =========================================================================
  UOverlay *CenterOverlay = NewObject<UOverlay>(this);
  UCanvasPanelSlot *COSlot = RootCanvas->AddChildToCanvas(CenterOverlay);
  if (COSlot) {
    COSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    COSlot->SetOffsets(FMargin(0.f));
    COSlot->SetZOrder(1);
  }

  // =========================================================================
  // 3. MAIN PANEL (glassmorphism card with subtle gold accent border)
  // =========================================================================
  UBorder *PanelBorder = NewObject<UBorder>(this);
  PanelBorder->SetBrushColor(FPMEscColors::PanelBG);
  PanelBorder->SetPadding(FMargin(52.f, 44.f));

  USizeBox *PanelSizeBox = NewObject<USizeBox>(this);
  PanelSizeBox->SetWidthOverride(400.f);

  UOverlaySlot *PanelOverlaySlot =
      CenterOverlay->AddChildToOverlay(PanelSizeBox);
  if (PanelOverlaySlot) {
    PanelOverlaySlot->SetHorizontalAlignment(HAlign_Center);
    PanelOverlaySlot->SetVerticalAlignment(VAlign_Center);
  }
  PanelSizeBox->AddChild(PanelBorder);

  UVerticalBox *ContentVBox = NewObject<UVerticalBox>(this);
  PanelBorder->AddChild(ContentVBox);

  // =========================================================================
  // 4. HEADER — Title + subtitle
  // =========================================================================

  // Decorative top ornament (thin gold line)
  UImage *TopLine = NewObject<UImage>(this);
  TopLine->SetColorAndOpacity(FPMEscColors::GoldDim);
  UVerticalBoxSlot *TopLineSlot = ContentVBox->AddChildToVerticalBox(TopLine);
  if (TopLineSlot) {
    TopLineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    TopLineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    // Fixed height via SizeBox trick
  }
  // Wrap TopLine in a SizeBox to set its height
  // (UImage doesn't have a direct height property easily, done via padding)

  // Title
  TitleText = NewObject<UTextBlock>(this);
  TitleText->SetText(FText::FromString(TEXT("FALDORAN PRIME")));
  {
    FSlateFontInfo TitleFont = TitleText->GetFont();
    TitleFont.Size = 32;
    TitleText->SetFont(TitleFont);
  }
  TitleText->SetColorAndOpacity(FSlateColor(FPMEscColors::White));
  TitleText->SetJustification(ETextJustify::Center);
  TitleText->SetShadowOffset(FVector2D(0.f, 2.f));
  TitleText->SetShadowColorAndOpacity(
      FLinearColor(0.773f, 0.627f, 0.349f, 0.4f));

  UVerticalBoxSlot *TitleSlot = ContentVBox->AddChildToVerticalBox(TitleText);
  if (TitleSlot) {
    TitleSlot->SetHorizontalAlignment(HAlign_Center);
    TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
  }

  // Subtitle — "IN WORLD" session status
  SubtitleText = NewObject<UTextBlock>(this);
  SubtitleText->SetText(FText::FromString(TEXT("SESSION ACTIVE")));
  {
    FSlateFontInfo SubFont = SubtitleText->GetFont();
    SubFont.Size = 10;
    SubtitleText->SetFont(SubFont);
  }
  SubtitleText->SetColorAndOpacity(FSlateColor(FPMEscColors::GoldPrimary));
  SubtitleText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *SubSlot = ContentVBox->AddChildToVerticalBox(SubtitleText);
  if (SubSlot) {
    SubSlot->SetHorizontalAlignment(HAlign_Center);
    SubSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
  }

  // Gold separator line
  SeparatorLine = NewObject<UImage>(this);
  SeparatorLine->SetColorAndOpacity(FPMEscColors::GoldDim);

  USizeBox *SepSizeBox = NewObject<USizeBox>(this);
  SepSizeBox->SetHeightOverride(1.f);
  SepSizeBox->AddChild(SeparatorLine);

  UVerticalBoxSlot *SepSlot = ContentVBox->AddChildToVerticalBox(SepSizeBox);
  if (SepSlot) {
    SepSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
    SepSlot->SetHorizontalAlignment(HAlign_Fill);
  }

  // =========================================================================
  // 5. BUTTONS
  // =========================================================================

  // --- Resume ---
  ResumeButton = MakePrimaryButton(TEXT("RESUME"), ResumeButtonText);
  UVerticalBoxSlot *ResSlot = ContentVBox->AddChildToVerticalBox(ResumeButton);
  if (ResSlot) {
    ResSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    ResSlot->SetHorizontalAlignment(HAlign_Fill);
  }

  // --- Logout & Save ---
  LogoutButton = MakeSecondaryButton(TEXT("LOGOUT  &  SAVE"), LogoutButtonText);
  {
    // Override the secondary ghost style with a deep-red danger tone
    FButtonStyle BtnStyle = LogoutButton->GetStyle();
    BtnStyle.Normal.TintColor =
        FSlateColor(FLinearColor(0.3f, 0.04f, 0.04f, 0.85f));
    BtnStyle.Hovered.TintColor =
        FSlateColor(FLinearColor(0.45f, 0.07f, 0.07f, 0.9f));
    BtnStyle.Pressed.TintColor =
        FSlateColor(FLinearColor(0.55f, 0.10f, 0.10f, 1.0f));
    LogoutButton->SetStyle(BtnStyle);
  }
  if (LogoutButtonText) {
    LogoutButtonText->SetColorAndOpacity(
        FSlateColor(FLinearColor(0.95f, 0.70f, 0.70f, 1.0f)));
  }

  UVerticalBoxSlot *LogSlot = ContentVBox->AddChildToVerticalBox(LogoutButton);
  if (LogSlot) {
    LogSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    LogSlot->SetHorizontalAlignment(HAlign_Fill);
  }

  // =========================================================================
  // 6. STATUS TEXT (feedback during save + logout)
  // =========================================================================
  StatusText = NewObject<UTextBlock>(this);
  StatusText->SetText(FText::GetEmpty());
  {
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 10;
    StatusText->SetFont(StatusFont);
  }
  StatusText->SetColorAndOpacity(FSlateColor(FPMEscColors::TextMuted));
  StatusText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *StatSlot = ContentVBox->AddChildToVerticalBox(StatusText);
  if (StatSlot) {
    StatSlot->SetHorizontalAlignment(HAlign_Center);
  }

  // =========================================================================
  // 7. FOOTER — keyboard hint
  // =========================================================================
  USpacer *FooterSp = NewObject<USpacer>(this);
  FooterSp->SetSize(FVector2D(0.f, 12.f));
  ContentVBox->AddChildToVerticalBox(FooterSp);

  UTextBlock *EscHint = NewObject<UTextBlock>(this);
  EscHint->SetText(FText::FromString(TEXT("[ESC] to resume")));
  {
    FSlateFontInfo HintFont = EscHint->GetFont();
    HintFont.Size = 9;
    EscHint->SetFont(HintFont);
  }
  EscHint->SetColorAndOpacity(
      FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 0.5f)));
  EscHint->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *HintSlot = ContentVBox->AddChildToVerticalBox(EscHint);
  if (HintSlot) {
    HintSlot->SetHorizontalAlignment(HAlign_Center);
  }
}

// ===========================================================================
//  Helpers
// ===========================================================================

UTextBlock *UFPMEscMenuWidget::MakeLabel(UObject *Outer, const FString &Text,
                                         int32 Size) {
  UTextBlock *Lbl = NewObject<UTextBlock>(Outer);
  Lbl->SetText(FText::FromString(Text));
  FSlateFontInfo F = Lbl->GetFont();
  F.Size = Size;
  Lbl->SetFont(F);
  Lbl->SetColorAndOpacity(FSlateColor(FPMEscColors::GoldPrimary));
  return Lbl;
}

UButton *
UFPMEscMenuWidget::MakePrimaryButton(const FString &Label,
                                     TObjectPtr<UTextBlock> &OutTextRef) {
  UButton *Btn = NewObject<UButton>(this);
  {
    FButtonStyle S = Btn->GetStyle();
    S.Normal.TintColor = FSlateColor(FPMEscColors::ButtonBG);
    S.Hovered.TintColor = FSlateColor(FPMEscColors::ButtonHover);
    S.Pressed.TintColor = FSlateColor(FPMEscColors::GoldPrimary);
    S.NormalPadding = FMargin(0.f, 14.f);
    S.PressedPadding = FMargin(0.f, 14.f);
    Btn->SetStyle(S);
  }

  UTextBlock *Txt = NewObject<UTextBlock>(this);
  Txt->SetText(FText::FromString(Label));
  {
    FSlateFontInfo F = Txt->GetFont();
    F.Size = 14;
    Txt->SetFont(F);
  }
  Txt->SetColorAndOpacity(FSlateColor(FPMEscColors::ButtonDark));
  Txt->SetJustification(ETextJustify::Center);
  Btn->AddChild(Txt);
  OutTextRef = Txt;
  return Btn;
}

UButton *
UFPMEscMenuWidget::MakeSecondaryButton(const FString &Label,
                                       TObjectPtr<UTextBlock> &OutTextRef) {
  UButton *Btn = NewObject<UButton>(this);
  {
    FButtonStyle S = Btn->GetStyle();
    S.Normal.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.01f));
    S.Hovered.TintColor =
        FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.08f));
    S.Pressed.TintColor =
        FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.15f));
    S.NormalPadding = FMargin(0.f, 12.f);
    S.PressedPadding = FMargin(0.f, 12.f);
    Btn->SetStyle(S);
  }

  UTextBlock *Txt = NewObject<UTextBlock>(this);
  Txt->SetText(FText::FromString(Label));
  {
    FSlateFontInfo F = Txt->GetFont();
    F.Size = 13;
    Txt->SetFont(F);
  }
  Txt->SetColorAndOpacity(FSlateColor(FPMEscColors::GoldPrimary));
  Txt->SetJustification(ETextJustify::Center);
  Btn->AddChild(Txt);
  OutTextRef = Txt;
  return Btn;
}

// ===========================================================================
//  SetStatusMessage
// ===========================================================================

void UFPMEscMenuWidget::SetStatusMessage(const FString &Message,
                                         bool bIsError) {
  if (!StatusText)
    return;
  StatusText->SetText(FText::FromString(Message));
  const FLinearColor Col =
      bIsError ? FLinearColor::Red
               : (Message.StartsWith(TEXT("\u2713"))
                      ? FPMEscColors::SuccessGreen // "✓" = green
                      : FPMEscColors::TextMuted);  // default = muted
  StatusText->SetColorAndOpacity(FSlateColor(Col));
}

// ===========================================================================
//  Button Handlers
// ===========================================================================

void UFPMEscMenuWidget::OnResumeClicked() {
  if (AFPMPlayerController *PC =
          Cast<AFPMPlayerController>(GetOwningPlayer())) {
    PC->HideEscMenu();
  }
}

void UFPMEscMenuWidget::OnLogoutClicked() {
  if (bLogoutPending)
    return; // prevent double-click

  bLogoutPending = true;

  // Disable both buttons visually to prevent spam
  if (ResumeButton)
    ResumeButton->SetIsEnabled(false);
  if (LogoutButton)
    LogoutButton->SetIsEnabled(false);

  SetStatusMessage(TEXT("Saving position\u2026"), false);

  // Ask the PlayerController to fire the save RPC.
  // The server will write spawn_x/y/z to PostgreSQL.
  // When save completes the server confirms via ClientSaveComplete —
  // but we also set a local countdown as a fallback so UI doesn't hang.
  if (AFPMPlayerController *PC =
          Cast<AFPMPlayerController>(GetOwningPlayer())) {
    PC->RequestSaveAndLogout();
  }

  // The PlayerController will call OnSaveComplete() when the server ACKs.
  // As a safety net, start a 2.5-second countdown regardless.
  bCountingDown = true;
  LogoutCountdown = 2.5f;

  UE_LOG(LogFPMEscMenu, Log,
         TEXT("FPM ESC Menu: Logout requested — save in progress."));
}
