// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
// BuildUI implementation for UFPMCharacterCreationWidget.
// Split from FPMCharacterCreationWidget.cpp to respect the 500-line limit.

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/FPMCharacterCreationWidget.h"

// Glass & Gold palette — matches login & character select screens
namespace CC {
static const FLinearColor DeepBG(0.039f, 0.047f, 0.063f, 1.0f);
static const FLinearColor PanelBG(0.059f, 0.078f, 0.118f, 0.7f);
static const FLinearColor Gold(0.773f, 0.627f, 0.349f, 1.0f);
static const FLinearColor GoldLight(0.933f, 0.804f, 0.553f, 1.0f);
static const FLinearColor TextMuted(0.6f, 0.6f, 0.6f, 0.7f);
static const FLinearColor White(1, 1, 1, 1);
static const FLinearColor BtnBG(0.651f, 0.545f, 0.298f, 1.0f);
static const FLinearColor BtnHover(0.718f, 0.612f, 0.369f, 1.0f);
static const FLinearColor BtnText(0.047f, 0.055f, 0.071f, 1.0f);
static const FLinearColor InputBG(1.f, 1.f, 1.f, 0.05f);
static const FLinearColor GhostBG(0, 0, 0, 0.01f);
static const FLinearColor ActiveGender(0.773f, 0.627f, 0.349f, 0.2f);
} // namespace CC

// =====================================================================
// UI HELPER METHODS
// =====================================================================

void UFPMCharacterCreationWidget::AddSectionLabel(UVerticalBox *P,
                                                  const FString &T) {
  UTextBlock *L = NewObject<UTextBlock>(this);
  FSlateFontInfo F = L->GetFont();
  F.Size = 14;
  L->SetFont(F);
  L->SetText(FText::FromString(T));
  L->SetColorAndOpacity(FSlateColor(CC::Gold));
  if (auto *S = P->AddChildToVerticalBox(L))
    S->SetPadding(FMargin(0, 0, 0, 10));
}

void UFPMCharacterCreationWidget::AddSubLabel(UVerticalBox *P,
                                              const FString &T) {
  UTextBlock *L = NewObject<UTextBlock>(this);
  FSlateFontInfo F = L->GetFont();
  F.Size = 11;
  L->SetFont(F);
  L->SetText(FText::FromString(T));
  L->SetColorAndOpacity(FSlateColor(CC::TextMuted));
  if (auto *S = P->AddChildToVerticalBox(L))
    S->SetPadding(FMargin(0, 8, 0, 4));
}

USlider *UFPMCharacterCreationWidget::AddSliderRow(UVerticalBox *P,
                                                   const FString &Label,
                                                   float Min, float Max,
                                                   float Def) {
  UHorizontalBox *Row = NewObject<UHorizontalBox>(this);
  if (!Label.IsEmpty()) {
    UTextBlock *Lbl = NewObject<UTextBlock>(this);
    FSlateFontInfo F = Lbl->GetFont();
    F.Size = 12;
    Lbl->SetFont(F);
    Lbl->SetText(FText::FromString(Label));
    Lbl->SetColorAndOpacity(FSlateColor(CC::TextMuted));
    if (auto *LS = Row->AddChildToHorizontalBox(Lbl)) {
      LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
      LS->SetPadding(FMargin(0, 0, 6, 0));
    }
  }
  USlider *Sl = NewObject<USlider>(this);
  Sl->SetMinValue(Min);
  Sl->SetMaxValue(Max);
  Sl->SetValue(Def);
  Sl->SetSliderBarColor(CC::InputBG);
  Sl->SetSliderHandleColor(CC::Gold);
  if (auto *SS = Row->AddChildToHorizontalBox(Sl))
    SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  if (auto *RS = P->AddChildToVerticalBox(Row))
    RS->SetPadding(FMargin(0, 0, 0, 2));
  return Sl;
}

