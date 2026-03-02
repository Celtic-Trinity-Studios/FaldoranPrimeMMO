// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMLoginWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogFPMLoginWidget, Log, All);

// ===================================================================
// Color Palette
// ===================================================================
namespace FPMLoginColors {
static const FLinearColor DeepBG(0.039f, 0.047f, 0.063f, 1.0f); // #0a0c10
static const FLinearColor PanelBG(0.059f, 0.078f, 0.118f,
                                  0.7f); // rgba(15,20,30,0.7)
static const FLinearColor GoldPrimary(0.773f, 0.627f, 0.349f, 1.0f); // #c5a059
static const FLinearColor GoldLight(0.933f, 0.804f, 0.553f, 1.0f);   // #eecd8d
static const FLinearColor GoldDim(0.773f, 0.627f, 0.349f, 0.4f);  // gold at 40%
static const FLinearColor TextMain(0.878f, 0.878f, 0.878f, 1.0f); // #e0e0e0
static const FLinearColor TextMuted(0.627f, 0.627f, 0.627f, 0.7f); // #a0a0a0
static const FLinearColor InputBG(0.0f, 0.0f, 0.0f, 0.4f);
static const FLinearColor InputBorder(1.0f, 1.0f, 1.0f, 0.05f);
static const FLinearColor GoldBorder(0.773f, 0.627f, 0.349f, 0.4f);
static const FLinearColor PanelBorder(1.0f, 1.0f, 1.0f, 0.08f);
static const FLinearColor ButtonBG(0.651f, 0.545f, 0.298f, 1.0f); // #a68b4c
static const FLinearColor ButtonHover(0.718f, 0.612f, 0.369f,
                                      1.0f); // lighter gold
static const FLinearColor ButtonText(0.047f, 0.055f, 0.071f,
                                     1.0f); // near-black
static const FLinearColor SuccessGreen(0.298f, 0.686f, 0.314f, 1.0f);
static const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
} // namespace FPMLoginColors

// ===================================================================
// NativeConstruct
// ===================================================================

void UFPMLoginWidget::NativeConstruct() {
  Super::NativeConstruct();

  BuildUI();

  // Bind button clicks
  if (LoginButton) {
    LoginButton->OnClicked.AddDynamic(this, &UFPMLoginWidget::OnLoginClicked);
  }
  if (CreateAccountButton) {
    CreateAccountButton->OnClicked.AddDynamic(
        this, &UFPMLoginWidget::OnCreateAccountClicked);
  }

  // Clear any placeholder text
  SetResultMessage(TEXT(""), false);

  UE_LOG(LogFPMLoginWidget, Log,
         TEXT("FPM Login: Widget constructed. BackgroundTexture=%s"),
         BackgroundTexture ? *BackgroundTexture->GetName()
                           : TEXT("none (solid)"));
}

// ===================================================================
// NativeTick — breathing glow + shimmer sweep
// ===================================================================

void UFPMLoginWidget::NativeTick(const FGeometry &MyGeometry,
                                 float InDeltaTime) {
  Super::NativeTick(MyGeometry, InDeltaTime);

  AnimTime += InDeltaTime;

  // ---- 1. Title breathing glow -----------------------------------
  // Sine wave in [0,1] with a 3-second period. Modulates the gold shadow
  // opacity from 0.15 → 0.55, giving the title a gentle "pulse" effect.
  if (TitleText) {
    const float GlowAlpha = FMath::Lerp(
        0.15f, 0.55f, (FMath::Sin(AnimTime * (TWO_PI / 3.0f)) + 1.0f) * 0.5f);
    TitleText->SetShadowColorAndOpacity(
        FLinearColor(0.773f, 0.627f, 0.349f, GlowAlpha));
  }

  // ---- 2. Shimmer sweep ------------------------------------------
  // ShimmerPhase goes 0 → 1 over ShimmerPeriodSeconds, then wraps.
  ShimmerPhase = FMath::Frac(AnimTime / ShimmerPeriodSeconds);

  if (ShimmerLine && ShimmerSlot) {
    // Panel is centered horizontally. Calculate its left edge in viewport
    // space. We use the cached width; the panel starts at roughly
    // (ViewportWidth/2 - CachedPanelWidth/2) from the left edge.
    const FVector2D ViewportSize = MyGeometry.GetLocalSize();
    const float PanelLeft = (ViewportSize.X - CachedPanelWidth) * 0.5f;
    const float ShimmerX = PanelLeft + ShimmerPhase * CachedPanelWidth;

    // Position shimmer line at the computed X
    FMargin Offsets = ShimmerSlot->GetOffsets();
    Offsets.Left = ShimmerX;
    ShimmerSlot->SetOffsets(Offsets);

    // Fade in at start and out at end of sweep for a smooth look
    float ShimmerAlpha;
    if (ShimmerPhase < 0.1f) {
      ShimmerAlpha = ShimmerPhase / 0.1f;
    } else if (ShimmerPhase > 0.85f) {
      ShimmerAlpha = (1.0f - ShimmerPhase) / 0.15f;
    } else {
      ShimmerAlpha = 1.0f;
    }
    ShimmerLine->SetColorAndOpacity(
        FLinearColor(0.933f, 0.804f, 0.553f, ShimmerAlpha * 0.25f));
  }
}

