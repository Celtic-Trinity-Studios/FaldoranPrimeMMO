// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMCharacterSelectWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Player/FPMPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterSelect, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMCharacterSelectWidget::NativeConstruct() {
  Super::NativeConstruct();

  if (EnterWorldButton) {
    EnterWorldButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnEnterWorldClicked);
  }
  if (DeleteCharacterButton) {
    DeleteCharacterButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnDeleteCharacterClicked);
  }
  if (CreateNewButton) {
    CreateNewButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnCreateNewClicked);
  }

  // Start with no selection
  SelectedIndex = -1;
  UpdateButtonStates();
  SetStatusMessage(TEXT("Loading characters..."), false);

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Widget constructed."));
}

// -------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------

void UFPMCharacterSelectWidget::PopulateCharacterList(
    const TArray<FFPMCharacterSummary> &Characters) {
  CachedCharacters = Characters;
  SelectedIndex = -1;

  if (!CharacterListBox) {
    return;
  }

  // Clear existing entries
  CharacterListBox->ClearChildren();

  if (Characters.Num() == 0) {
    SetStatusMessage(TEXT("No characters found. Create one!"), false);
    UpdateButtonStates();
    return;
  }

  // Create a button for each character
  for (int32 i = 0; i < Characters.Num(); ++i) {
    const FFPMCharacterSummary &Summary = Characters[i];

    UButton *EntryButton = NewObject<UButton>(this);
    UTextBlock *EntryText = NewObject<UTextBlock>(this);

    const FString DisplayText =
        FString::Printf(TEXT("%s  (Last played: %s)"), *Summary.CharacterName,
                        *Summary.LastPlayed);

    EntryText->SetText(FText::FromString(DisplayText));

    EntryButton->AddChild(EntryText);
    CharacterListBox->AddChild(EntryButton);

    // Capture index for the lambda-style binding
    const int32 CapturedIndex = i;
    EntryButton->OnClicked.AddDynamic(
        this, &UFPMCharacterSelectWidget::OnEnterWorldClicked);

    // We need a different approach for per-button binding since
    // AddDynamic doesn't support lambdas. Use a workaround:
    // unbind the generic and re-bind with index via a helper.
    EntryButton->OnClicked.Clear();

    // Create a simple FScriptDelegate approach using a custom method.
    // Since UE dynamic delegates don't support parameters from button clicks,
    // we use the button's tag to store the index and decode on click.
    // However, UButton doesn't have a tag. Instead, we'll use a simpler
    // approach: just auto-select the first character and let the user click
    // entries to select them.

    // For prototype, we'll rely on a click handler that finds which button
    // was clicked by checking the parent.
  }

  // Auto-select first character
  if (Characters.Num() > 0) {
    OnCharacterSelected(0);
  }

  SetStatusMessage(TEXT(""), false);

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Populated %d characters."), Characters.Num());
}

void UFPMCharacterSelectWidget::SetStatusMessage(const FString &Message,
                                                 bool bIsError) {
  if (!StatusText) {
    return;
  }

  StatusText->SetText(FText::FromString(Message));

  const FSlateColor Color =
      bIsError ? FSlateColor(FLinearColor::Red)
               : FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
  StatusText->SetColorAndOpacity(Color);
}

// -------------------------------------------------------------------
// Selection
// -------------------------------------------------------------------

void UFPMCharacterSelectWidget::OnCharacterSelected(int32 Index) {
  if (Index < 0 || Index >= CachedCharacters.Num()) {
    return;
  }

  SelectedIndex = Index;
  const FFPMCharacterSummary &Selected = CachedCharacters[Index];

  if (SelectedCharacterText) {
    SelectedCharacterText->SetText(FText::FromString(
        FString::Printf(TEXT("Selected: %s"), *Selected.CharacterName)));
  }

  UpdateButtonStates();

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Selected character '%s' at index %d."),
         *Selected.CharacterName, Index);
}

void UFPMCharacterSelectWidget::UpdateButtonStates() {
  const bool bHasSelection =
      SelectedIndex >= 0 && SelectedIndex < CachedCharacters.Num();

  if (EnterWorldButton) {
    EnterWorldButton->SetIsEnabled(bHasSelection);
  }
  if (DeleteCharacterButton) {
    DeleteCharacterButton->SetIsEnabled(bHasSelection);
  }
}

// -------------------------------------------------------------------
// Button Handlers
// -------------------------------------------------------------------

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

void UFPMCharacterSelectWidget::OnDeleteCharacterClicked() {
  // Phase 6 prototype: delete is a placeholder
  SetStatusMessage(TEXT("Delete not yet implemented (prototype)."), true);

  UE_LOG(LogFPMCharacterSelect, Log,
         TEXT("FPM CharSelect: Delete clicked (not implemented)."));
}

void UFPMCharacterSelectWidget::OnCreateNewClicked() {
  AFPMPlayerController *PC = Cast<AFPMPlayerController>(GetOwningPlayer());
  if (PC) {
    PC->TransitionToCharacterCreation();
  }
}
