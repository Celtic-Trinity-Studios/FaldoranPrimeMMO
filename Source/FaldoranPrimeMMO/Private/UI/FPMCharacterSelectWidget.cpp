// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMCharacterSelectWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/FPMPlayerController.h"
#include "Styling/SlateColor.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterSelect, Log, All);

// ===================================================================
// Color Palette (same Glass & Gold theme as login screen)
// ===================================================================
namespace FPMSelectColors {
static const FLinearColor DeepBG(0.039f, 0.047f, 0.063f, 1.0f);
static const FLinearColor PanelBG(0.059f, 0.078f, 0.118f, 0.7f);
static const FLinearColor GoldPrimary(0.773f, 0.627f, 0.349f, 1.0f);
static const FLinearColor GoldLight(0.933f, 0.804f, 0.553f, 1.0f);
static const FLinearColor TextMain(0.878f, 0.878f, 0.878f, 1.0f);
static const FLinearColor TextMuted(0.627f, 0.627f, 0.627f, 0.7f);
static const FLinearColor ButtonBG(0.651f, 0.545f, 0.298f, 1.0f);
static const FLinearColor ButtonHover(0.718f, 0.612f, 0.369f, 1.0f);
static const FLinearColor ButtonText(0.047f, 0.055f, 0.071f, 1.0f);
static const FLinearColor EntryNormal(0.059f, 0.078f, 0.118f, 0.5f);
static const FLinearColor EntryHover(0.1f, 0.12f, 0.16f, 0.7f);
static const FLinearColor EntrySelected(0.773f, 0.627f, 0.349f, 0.2f);
static const FLinearColor White(1.0f, 1.0f, 1.0f, 1.0f);
static const FLinearColor SuccessGreen(0.298f, 0.686f, 0.314f, 1.0f);
static const FLinearColor TransparentBtn(0.0f, 0.0f, 0.0f, 0.01f);
static const FLinearColor GoldBtnHover(0.773f, 0.627f, 0.349f, 0.08f);
static const FLinearColor GoldBtnPress(0.773f, 0.627f, 0.349f, 0.15f);
} // namespace FPMSelectColors

// ===================================================================
// NativeConstruct
// ===================================================================

void UFPMCharacterSelectWidget::NativeConstruct() {
  Super::NativeConstruct();

  BuildUI();

  // Bind button clicks
  if (EnterWorldButton) {
    EnterWorldButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnEnterWorldClicked);
  }
  if (CreateNewButton) {
    CreateNewButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnCreateNewClicked);
  }
  if (BackButton) {
    BackButton->OnClicked.AddDynamic(this,
                                     &UFPMCharacterSelectWidget::OnBackClicked);
  }

  // Start with no selection
  SelectedIndex = -1;
  UpdateButtonStates();
  SetStatusMessage(TEXT("Loading characters..."), false);

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Widget constructed with Glass & Gold theme."));
}

// ===================================================================
// BuildUI — programmatic UI construction
// ===================================================================