// ===================================================================
// BuildUI — programmatic UI construction
// ===================================================================

void UFPMLoginWidget::BuildUI() {
  // Get the root widget slot — we expect the Blueprint root to be a Canvas
  UCanvasPanel *RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
  if (!RootCanvas) {
    UE_LOG(LogFPMLoginWidget, Warning,
           TEXT("FPM Login: Root widget is not a CanvasPanel! Cannot build UI "
                "programmatically."));
    return;
  }

  // ==============================================================
  // 1. FULL-SCREEN BACKGROUND IMAGE
  // Fully opaque by default — hides everything behind the widget.
  // If BackgroundTexture is assigned (EditAnywhere in Details or set
  // by PlayerController before construction), it fills the screen.
  // Leave null for the solid deep-navy fallback colour.
  // ==============================================================
  BackgroundImage = NewObject<UImage>(this);

  if (BackgroundTexture) {
    // Texture path: fill the screen with the assigned image
    FSlateBrush Brush;
    Brush.SetResourceObject(BackgroundTexture);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Brush.Mirroring = ESlateBrushMirrorType::NoMirror;
    BackgroundImage->SetBrush(Brush);
    BackgroundImage->SetColorAndOpacity(FLinearColor::White); // no tint
  } else {
    // Fallback: solid deep-navy, fully opaque
    static const FLinearColor SolidBG(0.039f, 0.047f, 0.063f, 1.0f); // #0a0c10
    BackgroundImage->SetColorAndOpacity(SolidBG);
    BackgroundImage->SetBrushTintColor(FSlateColor(SolidBG));
  }

  UCanvasPanelSlot *BgSlot = RootCanvas->AddChildToCanvas(BackgroundImage);
  if (BgSlot) {
    BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // full stretch
    BgSlot->SetOffsets(FMargin(0.0f));
    BgSlot->SetZOrder(0);
  }

  // ==============================================================
  // 2. CENTER OVERLAY (for centered content layout)
  // ==============================================================
  UOverlay *CenterOverlay = NewObject<UOverlay>(this);
  UCanvasPanelSlot *OverlaySlot = RootCanvas->AddChildToCanvas(CenterOverlay);
  if (OverlaySlot) {
    OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    OverlaySlot->SetOffsets(FMargin(0.0f));
    OverlaySlot->SetZOrder(1);
  }

  // ==============================================================
  // 3. SHIMMER LINE (thin gold sweep animated in NativeTick)
  // Positioned by NativeTick via ShimmerSlot. Zero width initially.
  // ==============================================================
  ShimmerLine = NewObject<UImage>(this);
  ShimmerLine->SetColorAndOpacity(FLinearColor(0.933f, 0.804f, 0.553f, 0.0f));

  ShimmerSlot = RootCanvas->AddChildToCanvas(ShimmerLine);
  if (ShimmerSlot) {
    // Anchored top-left; NativeTick updates Left offset each frame
    ShimmerSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
    // Width = 3px wide shimmer line, Height = full viewport height
    ShimmerSlot->SetOffsets(FMargin(0.0f, -400.0f, 3.0f, 800.0f));
    ShimmerSlot->SetZOrder(10);
  }

  // Main vertical content container
  UVerticalBox *MainVBox = NewObject<UVerticalBox>(this);
  UOverlaySlot *VBoxOverlaySlot = CenterOverlay->AddChildToOverlay(MainVBox);
  if (VBoxOverlaySlot) {
    VBoxOverlaySlot->SetHorizontalAlignment(HAlign_Center);
    VBoxOverlaySlot->SetVerticalAlignment(VAlign_Center);
  }

  // ==============================================================
  // 3. TITLE SECTION
  // ==============================================================

  // Title: "FALDORAN PRIME"
  TitleText = NewObject<UTextBlock>(this);
  TitleText->SetText(FText::FromString(TEXT("FALDORAN PRIME")));
  {
    FSlateFontInfo TitleFont = TitleText->GetFont();
    TitleFont.Size = 48;
    TitleFont.OutlineSettings.OutlineSize = 0;
    TitleText->SetFont(TitleFont);
  }
  TitleText->SetColorAndOpacity(FSlateColor(FPMLoginColors::White));
  TitleText->SetJustification(ETextJustify::Center);
  TitleText->SetShadowColorAndOpacity(
      FLinearColor(0.773f, 0.627f, 0.349f, 0.4f));
  TitleText->SetShadowOffset(FVector2D(0.0f, 2.0f));

  UVerticalBoxSlot *TitleSlot = MainVBox->AddChildToVerticalBox(TitleText);
  if (TitleSlot) {
    TitleSlot->SetHorizontalAlignment(HAlign_Center);
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
  }

  // Subtitle: "The Civilization Evolution"
  SubtitleText = NewObject<UTextBlock>(this);
  SubtitleText->SetText(FText::FromString(TEXT("THE CIVILIZATION EVOLUTION")));
  {
    FSlateFontInfo SubFont = SubtitleText->GetFont();
    SubFont.Size = 11;
    SubtitleText->SetFont(SubFont);
  }
  SubtitleText->SetColorAndOpacity(FSlateColor(FPMLoginColors::TextMuted));
  SubtitleText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *SubSlot = MainVBox->AddChildToVerticalBox(SubtitleText);
  if (SubSlot) {
    SubSlot->SetHorizontalAlignment(HAlign_Center);
    SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 30.0f));
  }

  // ==============================================================
  // 4. LOGIN PANEL (dark semi-transparent card with gold border)
  // ==============================================================
  UBorder *PanelBorder = NewObject<UBorder>(this);
  PanelBorder->SetBrushColor(FPMLoginColors::PanelBG);
  PanelBorder->SetPadding(FMargin(48.0f, 40.0f));
  // Use the content color to tint the border outline
  PanelBorder->SetContentColorAndOpacity(FPMLoginColors::White);

  USizeBox *PanelSizeBox = NewObject<USizeBox>(this);
  PanelSizeBox->SetWidthOverride(440.0f);

  UVerticalBoxSlot *PanelSlot = MainVBox->AddChildToVerticalBox(PanelSizeBox);
  if (PanelSlot) {
    PanelSlot->SetHorizontalAlignment(HAlign_Center);
  }
  PanelSizeBox->AddChild(PanelBorder);

  // Inner vertical box for form elements
  UVerticalBox *FormVBox = NewObject<UVerticalBox>(this);
  PanelBorder->AddChild(FormVBox);

  // --- Server Status ---
  StatusText = NewObject<UTextBlock>(this);
  StatusText->SetText(FText::FromString(TEXT("\u25CF  SERVER ONLINE")));
  {
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 10;
    StatusText->SetFont(StatusFont);
  }
  StatusText->SetColorAndOpacity(FSlateColor(FPMLoginColors::SuccessGreen));
  StatusText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *StatusSlot = FormVBox->AddChildToVerticalBox(StatusText);
  if (StatusSlot) {
    StatusSlot->SetHorizontalAlignment(HAlign_Center);
    StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
  }

  // --- Username Label ---
  UTextBlock *UserLabel = NewObject<UTextBlock>(this);
  UserLabel->SetText(FText::FromString(TEXT("ACCOUNT IDENTITY")));
  {
    FSlateFontInfo LabelFont = UserLabel->GetFont();
    LabelFont.Size = 10;
    UserLabel->SetFont(LabelFont);
  }
  UserLabel->SetColorAndOpacity(FSlateColor(FPMLoginColors::GoldPrimary));

  UVerticalBoxSlot *UserLabelSlot = FormVBox->AddChildToVerticalBox(UserLabel);
  if (UserLabelSlot) {
    UserLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
  }

  // --- Username Input ---
  UsernameInput = NewObject<UEditableTextBox>(this);
  UsernameInput->SetHintText(FText::FromString(TEXT("Username")));
  {
    FEditableTextBoxStyle InputStyle = UsernameInput->WidgetStyle;

    // Background brush — dark transparent
    InputStyle.BackgroundImageNormal.TintColor =
        FSlateColor(FPMLoginColors::InputBG);
    InputStyle.BackgroundImageHovered.TintColor =
        FSlateColor(FLinearColor(0.059f, 0.078f, 0.118f, 0.6f));
    InputStyle.BackgroundImageFocused.TintColor =
        FSlateColor(FLinearColor(0.059f, 0.078f, 0.118f, 0.8f));

    // Text color
    InputStyle.ForegroundColor = FSlateColor(FPMLoginColors::White);

    // Padding inside the input
    InputStyle.Padding = FMargin(12.0f, 10.0f);

    UsernameInput->WidgetStyle = InputStyle;
  }

  UVerticalBoxSlot *UserInputSlot =
      FormVBox->AddChildToVerticalBox(UsernameInput);
  if (UserInputSlot) {
    UserInputSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
  }

  // --- Password Label ---
  UTextBlock *PassLabel = NewObject<UTextBlock>(this);
  PassLabel->SetText(FText::FromString(TEXT("ACCESS CIPHER")));
  {
    FSlateFontInfo LabelFont = PassLabel->GetFont();
    LabelFont.Size = 10;
    PassLabel->SetFont(LabelFont);
  }
  PassLabel->SetColorAndOpacity(FSlateColor(FPMLoginColors::GoldPrimary));

  UVerticalBoxSlot *PassLabelSlot = FormVBox->AddChildToVerticalBox(PassLabel);
  if (PassLabelSlot) {
    PassLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
  }

  // --- Password Input ---
  PasswordInput = NewObject<UEditableTextBox>(this);
  PasswordInput->SetHintText(FText::FromString(
      TEXT("\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022")));
  PasswordInput->SetIsPassword(true);
  {
    FEditableTextBoxStyle InputStyle = PasswordInput->WidgetStyle;

    InputStyle.BackgroundImageNormal.TintColor =
        FSlateColor(FPMLoginColors::InputBG);
    InputStyle.BackgroundImageHovered.TintColor =
        FSlateColor(FLinearColor(0.059f, 0.078f, 0.118f, 0.6f));
    InputStyle.BackgroundImageFocused.TintColor =
        FSlateColor(FLinearColor(0.059f, 0.078f, 0.118f, 0.8f));
    InputStyle.ForegroundColor = FSlateColor(FPMLoginColors::White);
    InputStyle.Padding = FMargin(12.0f, 10.0f);

    PasswordInput->WidgetStyle = InputStyle;
  }

  UVerticalBoxSlot *PassInputSlot =
      FormVBox->AddChildToVerticalBox(PasswordInput);
  if (PassInputSlot) {
    PassInputSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
  }

  // --- Login Button ---
  LoginButton = NewObject<UButton>(this);
  {
    FButtonStyle BtnStyle = LoginButton->GetStyle();

    // Normal state — Gold gradient feel (solid tint in UMG)
    BtnStyle.Normal.TintColor = FSlateColor(FPMLoginColors::ButtonBG);
    BtnStyle.Hovered.TintColor = FSlateColor(FPMLoginColors::ButtonHover);
    BtnStyle.Pressed.TintColor = FSlateColor(FPMLoginColors::GoldPrimary);

    LoginButton->SetStyle(BtnStyle);
  }

  LoginButtonText = NewObject<UTextBlock>(this);
  LoginButtonText->SetText(FText::FromString(TEXT("CONNECT TO WORLD")));
  {
    FSlateFontInfo BtnFont = LoginButtonText->GetFont();
    BtnFont.Size = 14;
    LoginButtonText->SetFont(BtnFont);
  }
  LoginButtonText->SetColorAndOpacity(FSlateColor(FPMLoginColors::ButtonText));
  LoginButtonText->SetJustification(ETextJustify::Center);
  LoginButton->AddChild(LoginButtonText);

  UVerticalBoxSlot *LoginBtnSlot = FormVBox->AddChildToVerticalBox(LoginButton);
  if (LoginBtnSlot) {
    LoginBtnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
  }

  // --- Create Account Button ---
  CreateAccountButton = NewObject<UButton>(this);
  {
    FButtonStyle BtnStyle = CreateAccountButton->GetStyle();

    // Transparent with gold border feel
    BtnStyle.Normal.TintColor =
        FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.01f));
    BtnStyle.Hovered.TintColor =
        FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.08f));
    BtnStyle.Pressed.TintColor =
        FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.15f));

    CreateAccountButton->SetStyle(BtnStyle);
  }

  CreateAccountButtonText = NewObject<UTextBlock>(this);
  CreateAccountButtonText->SetText(
      FText::FromString(TEXT("CREATE NEW IDENTITY")));
  {
    FSlateFontInfo BtnFont = CreateAccountButtonText->GetFont();
    BtnFont.Size = 12;
    CreateAccountButtonText->SetFont(BtnFont);
  }
  CreateAccountButtonText->SetColorAndOpacity(
      FSlateColor(FPMLoginColors::GoldPrimary));
  CreateAccountButtonText->SetJustification(ETextJustify::Center);
  CreateAccountButton->AddChild(CreateAccountButtonText);

  UVerticalBoxSlot *CreateBtnSlot =
      FormVBox->AddChildToVerticalBox(CreateAccountButton);
  if (CreateBtnSlot) {
    CreateBtnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
  }

  // --- Result Text ---
  ResultText = NewObject<UTextBlock>(this);
  ResultText->SetText(FText::GetEmpty());
  {
    FSlateFontInfo ResFont = ResultText->GetFont();
    ResFont.Size = 11;
    ResultText->SetFont(ResFont);
  }
  ResultText->SetColorAndOpacity(FSlateColor(FPMLoginColors::White));
  ResultText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *ResultSlot = FormVBox->AddChildToVerticalBox(ResultText);
  if (ResultSlot) {
    ResultSlot->SetHorizontalAlignment(HAlign_Center);
  }

  // ==============================================================
  // 5. FOOTER (Version + Copyright)
  // ==============================================================

  // Spacer to push footer down
  USpacer *FooterSpacer = NewObject<USpacer>(this);
  FooterSpacer->SetSize(FVector2D(0.0f, 30.0f));
  MainVBox->AddChildToVerticalBox(FooterSpacer);

  VersionText = NewObject<UTextBlock>(this);
  VersionText->SetText(
      FText::FromString(TEXT("VERSION 0.1.0-ALPHA-PROTOTYPE")));
  {
    FSlateFontInfo VerFont = VersionText->GetFont();
    VerFont.Size = 9;
    VersionText->SetFont(VerFont);
  }
  VersionText->SetColorAndOpacity(FSlateColor(FPMLoginColors::TextMuted));
  VersionText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *VerSlot = MainVBox->AddChildToVerticalBox(VersionText);
  if (VerSlot) {
    VerSlot->SetHorizontalAlignment(HAlign_Center);
    VerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
  }

  CopyrightText = NewObject<UTextBlock>(this);
  CopyrightText->SetText(FText::FromString(
      TEXT("\u00A9 2026 CELTIC TRINITY STUDIOS. ALL RIGHTS RESERVED.")));
  {
    FSlateFontInfo CopyFont = CopyrightText->GetFont();
    CopyFont.Size = 9;
    CopyrightText->SetFont(CopyFont);
  }
  CopyrightText->SetColorAndOpacity(FSlateColor(FPMLoginColors::TextMuted));
  CopyrightText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *CopySlot = MainVBox->AddChildToVerticalBox(CopyrightText);
  if (CopySlot) {
    CopySlot->SetHorizontalAlignment(HAlign_Center);
  }
}

