// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Gameplay/FPMInventoryComponent.h"

#include "FPMInventoryItemWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UOverlay;
class UCanvasPanel;
class UCanvasPanelSlot;
class UFPMInventoryDragOperation;

/**
 * UFPMInventoryItemWidget
 *
 * Renders a single item tile on the Tetris-style inventory grid.
 * Sized in pixels to exactly cover the cells the item occupies
 * (SizeX * CellSize wide, SizeY * CellSize tall).
 *
 * Visual layers (bottom → top):
 *   1. Background fill      — rarity-tinted semi-transparent panel
 *   2. Rarity border        — 1px glow in rarity colour
 *   3. Item icon            — placeholder coloured square (swap for UTexture2D)
 *   4. Stack count badge    — bottom-right corner, only when Count > 1
 *   5. Hover highlight      — brightens on mouse-over
 *
 * Drag & Drop:
 *   NativeOnMouseButtonDown detects left-click + hold movement and
 *   initiates a UFPMInventoryDragOperation carrying this item's data.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMInventoryItemWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /**
   * Populate the tile with item data and resize it to cover the right
   * number of grid cells. Call this immediately after creating the widget.
   *
   * @param InItem        The inventory item this tile represents.
   * @param InCellSize    Pixel size of one grid cell (same value used by the
   * grid).
   */
  void SetItemData(const FFPMInventoryItem &InItem, float InCellSize);

  /** Returns the item data this tile is currently showing. */
  const FFPMInventoryItem &GetItemData() const { return ItemData; }

  /** Map item rarity to a display colour. */
  static FLinearColor RarityToColor(EFPMItemRarity Rarity);

protected:
  virtual TSharedRef<SWidget> RebuildWidget() override;
  virtual void NativeConstruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float InDeltaTime) override;

  // --- Drag & Drop ---
  virtual FReply
  NativeOnMouseButtonDown(const FGeometry &InGeometry,
                          const FPointerEvent &InMouseEvent) override;
  virtual void NativeOnDragDetected(const FGeometry &InGeometry,
                                    const FPointerEvent &InMouseEvent,
                                    UDragDropOperation *&OutOperation) override;
  virtual bool NativeOnDrop(const FGeometry &InGeometry,
                            const FDragDropEvent &InDragDropEvent,
                            UDragDropOperation *InOperation) override;

  // --- Mouse hover feedback ---
  virtual void NativeOnMouseEnter(const FGeometry &InGeometry,
                                  const FPointerEvent &InMouseEvent) override;
  virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;

private:
  // -----------------------------------------------------------------------
  // Widget tree (built entirely in C++)
  // -----------------------------------------------------------------------

  UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
  UPROPERTY() TObjectPtr<UBorder> BackgroundBorder;
  UPROPERTY() TObjectPtr<UBorder> RarityBorder;
  UPROPERTY() TObjectPtr<UImage> ItemIcon;
  UPROPERTY() TObjectPtr<UBorder> StackBadgeBG;
  UPROPERTY() TObjectPtr<UTextBlock> StackCountText;
  UPROPERTY() TObjectPtr<UTextBlock> ItemNameText;
  UPROPERTY() TObjectPtr<UBorder> HoverOverlay;

  // -----------------------------------------------------------------------
  // State
  // -----------------------------------------------------------------------

  FFPMInventoryItem ItemData;
  float CellSize = 64.0f;
  bool bIsHovered = false;
  float HoverAlpha = 0.0f; // 0→1 animated on hover enter/leave
  float AnimTime = 0.0f;   // for idle shimmer / pulse

  // -----------------------------------------------------------------------
  // Helpers
  // -----------------------------------------------------------------------

  void BuildUI();
  void RefreshVisuals();
};
