// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"

#include "FPMCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * UFPMCharacterSelectWidget
 *
 * C++ backing class for the WBP_CharacterSelect Widget Blueprint.
 * Displays the player's existing characters in a scrollable list and
 * provides buttons to enter the world, delete, or create a new character.
 *
 * The Blueprint must contain widgets with these exact names:
 *   - CharacterListBox (VerticalBox) — dynamically populated
 *   - EnterWorldButton (Button)
 *   - DeleteCharacterButton (Button)
 *   - CreateNewButton (Button) — navigates to character creation
 *   - SelectedCharacterText (TextBlock)
 *   - StatusText (TextBlock) — loading/error messages
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

  // --- Bound Widgets (must match names in Blueprint) ---

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UVerticalBox> CharacterListBox;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> EnterWorldButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> DeleteCharacterButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UButton> CreateNewButton;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UTextBlock> SelectedCharacterText;

  UPROPERTY(meta = (BindWidget))
  TObjectPtr<UTextBlock> StatusText;

private:
  /** Called when Enter World is clicked. */
  UFUNCTION()
  void OnEnterWorldClicked();

  /** Called when Delete Character is clicked. */
  UFUNCTION()
  void OnDeleteCharacterClicked();

  /** Called when Create New is clicked. */
  UFUNCTION()
  void OnCreateNewClicked();

  /** Called when a character entry button is clicked. */
  void OnCharacterSelected(int32 Index);

  /** The currently selected character index in the list. */
  int32 SelectedIndex = -1;

  /** Cached character list from the server. */
  TArray<FFPMCharacterSummary> CachedCharacters;

  /** Update button enabled states based on selection. */
  void UpdateButtonStates();
};