// ===================================================================
// SetResultMessage
// ===================================================================

void UFPMLoginWidget::SetResultMessage(const FString &Message, bool bIsError) {
  if (!ResultText) {
    return;
  }

  ResultText->SetText(FText::FromString(Message));

  // Red for errors, white for success/info
  const FSlateColor Color = bIsError ? FSlateColor(FLinearColor::Red)
                                     : FSlateColor(FPMLoginColors::White);
  ResultText->SetColorAndOpacity(Color);
}

// ===================================================================
// Button Handlers
// ===================================================================

void UFPMLoginWidget::OnLoginClicked() {
  if (!UsernameInput || !PasswordInput) {
    return;
  }

  const FString Username = UsernameInput->GetText().ToString();
  const FString Password = PasswordInput->GetText().ToString();

  if (Username.IsEmpty() || Password.IsEmpty()) {
    SetResultMessage(TEXT("Please enter username and password."), true);
    return;
  }

  // Forward to the PlayerController which sends the Server RPC
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestLogin(Username, Password);
    SetResultMessage(TEXT("Establishing connection..."), false);
  }
}

void UFPMLoginWidget::OnCreateAccountClicked() {
  if (!UsernameInput || !PasswordInput) {
    return;
  }

  const FString Username = UsernameInput->GetText().ToString();
  const FString Password = PasswordInput->GetText().ToString();

  if (Username.IsEmpty() || Password.IsEmpty()) {
    SetResultMessage(TEXT("Please enter username and password."), true);
    return;
  }

  // Forward to the PlayerController which sends the Server RPC
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestCreateAccount(Username, Password);
    SetResultMessage(TEXT("Creating identity..."), false);
  }
}
