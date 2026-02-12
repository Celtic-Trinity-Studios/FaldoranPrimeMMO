// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"

#include "FPMCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UCanvasPanel;
class UImage;
class UBorder;
class UOverlay;
class USizeBox;
class USpacer;
class UScrollBox;

/**
 * UFPMCharacterSelectWidget
 *
 * Self-contained character select screen widget for Faldoran Prime.
 *
 * Builds its entire UI programmatically in C++ with the same
 * "Glass & Gold" visual theme as the login screen.
 * Full-screen dark background, themed panel, styled character list,
 * and action buttons.
 *
 * NOTE: The WBP_CharacterSelect Blueprint should have its root be an
 * empty Canvas Panel (no children). All visual children are created
 * in NativeConstruct.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterSelectWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /**
   * Populate the character list from server-provided data.
   * Called by the PlayerController when ClientReceiveCharacterList fires.
   */
  void PopulateCharacterList(const TArray<FFPMCharacterSummary> &Characters);

  /** Show a status message (loading, error, info). */
  void SetStatusMessage(const FString &Message, bool bIsError);

protected:
  virtual void NativeConstruct() override;

private:
  // --- Programmatically created widgets ---

  UPROPERTY()
  TObjectPtr<UImage> BackgroundImage;

  UPROPERTY()
  TObjectPtr<UTextBlock> TitleText;

  UPROPERTY()
  TObjectPtr<UTextBlock> SubtitleText;

  UPROPERTY()
  TObjectPtr<UVerticalBox> CharacterListBox;

  UPROPERTY()
  TObjectPtr<UButton> EnterWorldButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> EnterWorldButtonText;

  UPROPERTY()
  TObjectPtr<UButton> CreateNewButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> CreateNewButtonText;

  UPROPERTY()
  TObjectPtr<UButton> BackButton;

  UPROPERTY()
  TObjectPtr<UTextBlock> BackButtonText;

  UPROPERTY()
  TObjectPtr<UTextBlock> SelectedCharacterText;

  UPROPERTY()
  TObjectPtr<UTextBlock> StatusText;

  UPROPERTY()
  TObjectPtr<UTextBlock> FooterText;

  // --- Helpers ---

  /** Build the entire UI tree and add it to the root canvas. */
  void BuildUI();

  /** Called when Enter World is clicked. */
  UFUNCTION()
  void OnEnterWorldClicked();

  /** Called when Create New is clicked. */
  UFUNCTION()
  void OnCreateNewClicked();

  /** Called when Back is clicked. */
  UFUNCTION()
  void OnBackClicked();

  /** Called when a character entry is selected. */
  void OnCharacterSelected(int32 Index);

  /** Update button enabled states based on selection. */
  void UpdateButtonStates();

  /** The currently selected character index in the list. */
  int32 SelectedIndex = -1;

  /** Cached character list from the server. */
  TArray<FFPMCharacterSummary> CachedCharacters;

  /** References to character entry buttons for styling updates. */
  TArray<TObjectPtr<UButton>> CharacterEntryButtons;
};