void UFPMCharacterSelectWidget::BuildUI() {
  UCanvasPanel *RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
  if (!RootCanvas) {
    UE_LOG(LogFPMCharacterSelect, Warning,
           TEXT("FPM CharSelect: Root widget is not a CanvasPanel! Cannot "
                "build UI programmatically."));
    return;
  }

  // ==============================================================
  // 1. FULL-SCREEN OPAQUE BACKGROUND
  // ==============================================================
  BackgroundImage = NewObject<UImage>(this);
  BackgroundImage->SetColorAndOpacity(FPMSelectColors::DeepBG);
  BackgroundImage->SetBrushTintColor(FSlateColor(FPMSelectColors::DeepBG));

  UCanvasPanelSlot *BgSlot = RootCanvas->AddChildToCanvas(BackgroundImage);
  if (BgSlot) {
    BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BgSlot->SetOffsets(FMargin(0.0f));
  }

  // ==============================================================
  // 2. CENTER OVERLAY
  // ==============================================================
  UOverlay *CenterOverlay = NewObject<UOverlay>(this);
  UCanvasPanelSlot *OverlaySlot = RootCanvas->AddChildToCanvas(CenterOverlay);
  if (OverlaySlot) {
    OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    OverlaySlot->SetOffsets(FMargin(0.0f));
  }

  UVerticalBox *MainVBox = NewObject<UVerticalBox>(this);
  UOverlaySlot *VBoxOverlaySlot = CenterOverlay->AddChildToOverlay(MainVBox);
  if (VBoxOverlaySlot) {
    VBoxOverlaySlot->SetHorizontalAlignment(HAlign_Center);
    VBoxOverlaySlot->SetVerticalAlignment(VAlign_Center);
  }

  // ==============================================================
  // 3. TITLE SECTION
  // ==============================================================
  TitleText = NewObject<UTextBlock>(this);
  TitleText->SetText(FText::FromString(TEXT("FALDORAN PRIME")));
  {
    FSlateFontInfo TitleFont = TitleText->GetFont();
    TitleFont.Size = 42;
    TitleText->SetFont(TitleFont);
  }
  TitleText->SetColorAndOpacity(FSlateColor(FPMSelectColors::White));
  TitleText->SetJustification(ETextJustify::Center);
  TitleText->SetShadowColorAndOpacity(
      FLinearColor(0.773f, 0.627f, 0.349f, 0.4f));
  TitleText->SetShadowOffset(FVector2D(0.0f, 2.0f));

  UVerticalBoxSlot *TitleSlot = MainVBox->AddChildToVerticalBox(TitleText);
  if (TitleSlot) {
    TitleSlot->SetHorizontalAlignment(HAlign_Center);
    TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
  }

  // Subtitle
  SubtitleText = NewObject<UTextBlock>(this);
  SubtitleText->SetText(FText::FromString(TEXT("SELECT YOUR CHARACTER")));
  {
    FSlateFontInfo SubFont = SubtitleText->GetFont();
    SubFont.Size = 11;
    SubtitleText->SetFont(SubFont);
  }
  SubtitleText->SetColorAndOpacity(FSlateColor(FPMSelectColors::GoldPrimary));
  SubtitleText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *SubSlot = MainVBox->AddChildToVerticalBox(SubtitleText);
  if (SubSlot) {
    SubSlot->SetHorizontalAlignment(HAlign_Center);
    SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 30.0f));
  }

  // ==============================================================
  // 4. CHARACTER SELECT PANEL
  // ==============================================================
  UBorder *PanelBorder = NewObject<UBorder>(this);
  PanelBorder->SetBrushColor(FPMSelectColors::PanelBG);
  PanelBorder->SetPadding(FMargin(40.0f, 32.0f));
  PanelBorder->SetContentColorAndOpacity(FPMSelectColors::White);

  USizeBox *PanelSizeBox = NewObject<USizeBox>(this);
  PanelSizeBox->SetWidthOverride(500.0f);

  UVerticalBoxSlot *PanelSlot = MainVBox->AddChildToVerticalBox(PanelSizeBox);
  if (PanelSlot) {
    PanelSlot->SetHorizontalAlignment(HAlign_Center);
  }
  PanelSizeBox->AddChild(PanelBorder);

  UVerticalBox *FormVBox = NewObject<UVerticalBox>(this);
  PanelBorder->AddChild(FormVBox);

  // --- Status / Loading text ---
  StatusText = NewObject<UTextBlock>(this);
  StatusText->SetText(FText::FromString(TEXT("Loading characters...")));
  {
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 10;
    StatusText->SetFont(StatusFont);
  }
  StatusText->SetColorAndOpacity(FSlateColor(FPMSelectColors::TextMuted));
  StatusText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *StatusSlot = FormVBox->AddChildToVerticalBox(StatusText);
  if (StatusSlot) {
    StatusSlot->SetHorizontalAlignment(HAlign_Center);
    StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
  }

  // --- Character List Container ---
  // Label
  UTextBlock *ListLabel = NewObject<UTextBlock>(this);
  ListLabel->SetText(FText::FromString(TEXT("YOUR CHARACTERS")));
  {
    FSlateFontInfo LabelFont = ListLabel->GetFont();
    LabelFont.Size = 10;
    ListLabel->SetFont(LabelFont);
  }
  ListLabel->SetColorAndOpacity(FSlateColor(FPMSelectColors::GoldPrimary));

  UVerticalBoxSlot *ListLabelSlot = FormVBox->AddChildToVerticalBox(ListLabel);
  if (ListLabelSlot) {
    ListLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
  }

  // Character list vertical box (entries added dynamically)
  CharacterListBox = NewObject<UVerticalBox>(this);

  // Wrap in a size box to constrain height
  USizeBox *ListSizeBox = NewObject<USizeBox>(this);
  ListSizeBox->SetMaxDesiredHeight(200.0f);
  ListSizeBox->AddChild(CharacterListBox);

  UVerticalBoxSlot *ListSlot = FormVBox->AddChildToVerticalBox(ListSizeBox);
  if (ListSlot) {
    ListSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
  }

  // --- Selected Character indicator ---
  SelectedCharacterText = NewObject<UTextBlock>(this);
  SelectedCharacterText->SetText(
      FText::FromString(TEXT("No character selected")));
  {
    FSlateFontInfo SelFont = SelectedCharacterText->GetFont();
    SelFont.Size = 12;
    SelectedCharacterText->SetFont(SelFont);
  }
  SelectedCharacterText->SetColorAndOpacity(
      FSlateColor(FPMSelectColors::GoldLight));
  SelectedCharacterText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *SelSlot =
      FormVBox->AddChildToVerticalBox(SelectedCharacterText);
  if (SelSlot) {
    SelSlot->SetHorizontalAlignment(HAlign_Center);
    SelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
  }

  // --- Enter World Button (primary gold) ---
  EnterWorldButton = NewObject<UButton>(this);
  {
    FButtonStyle BtnStyle = EnterWorldButton->GetStyle();
    BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::ButtonBG);
    BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::ButtonHover);
    BtnStyle.Pressed.TintColor = FSlateColor(FPMSelectColors::GoldPrimary);
    BtnStyle.Disabled.TintColor =
        FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
    EnterWorldButton->SetStyle(BtnStyle);
  }

  EnterWorldButtonText = NewObject<UTextBlock>(this);
  EnterWorldButtonText->SetText(FText::FromString(TEXT("ENTER WORLD")));
  {
    FSlateFontInfo BtnFont = EnterWorldButtonText->GetFont();
    BtnFont.Size = 14;
    EnterWorldButtonText->SetFont(BtnFont);
  }
  EnterWorldButtonText->SetColorAndOpacity(
      FSlateColor(FPMSelectColors::ButtonText));
  EnterWorldButtonText->SetJustification(ETextJustify::Center);
  EnterWorldButton->AddChild(EnterWorldButtonText);

  UVerticalBoxSlot *EnterSlot =
      FormVBox->AddChildToVerticalBox(EnterWorldButton);
  if (EnterSlot) {
    EnterSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
  }

  // --- Create New Button (secondary, transparent with gold text) ---
  CreateNewButton = NewObject<UButton>(this);
  {
    FButtonStyle BtnStyle = CreateNewButton->GetStyle();
    BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::TransparentBtn);
    BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::GoldBtnHover);
    BtnStyle.Pressed.TintColor = FSlateColor(FPMSelectColors::GoldBtnPress);
    CreateNewButton->SetStyle(BtnStyle);
  }

  CreateNewButtonText = NewObject<UTextBlock>(this);
  CreateNewButtonText->SetText(FText::FromString(TEXT("CREATE NEW CHARACTER")));
  {
    FSlateFontInfo BtnFont = CreateNewButtonText->GetFont();
    BtnFont.Size = 12;
    CreateNewButtonText->SetFont(BtnFont);
  }
  CreateNewButtonText->SetColorAndOpacity(
      FSlateColor(FPMSelectColors::GoldPrimary));
  CreateNewButtonText->SetJustification(ETextJustify::Center);
  CreateNewButton->AddChild(CreateNewButtonText);

  UVerticalBoxSlot *CreateSlot =
      FormVBox->AddChildToVerticalBox(CreateNewButton);
  if (CreateSlot) {
    CreateSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
  }

  // --- Back Button (tertiary, muted) ---
  BackButton = NewObject<UButton>(this);
  {
    FButtonStyle BtnStyle = BackButton->GetStyle();
    BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::TransparentBtn);
    BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::GoldBtnHover);
    BtnStyle.Pressed.TintColor = FSlateColor(FPMSelectColors::GoldBtnPress);
    BackButton->SetStyle(BtnStyle);
  }

  BackButtonText = NewObject<UTextBlock>(this);
  BackButtonText->SetText(FText::FromString(TEXT("BACK TO LOGIN")));
  {
    FSlateFontInfo BtnFont = BackButtonText->GetFont();
    BtnFont.Size = 10;
    BackButtonText->SetFont(BtnFont);
  }
  BackButtonText->SetColorAndOpacity(FSlateColor(FPMSelectColors::TextMuted));
  BackButtonText->SetJustification(ETextJustify::Center);
  BackButton->AddChild(BackButtonText);

  UVerticalBoxSlot *BackSlot = FormVBox->AddChildToVerticalBox(BackButton);
  if (BackSlot) {
    BackSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
  }

  // ==============================================================
  // 5. FOOTER
  // ==============================================================
  USpacer *FooterSpacer = NewObject<USpacer>(this);
  FooterSpacer->SetSize(FVector2D(0.0f, 30.0f));
  MainVBox->AddChildToVerticalBox(FooterSpacer);

  FooterText = NewObject<UTextBlock>(this);
  FooterText->SetText(FText::FromString(
      TEXT("\u00A9 2026 CELTIC TRINITY STUDIOS. ALL RIGHTS RESERVED.")));
  {
    FSlateFontInfo FootFont = FooterText->GetFont();
    FootFont.Size = 9;
    FooterText->SetFont(FootFont);
  }
  FooterText->SetColorAndOpacity(FSlateColor(FPMSelectColors::TextMuted));
  FooterText->SetJustification(ETextJustify::Center);

  UVerticalBoxSlot *FootSlot = MainVBox->AddChildToVerticalBox(FooterText);
  if (FootSlot) {
    FootSlot->SetHorizontalAlignment(HAlign_Center);
  }
}