USlider *UFPMCharacterCreationWidget::AddAffinityRow(UVerticalBox *P,
                                                     const FString &Label,
                                                     float Max, float Def,
                                                     UTextBlock *&OutVal) {
  UHorizontalBox *Row = NewObject<UHorizontalBox>(this);
  UTextBlock *Lbl = NewObject<UTextBlock>(this);
  FSlateFontInfo F = Lbl->GetFont();
  F.Size = 12;
  Lbl->SetFont(F);
  Lbl->SetText(FText::FromString(Label));
  Lbl->SetColorAndOpacity(FSlateColor(CC::TextMuted));
  if (auto *LS = Row->AddChildToHorizontalBox(Lbl)) {
    LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    LS->SetPadding(FMargin(0, 0, 6, 0));
  }
  USlider *Sl = NewObject<USlider>(this);
  Sl->SetMinValue(0);
  Sl->SetMaxValue(Max);
  Sl->SetStepSize(1);
  Sl->SetValue(Def);
  Sl->SetSliderBarColor(CC::InputBG);
  Sl->SetSliderHandleColor(CC::Gold);
  if (auto *SS = Row->AddChildToHorizontalBox(Sl))
    SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  OutVal = NewObject<UTextBlock>(this);
  FSlateFontInfo VF = OutVal->GetFont();
  VF.Size = 11;
  OutVal->SetFont(VF);
  OutVal->SetText(FText::FromString(FString::FromInt((int)Def)));
  OutVal->SetColorAndOpacity(FSlateColor(CC::TextMuted));
  if (auto *VS = Row->AddChildToHorizontalBox(OutVal)) {
    VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    VS->SetPadding(FMargin(6, 0, 0, 0));
  }
  if (auto *RS = P->AddChildToVerticalBox(Row))
    RS->SetPadding(FMargin(0, 0, 0, 2));
  return Sl;
}

UButton *UFPMCharacterCreationWidget::MakeButton(const FString &Label,
                                                 bool bGold) {
  UButton *B = NewObject<UButton>(this);
  FButtonStyle St = B->GetStyle();
  if (bGold) {
    St.Normal.TintColor = FSlateColor(CC::BtnBG);
    St.Hovered.TintColor = FSlateColor(CC::BtnHover);
    St.Pressed.TintColor = FSlateColor(CC::Gold);
  } else {
    St.Normal.TintColor = FSlateColor(CC::GhostBG);
    St.Hovered.TintColor = FSlateColor(FLinearColor(.773f, .627f, .349f, .08f));
    St.Pressed.TintColor = FSlateColor(FLinearColor(.773f, .627f, .349f, .15f));
  }
  B->SetStyle(St);
  UTextBlock *T = NewObject<UTextBlock>(this);
  FSlateFontInfo Ft = T->GetFont();
  Ft.Size = bGold ? 14 : 12;
  T->SetFont(Ft);
  T->SetText(FText::FromString(Label));
  T->SetColorAndOpacity(FSlateColor(bGold ? CC::BtnText : CC::Gold));
  T->SetJustification(ETextJustify::Center);
  B->AddChild(T);
  return B;
}

void UFPMCharacterCreationWidget::UpdateGenderButtons() {
  if (!GenderMaleBtn || !GenderFemaleBtn)
    return;
  FButtonStyle MS = GenderMaleBtn->GetStyle();
  FButtonStyle FS = GenderFemaleBtn->GetStyle();
  MS.Normal.TintColor = FSlateColor(bIsFemale ? CC::GhostBG : CC::ActiveGender);
  FS.Normal.TintColor = FSlateColor(bIsFemale ? CC::ActiveGender : CC::GhostBG);
  GenderMaleBtn->SetStyle(MS);
  GenderFemaleBtn->SetStyle(FS);
}

void UFPMCharacterCreationWidget::StyleComboBox(UComboBoxString *CB) {
  if (!CB)
    return;
  // Dropdown item row style — use getter/setter API
  FTableRowStyle IS = CB->GetItemStyle();
  IS.TextColor = FSlateColor(CC::White);
  IS.SelectedTextColor = FSlateColor(CC::Gold);
  // Row backgrounds — dark with subtle gold highlight on hover/active
  FSlateBrush DarkBrush;
  DarkBrush.TintColor = FSlateColor(CC::DeepBG);
  FSlateBrush HoverBrush;
  HoverBrush.TintColor =
      FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.15f));
  FSlateBrush ActiveBrush;
  ActiveBrush.TintColor =
      FSlateColor(FLinearColor(0.773f, 0.627f, 0.349f, 0.25f));
  IS.SetActiveBrush(ActiveBrush);
  IS.SetActiveHoveredBrush(ActiveBrush);
  IS.SetInactiveBrush(DarkBrush);
  IS.SetInactiveHoveredBrush(HoverBrush);
  IS.SetEvenRowBackgroundBrush(DarkBrush);
  IS.SetEvenRowBackgroundHoveredBrush(HoverBrush);
  IS.SetOddRowBackgroundBrush(DarkBrush);
  IS.SetOddRowBackgroundHoveredBrush(HoverBrush);
  CB->SetItemStyle(IS);
}

