// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "Styling/CoreStyle.h"

#include "FPMInventoryGridWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UBorder;
class UTextBlock;
class UImage;
class UOverlay;
class USizeBox;
class UFPMInventoryItemWidget;
class UFPMInventoryComponent;
class UFPMEquipSlotWidget;

/**
 * UFPMInventoryGridWidget
 *
 * The main inventory UI panel for Faldoran Prime.
 *
 * Interaction: click an item to pick it up, press R to rotate,
 * click again to place it in a new cell. Right-click or ESC cancels.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMInventoryGridWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /** Bind to an inventory component and do the initial draw. */
  void InitializeInventory(UFPMInventoryComponent *InInventory);

  /** Clear and re-draw all backpack item tiles. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  void RefreshGrid();

  /** Refresh equipment slot visuals from current equipment state. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Equipment")
  void RefreshEquipment();

  /** Pixel size of one backpack grid cell. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPM|Inventory",
            meta = (ClampMin = "32", ClampMax = "128"))
  float CellSize = 64.0f;

protected:
  virtual TSharedRef<SWidget> RebuildWidget() override;
  virtual void NativeConstruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float InDeltaTime) override;

  // --- Click-to-pick-up interaction ---
  virtual FReply
  NativeOnMouseButtonDown(const FGeometry &InGeometry,
                          const FPointerEvent &InMouseEvent) override;
  virtual FReply NativeOnMouseMove(const FGeometry &InGeometry,
                                   const FPointerEvent &InMouseEvent) override;
  virtual FReply NativeOnKeyDown(const FGeometry &InGeometry,
                                 const FKeyEvent &InKeyEvent) override;
  virtual FReply NativeOnMouseWheel(const FGeometry &InGeometry,
                                    const FPointerEvent &InMouseEvent) override;
  virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:
  // -----------------------------------------------------------------------
  // Widget tree (built entirely in C++)
  // -----------------------------------------------------------------------

  UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;

  // --- Header ---
  UPROPERTY() TObjectPtr<UBorder> HeaderBG;
  UPROPERTY() TObjectPtr<UTextBlock> TitleText;
  UPROPERTY() TObjectPtr<UTextBlock> CloseButtonText;

  // --- Grid body ---
  UPROPERTY() TObjectPtr<UBorder> GridBG;
  UPROPERTY() TObjectPtr<UCanvasPanel> GridCanvas; // backpack item tiles here
  UPROPERTY() TObjectPtr<UImage> GridLines;

  // --- Footer ---
  UPROPERTY() TObjectPtr<UBorder> FooterBG;
  UPROPERTY() TObjectPtr<UTextBlock> CapacityText;

  // --- Ghost overlay (snaps to grid cell when holding an item) ---
  UPROPERTY() TObjectPtr<UBorder> GhostOverlay;

  // --- Cursor shadow (follows mouse when holding, even off-grid) ---
  UPROPERTY() TObjectPtr<UBorder> CursorShadow;
  UPROPERTY() TObjectPtr<UTextBlock> CursorShadowText;

  // --- Equipment slot widget refs (for RefreshEquipment) ---
  TMap<EFPMEquipSlot, TObjectPtr<UFPMEquipSlotWidget>> EquipSlotWidgets;

  // -----------------------------------------------------------------------
  // State
  // -----------------------------------------------------------------------

  UPROPERTY()
  TObjectPtr<UFPMInventoryComponent> InventoryComp;

  UPROPERTY()
  TArray<TObjectPtr<UFPMInventoryItemWidget>> ItemWidgets;

  float AnimTime = 0.0f;

  /** Panel top-left in screen-center-relative coords, set by BuildUI. */
  float PanelOriginX = 0.0f;
  float PanelOriginY = 0.0f;

  // --- Click-to-pick-up state ---
  bool bHoldingItem = false;
  FFPMInventoryItem HeldItem;    // copy of the item being held
  FIntPoint HeldItemOrigPos;     // original grid position
  bool bHeldItemRotated = false; // true if R was pressed
  bool bIsSplitPickup = false;   // true if Shift+click split a stack
  int32 SplitAmount = 0;         // how many items being split off
  FVector2D LastMouseLocalPos;   // for ghost overlay positioning

  // -----------------------------------------------------------------------
  // Helpers
  // -----------------------------------------------------------------------

  void BuildUI();
  void UpdateCapacityText();

  /** Create a single equipment slot block at (X,Y) relative to center. */
  UFPMEquipSlotWidget *CreateEquipSlotBlock(EFPMEquipSlot InSlot, float X,
                                            float Y, float SlotW, float SlotH);

  /** Convert local position (in GridCanvas space) to grid cell. */
  FIntPoint ScreenToCell(const FVector2D &LocalPos) const;

  /** Draw faint cell-divider lines. */
  void DrawCellLines();

  /** Close button handler. */
  UFUNCTION()
  void OnCloseClicked();

  /** Try to place the held item at the given cell. */
  void TryPlaceHeldItem(FIntPoint Cell);

  /** Cancel the pick-up and return item to original position. */
  void CancelHeldItem();

  /** Update the ghost overlay position and color based on mouse pos. */
  void UpdateGhostOverlay(const FVector2D &LocalMousePos);
};