// ===================================================================
// PopulateCharacterList
// ===================================================================

void UFPMCharacterSelectWidget::PopulateCharacterList(
    const TArray<FFPMCharacterSummary> &Characters) {
  CachedCharacters = Characters;
  SelectedIndex = -1;
  CharacterEntryButtons.Empty();

  if (!CharacterListBox) {
    return;
  }

  CharacterListBox->ClearChildren();

  if (Characters.Num() == 0) {
    SetStatusMessage(TEXT("No characters found. Create one!"), false);
    UpdateButtonStates();
    return;
  }

  // Create a styled button for each character
  for (int32 i = 0; i < Characters.Num(); ++i) {
    const FFPMCharacterSummary &Summary = Characters[i];

    UButton *EntryButton = NewObject<UButton>(this);
    {
      FButtonStyle BtnStyle = EntryButton->GetStyle();
      BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::EntryNormal);
      BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::EntryHover);
      BtnStyle.Pressed.TintColor = FSlateColor(FPMSelectColors::EntrySelected);
      EntryButton->SetStyle(BtnStyle);
    }

    // Character name and last played info
    UVerticalBox *EntryContent = NewObject<UVerticalBox>(this);

    UTextBlock *NameText = NewObject<UTextBlock>(this);
    NameText->SetText(FText::FromString(Summary.CharacterName));
    {
      FSlateFontInfo NameFont = NameText->GetFont();
      NameFont.Size = 14;
      NameText->SetFont(NameFont);
    }
    NameText->SetColorAndOpacity(FSlateColor(FPMSelectColors::White));

    UVerticalBoxSlot *NameSlot = EntryContent->AddChildToVerticalBox(NameText);
    if (NameSlot) {
      NameSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 2.0f));
    }

    UTextBlock *InfoText = NewObject<UTextBlock>(this);
    InfoText->SetText(FText::FromString(
        FString::Printf(TEXT("Last played: %s"), *Summary.LastPlayed)));
    {
      FSlateFontInfo InfoFont = InfoText->GetFont();
      InfoFont.Size = 9;
      InfoText->SetFont(InfoFont);
    }
    InfoText->SetColorAndOpacity(FSlateColor(FPMSelectColors::TextMuted));

    UVerticalBoxSlot *InfoSlot = EntryContent->AddChildToVerticalBox(InfoText);
    if (InfoSlot) {
      InfoSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 8.0f));
    }

    EntryButton->AddChild(EntryContent);
    CharacterListBox->AddChild(EntryButton);

    // Store reference for selection styling
    CharacterEntryButtons.Add(EntryButton);

    // Bind click — we use a workaround since UE dynamic delegates
    // don't support per-button parameters. Each button calls
    // OnEnterWorldClicked, but we override to first auto-select
    // the character by finding the button index.
    // Instead, we use the simpler approach: auto-select first, let
    // the user click to select.

    UVerticalBoxSlot *EntrySlot = Cast<UVerticalBoxSlot>(EntryButton->Slot);
    if (EntrySlot) {
      EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }
  }

  // Auto-select first character
  if (Characters.Num() > 0) {
    OnCharacterSelected(0);
  }

  SetStatusMessage(TEXT(""), false);

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Populated %d characters."), Characters.Num());
}