// =====================================================================
// BUILD UI — programmatic construction with Glass & Gold theme
// =====================================================================
void UFPMCharacterCreationWidget::BuildUI() {
  UCanvasPanel *Root = Cast<UCanvasPanel>(GetRootWidget());
  if (!Root)
    return;

  // Full-screen background
  BackgroundImage = NewObject<UImage>(this);
  BackgroundImage->SetColorAndOpacity(CC::DeepBG);
  if (auto *S = Root->AddChildToCanvas(BackgroundImage)) {
    S->SetAnchors(FAnchors(0, 0, 1, 1));
    S->SetOffsets(FMargin(0));
  }

  UOverlay *Ov = NewObject<UOverlay>(this);
  if (auto *S = Root->AddChildToCanvas(Ov)) {
    S->SetAnchors(FAnchors(0, 0, 1, 1));
    S->SetOffsets(FMargin(0));
  }
  UVerticalBox *MainV = NewObject<UVerticalBox>(this);
  if (auto *S = Ov->AddChildToOverlay(MainV)) {
    S->SetHorizontalAlignment(HAlign_Fill);
    S->SetVerticalAlignment(VAlign_Fill);
    S->SetPadding(FMargin(10, 6, 10, 4));
  }

  // ---- TITLE ----
  UTextBlock *Title = NewObject<UTextBlock>(this);
  {
    FSlateFontInfo F = Title->GetFont();
    F.Size = 36;
    Title->SetFont(F);
  }
  Title->SetText(FText::FromString(TEXT("FALDORAN PRIME")));
  Title->SetColorAndOpacity(FSlateColor(CC::White));
  Title->SetJustification(ETextJustify::Center);
  Title->SetShadowColorAndOpacity(FLinearColor(.773f, .627f, .349f, .4f));
  Title->SetShadowOffset(FVector2D(0, 2));
  if (auto *S = MainV->AddChildToVerticalBox(Title))
    S->SetHorizontalAlignment(HAlign_Center);
  UTextBlock *Sub = NewObject<UTextBlock>(this);
  {
    FSlateFontInfo F = Sub->GetFont();
    F.Size = 12;
    Sub->SetFont(F);
  }
  Sub->SetText(FText::FromString(TEXT("CREATE YOUR CHARACTER")));
  Sub->SetColorAndOpacity(FSlateColor(CC::Gold));
  Sub->SetJustification(ETextJustify::Center);
  if (auto *S = MainV->AddChildToVerticalBox(Sub)) {
    S->SetHorizontalAlignment(HAlign_Center);
    S->SetPadding(FMargin(0, 0, 0, 12));
  }

  // ---- 3-COLUMN LAYOUT ----
  UHorizontalBox *Cols = NewObject<UHorizontalBox>(this);
  if (auto *S = MainV->AddChildToVerticalBox(Cols))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

  // ==== LEFT PANEL: Appearance ====
  UBorder *LB = NewObject<UBorder>(this);
  LB->SetBrushColor(CC::PanelBG);
  LB->SetPadding(FMargin(16, 14));
  UScrollBox *LSc = NewObject<UScrollBox>(this);
  LB->AddChild(LSc);
  if (auto *S = Cols->AddChildToHorizontalBox(LB)) {
    S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    S->SetPadding(FMargin(0, 0, 8, 0));
  }
  USizeBox *LSz = NewObject<USizeBox>(this);
  LSz->SetWidthOverride(320);
  UVerticalBox *LV = NewObject<UVerticalBox>(this);
  LSz->AddChild(LV);
  LSc->AddChild(LSz);
  // ---- TAB BAR ----
  static const TCHAR *TabLabels[] = {TEXT("IDENTITY"), TEXT("BODY"),
                                     TEXT("FACE"), TEXT("HAIR")};
  UHorizontalBox *TabBar = NewObject<UHorizontalBox>(this);
  if (auto *S = LV->AddChildToVerticalBox(TabBar))
    S->SetPadding(FMargin(0, 0, 0, 8));
  TabButtons.SetNum(4);
  for (int i = 0; i < 4; ++i) {
    TabButtons[i] = MakeButton(TabLabels[i], false);
    if (auto *S = TabBar->AddChildToHorizontalBox(TabButtons[i]))
      S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  }
  TabButtons[0]->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnTabIdentityClicked);
  TabButtons[1]->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnTabBodyClicked);
  TabButtons[2]->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnTabFaceClicked);
  TabButtons[3]->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnTabHairClicked);

  // ---- TAB CONTENT PANELS ----
  TabPanels.SetNum(4);
  for (int i = 0; i < 4; ++i) {
    TabPanels[i] = NewObject<UVerticalBox>(this);
    if (auto *S = LV->AddChildToVerticalBox(TabPanels[i]))
      S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TabPanels[i]->SetVisibility(i == 0 ? ESlateVisibility::Visible
                                       : ESlateVisibility::Collapsed);
  }
  ActiveTabIndex = 0;
  UpdateTabButtonStyles();

  // ==== TAB 0: IDENTITY ====
  UVerticalBox *T0 = TabPanels[0];
  AddSectionLabel(T0, TEXT("IDENTITY"));
  AddSubLabel(T0, TEXT("Character Name"));
  NameInput = NewObject<UEditableTextBox>(this);
  NameInput->SetHintText(FText::FromString(TEXT("3-20 characters")));
  if (auto *S = T0->AddChildToVerticalBox(NameInput))
    S->SetPadding(FMargin(0, 0, 0, 8));
  // Race selection
  AddSubLabel(T0, TEXT("Race"));
  SpeciesComboBox = NewObject<UComboBoxString>(this);
  SpeciesComboBox->AddOption(TEXT("Human"));
  SpeciesComboBox->AddOption(TEXT("Half-Elf"));
  SpeciesComboBox->AddOption(TEXT("Elf"));
  SpeciesComboBox->AddOption(TEXT("Dwarf"));
  SpeciesComboBox->AddOption(TEXT("Halfling"));
  SpeciesComboBox->AddOption(TEXT("Half-Orc"));
  SpeciesComboBox->AddOption(TEXT("Gnome"));
  SpeciesComboBox->AddOption(TEXT("Kethari"));
  SpeciesComboBox->AddOption(TEXT("Rauken"));
  SpeciesComboBox->SetSelectedIndex(0);
  SpeciesComboBox->OnSelectionChanged.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnSpeciesChanged);
  if (auto *S = T0->AddChildToVerticalBox(SpeciesComboBox))
    S->SetPadding(FMargin(0, 0, 0, 8));
  StyleComboBox(SpeciesComboBox);
  // Gender toggle
  AddSubLabel(T0, TEXT("Gender"));
  UHorizontalBox *GRow = NewObject<UHorizontalBox>(this);
  GenderMaleBtn = MakeButton(TEXT("MALE"), false);
  GenderFemaleBtn = MakeButton(TEXT("FEMALE"), false);
  GenderMaleBtn->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnGenderMaleClicked);
  GenderFemaleBtn->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnGenderFemaleClicked);
  if (auto *S = GRow->AddChildToHorizontalBox(GenderMaleBtn))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  if (auto *S = GRow->AddChildToHorizontalBox(GenderFemaleBtn))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  if (auto *S = T0->AddChildToVerticalBox(GRow))
    S->SetPadding(FMargin(0, 0, 0, 4));

  // ==== TAB 1: BODY ====
  UVerticalBox *T1 = TabPanels[1];
  AddSectionLabel(T1, TEXT("BODY"));
  AddSubLabel(T1, TEXT("Body Type"));
  BodyTypeSlider = AddSliderRow(T1, TEXT(""), 0, 3, 0);
  BodyTypeSlider->SetStepSize(1);
  AddSubLabel(T1, TEXT("Skin Color"));
  {
    USizeBox *SB = NewObject<USizeBox>(this);
    SB->SetHeightOverride(24);
    SkinColorPreview = NewObject<UImage>(this);
    SkinColorPreview->SetColorAndOpacity(FLinearColor(0.8f, 0.6f, 0.5f, 1.f));
    SB->AddChild(SkinColorPreview);
    if (auto *S = T1->AddChildToVerticalBox(SB))
      S->SetPadding(FMargin(0, 0, 0, 4));
  }
  static const TCHAR *HSV[] = {TEXT("Hue"), TEXT("Sat"), TEXT("Brt")};
  SkinSliders.SetNum(3);
  float SkinD[] = {0.06f, 0.4f, 0.8f};
  for (int i = 0; i < 3; ++i)
    SkinSliders[i] = AddSliderRow(T1, HSV[i], 0, 1, SkinD[i]);

  // ==== TAB 2: FACE ====
  UVerticalBox *T2 = TabPanels[2];
  AddSectionLabel(T2, TEXT("FACE"));
  AddSubLabel(T2, TEXT("Eye Color"));
  {
    USizeBox *SB = NewObject<USizeBox>(this);
    SB->SetHeightOverride(24);
    EyeColorPreview = NewObject<UImage>(this);
    EyeColorPreview->SetColorAndOpacity(FLinearColor(0.3f, 0.5f, 0.8f, 1.f));
    SB->AddChild(EyeColorPreview);
    if (auto *S = T2->AddChildToVerticalBox(SB))
      S->SetPadding(FMargin(0, 0, 0, 4));
  }
  EyeSliders.SetNum(3);
  float EyeD[] = {0.6f, 0.6f, 0.8f};
  for (int i = 0; i < 3; ++i)
    EyeSliders[i] = AddSliderRow(T2, HSV[i], 0, 1, EyeD[i]);
  AddSubLabel(T2, TEXT("Facial Features"));
  MorphSliders.SetNum(4);
  static const TCHAR *ML[] = {TEXT("Jaw"), TEXT("Nose"), TEXT("Brow"),
                              TEXT("Lips")};
  for (int i = 0; i < 4; ++i)
    MorphSliders[i] = AddSliderRow(T2, ML[i], 0, 1, .5f);

  // ==== TAB 3: HAIR ====
  UVerticalBox *T3 = TabPanels[3];
  AddSectionLabel(T3, TEXT("HAIR"));
  AddSubLabel(T3, TEXT("Hair Style"));
  HairStyleComboBox = NewObject<UComboBoxString>(this);
  if (auto *S = T3->AddChildToVerticalBox(HairStyleComboBox))
    S->SetPadding(FMargin(0, 0, 0, 4));
  HairStyleComboBox->OnSelectionChanged.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnHairStyleChanged);
  StyleComboBox(HairStyleComboBox);
  AddSubLabel(T3, TEXT("Hair Color"));
  {
    USizeBox *SB = NewObject<USizeBox>(this);
    SB->SetHeightOverride(24);
    HairColorPreview = NewObject<UImage>(this);
    HairColorPreview->SetColorAndOpacity(FLinearColor(0.2f, 0.15f, 0.1f, 1.f));
    SB->AddChild(HairColorPreview);
    if (auto *S = T3->AddChildToVerticalBox(SB))
      S->SetPadding(FMargin(0, 0, 0, 4));
  }
  HairColorSliders.SetNum(3);
  float HairD[] = {0.08f, 0.5f, 0.2f};
  for (int i = 0; i < 3; ++i)
    HairColorSliders[i] = AddSliderRow(T3, HSV[i], 0, 1, HairD[i]);

  // ==== BIND ALL APPEARANCE SLIDERS ====
  auto Bind = [this](USlider *S) {
    if (S)
      S->OnValueChanged.AddDynamic(
          this, &UFPMCharacterCreationWidget::OnAppearanceChanged);
  };
  Bind(BodyTypeSlider);
  for (auto &S : SkinSliders)
    Bind(S);
  for (auto &S : EyeSliders)
    Bind(S);
  for (auto &S : HairColorSliders)
    Bind(S);
  for (auto &S : MorphSliders)
    Bind(S);

  UBorder *CB = NewObject<UBorder>(this);
  CB->SetBrushColor(CC::PanelBG);
  CB->SetPadding(FMargin(10));
  USizeBox *CenterSize = NewObject<USizeBox>(this);
  CenterSize->SetMaxDesiredWidth(600);
  CenterSize->AddChild(CB);
  if (auto *S = Cols->AddChildToHorizontalBox(CenterSize)) {
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    S->SetPadding(FMargin(0, 0, 8, 0));
  }
  UVerticalBox *CV = NewObject<UVerticalBox>(this);
  CB->AddChild(CV);
  PreviewImage = NewObject<UImage>(this);
  PreviewImage->SetColorAndOpacity(FLinearColor(.05f, .05f, .08f, 1));
  if (auto *S = CV->AddChildToVerticalBox(PreviewImage))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

  // ==== RIGHT PANEL: Affinities ====
  UBorder *RB = NewObject<UBorder>(this);
  RB->SetBrushColor(CC::PanelBG);
  RB->SetPadding(FMargin(16, 14));
  UScrollBox *RSc = NewObject<UScrollBox>(this);
  RB->AddChild(RSc);
  if (auto *S = Cols->AddChildToHorizontalBox(RB))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
  USizeBox *RSz = NewObject<USizeBox>(this);
  RSz->SetWidthOverride(320);
  UVerticalBox *RV = NewObject<UVerticalBox>(this);
  RSz->AddChild(RV);
  RSc->AddChild(RSz);
  AddSectionLabel(RV, TEXT("AFFINITIES"));
  PlaystyleTotalText = NewObject<UTextBlock>(this);
  {
    FSlateFontInfo F = PlaystyleTotalText->GetFont();
    F.Size = 13;
    PlaystyleTotalText->SetFont(F);
  }
  PlaystyleTotalText->SetColorAndOpacity(FSlateColor(CC::GoldLight));
  if (auto *S = RV->AddChildToVerticalBox(PlaystyleTotalText))
    S->SetPadding(FMargin(0, 0, 0, 6));
  static const TCHAR *PL[] = {TEXT("Martial"), TEXT("Ranged"),
                              TEXT("Magic"),   TEXT("Crafting"),
                              TEXT("Social"),  TEXT("Survival")};
  PlaystyleSliders.SetNum(6);
  PlaystyleValues.SetNum(6);
  for (int i = 0; i < 6; ++i) {
    UTextBlock *ValTxt = nullptr;
    PlaystyleSliders[i] = AddAffinityRow(RV, PL[i], 200, 100, ValTxt);
    PlaystyleValues[i] = ValTxt;
    PlaystyleSliders[i]->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnPlaystyleChanged);
  }
  AddSubLabel(RV, TEXT(""));
  MagicalTotalText = NewObject<UTextBlock>(this);
  {
    FSlateFontInfo F = MagicalTotalText->GetFont();
    F.Size = 13;
    MagicalTotalText->SetFont(F);
  }
  MagicalTotalText->SetColorAndOpacity(FSlateColor(CC::GoldLight));
  if (auto *S = RV->AddChildToVerticalBox(MagicalTotalText))
    S->SetPadding(FMargin(0, 0, 0, 6));
  static const TCHAR *MA[] = {TEXT("Fire"),   TEXT("Water"), TEXT("Earth"),
                              TEXT("Air"),    TEXT("Light"), TEXT("Shadow"),
                              TEXT("Nature"), TEXT("Arcane")};
  MagicalSliders.SetNum(8);
  MagicalValues.SetNum(8);
  for (int i = 0; i < 8; ++i) {
    UTextBlock *ValTxt = nullptr;
    MagicalSliders[i] = AddAffinityRow(RV, MA[i], 200, 100, ValTxt);
    MagicalValues[i] = ValTxt;
    MagicalSliders[i]->OnValueChanged.AddDynamic(
        this, &UFPMCharacterCreationWidget::OnMagicalChanged);
  }

  // ==== BOTTOM BAR ====
  UHorizontalBox *Bot = NewObject<UHorizontalBox>(this);
  if (auto *S = MainV->AddChildToVerticalBox(Bot))
    S->SetPadding(FMargin(0, 10, 0, 0));
  BackButton = MakeButton(TEXT("BACK"), false);
  BackButton->OnClicked.AddDynamic(this,
                                   &UFPMCharacterCreationWidget::OnBackClicked);
  if (auto *S = Bot->AddChildToHorizontalBox(BackButton))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
  ResultText = NewObject<UTextBlock>(this);
  {
    FSlateFontInfo F = ResultText->GetFont();
    F.Size = 12;
    ResultText->SetFont(F);
  }
  ResultText->SetColorAndOpacity(FSlateColor(CC::TextMuted));
  ResultText->SetJustification(ETextJustify::Center);
  if (auto *S = Bot->AddChildToHorizontalBox(ResultText)) {
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    S->SetVerticalAlignment(VAlign_Center);
  }
  SubmitButton = MakeButton(TEXT("CREATE CHARACTER"), true);
  SubmitButton->OnClicked.AddDynamic(
      this, &UFPMCharacterCreationWidget::OnSubmitClicked);
  if (auto *S = Bot->AddChildToHorizontalBox(SubmitButton))
    S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
}
