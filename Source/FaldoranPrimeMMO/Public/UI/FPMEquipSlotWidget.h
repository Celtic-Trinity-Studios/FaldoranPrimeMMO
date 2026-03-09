// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Gameplay/FPMInventoryComponent.h"

#include "FPMEquipSlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UCanvasPanel;
class UFPMInventoryItemWidget;

/**
 * UFPMEquipSlotWidget
 *
 * A single equipment slot block in the inventory UI.
 * Handles drag-and-drop: accepts backpack item drops (equip)
 * and initiates drags from equipped items (unequip).
 *
 * Created in C++ by UFPMInventoryGridWidget::CreateEquipSlotBlock().
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMEquipSlotWidget : public UUserWidget {
  GENERATED_BODY()

public:
  /** Initialize with the slot type and inventory reference. */
  void InitSlot(EFPMEquipSlot InSlot, UFPMInventoryComponent *InInventory);

  /** Refresh visual to show current equipment state. */
  void RefreshVisual();

  /** Get the slot type. */
  EFPMEquipSlot GetSlot() const { return Slot; }

  /** Get the inventory component. */
  UFPMInventoryComponent *GetInventoryComp() const { return InventoryComp; }

protected:
  virtual TSharedRef<SWidget> RebuildWidget() override;
  virtual void NativeConstruct() override;

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

  // --- Hover ---
  virtual void NativeOnMouseEnter(const FGeometry &InGeometry,
                                  const FPointerEvent &InMouseEvent) override;
  virtual void NativeOnMouseLeave(const FPointerEvent &InMouseEvent) override;

private:
  UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
  UPROPERTY() TObjectPtr<UBorder> BackgroundBorder;
  UPROPERTY() TObjectPtr<UTextBlock> SlotLabel;
  UPROPERTY() TObjectPtr<UBorder> HoverOverlay;

  UPROPERTY()
  TObjectPtr<UFPMInventoryComponent> InventoryComp;

  EFPMEquipSlot Slot = EFPMEquipSlot::None;
  bool bIsHovered = false;

  void BuildUI();
};