// ===================================================================
// SetStatusMessage
// ===================================================================

void UFPMCharacterSelectWidget::SetStatusMessage(const FString &Message,
                                                 bool bIsError) {
  if (!StatusText) {
    return;
  }

  StatusText->SetText(FText::FromString(Message));

  const FSlateColor Color = bIsError ? FSlateColor(FLinearColor::Red)
                                     : FSlateColor(FPMSelectColors::TextMuted);
  StatusText->SetColorAndOpacity(Color);
}

// ===================================================================
// Selection
// ===================================================================

void UFPMCharacterSelectWidget::OnCharacterSelected(int32 Index) {
  if (Index < 0 || Index >= CachedCharacters.Num()) {
    return;
  }

  SelectedIndex = Index;
  const FFPMCharacterSummary &Selected = CachedCharacters[Index];

  if (SelectedCharacterText) {
    SelectedCharacterText->SetText(FText::FromString(
        FString::Printf(TEXT("\u25B6  %s"), *Selected.CharacterName)));
  }

  // Update button styles to highlight selected
  for (int32 i = 0; i < CharacterEntryButtons.Num(); ++i) {
    UButton *Btn = CharacterEntryButtons[i];
    if (!Btn)
      continue;

    FButtonStyle BtnStyle = Btn->GetStyle();
    if (i == Index) {
      // Selected — gold tint
      BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::EntrySelected);
      BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::EntrySelected);
    } else {
      // Not selected — default dark
      BtnStyle.Normal.TintColor = FSlateColor(FPMSelectColors::EntryNormal);
      BtnStyle.Hovered.TintColor = FSlateColor(FPMSelectColors::EntryHover);
    }
    Btn->SetStyle(BtnStyle);
  }

  UpdateButtonStates();

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Selected '%s' at index %d."),
         *Selected.CharacterName, Index);
}

void UFPMCharacterSelectWidget::UpdateButtonStates() {
  const bool bHasSelection =
      SelectedIndex >= 0 && SelectedIndex < CachedCharacters.Num();

  if (EnterWorldButton) {
    EnterWorldButton->SetIsEnabled(bHasSelection);
  }
}

// ===================================================================
// Button Handlers
// ===================================================================

void UFPMCharacterSelectWidget::OnEnterWorldClicked() {
  if (SelectedIndex < 0 || SelectedIndex >= CachedCharacters.Num()) {
    SetStatusMessage(TEXT("Select a character first."), true);
    return;
  }

  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->RequestEnterWorld(CachedCharacters[SelectedIndex].CharacterId);
    SetStatusMessage(TEXT("Entering world..."), false);
  }
}

void UFPMCharacterSelectWidget::OnCreateNewClicked() {
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->TransitionToCharacterCreation();
  }
}

void UFPMCharacterSelectWidget::OnBackClicked() {
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->ReturnToLogin();
  }
}
