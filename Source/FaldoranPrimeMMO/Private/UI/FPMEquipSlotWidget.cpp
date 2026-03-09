// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMEquipSlotWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "Styling/CoreStyle.h"
#include "UI/FPMInventoryItemWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMEquipSlot, Log, All);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UFPMEquipSlotWidget::InitSlot(EFPMEquipSlot InSlot,
                                   UFPMInventoryComponent *InInventory) {
  Slot = InSlot;
  InventoryComp = InInventory;
  RefreshVisual();
}

void UFPMEquipSlotWidget::RefreshVisual() {
  if (!SlotLabel || !InventoryComp)
    return;

  FFPMEquippedItem Eq = InventoryComp->GetEquippedItem(Slot);
  if (Eq.IsValid()) {
    SlotLabel->SetText(FText::FromString(Eq.ItemID.ToString()));
    SlotLabel->SetColorAndOpacity(FLinearColor(0.9f, 0.85f, 0.6f, 1.0f));
    if (BackgroundBorder) {
      BackgroundBorder->SetBrushColor(FLinearColor(0.14f, 0.12f, 0.08f, 0.95f));
    }
  } else {
    SlotLabel->SetText(FPMGetEquipSlotDisplayName(Slot));
    SlotLabel->SetColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.5f, 0.7f));
    if (BackgroundBorder) {
      BackgroundBorder->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.16f, 0.9f));
    }
  }
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UFPMEquipSlotWidget::RebuildWidget() {
  if (!WidgetTree->RootWidget) {
    BuildUI();
  }
  return Super::RebuildWidget();
}

void UFPMEquipSlotWidget::NativeConstruct() { Super::NativeConstruct(); }

void UFPMEquipSlotWidget::BuildUI() {
  RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
      UCanvasPanel::StaticClass(), TEXT("EqSlotRoot"));
  WidgetTree->RootWidget = RootCanvas;

  // Background
  BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(
      UBorder::StaticClass(), TEXT("EqSlotBG"));
  BackgroundBorder->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.16f, 0.9f));
  BackgroundBorder->SetPadding(FMargin(2.0f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(BackgroundBorder);
    CS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CS->SetOffsets(FMargin(0.0f));
  }

  // Label
  SlotLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
                                                      TEXT("EqSlotLabel"));
  SlotLabel->SetText(FPMGetEquipSlotDisplayName(Slot));
  SlotLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 8));
  SlotLabel->SetColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.5f, 0.7f));
  SlotLabel->SetJustification(ETextJustify::Center);
  BackgroundBorder->SetContent(SlotLabel);

  // Hover overlay
  HoverOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                      TEXT("EqSlotHover"));
  HoverOverlay->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
  HoverOverlay->SetPadding(FMargin(0.0f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(HoverOverlay);
    CS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CS->SetOffsets(FMargin(0.0f));
  }
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

FReply UFPMEquipSlotWidget::NativeOnMouseButtonDown(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  // Only allow drag from a slot that has an item equipped
  if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)) {
    if (InventoryComp) {
      FFPMEquippedItem Eq = InventoryComp->GetEquippedItem(Slot);
      if (Eq.IsValid()) {
        return FReply::Handled().DetectDrag(TakeWidget(),
                                            EKeys::LeftMouseButton);
      }
    }
  }
  return FReply::Unhandled();
}

void UFPMEquipSlotWidget::NativeOnDragDetected(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent,
    UDragDropOperation *&OutOperation) {
  if (!InventoryComp)
    return;

  FFPMEquippedItem Eq = InventoryComp->GetEquippedItem(Slot);
  if (!Eq.IsValid())
    return;

  // Create a drag operation with this widget as the payload.
  // The grid widget can check if the payload is a UFPMEquipSlotWidget
  // to distinguish "unequip" from "backpack move".
  UDragDropOperation *DragOp = NewObject<UDragDropOperation>(this);
  DragOp->Payload = this; // UFPMEquipSlotWidget*
  DragOp->DefaultDragVisual = this;
  DragOp->Pivot = EDragPivot::CenterCenter;
  OutOperation = DragOp;

  UE_LOG(LogFPMEquipSlot, Log,
         TEXT("FPMEquipSlot: Started drag from slot %d (%s)"),
         static_cast<int32>(Slot), *Eq.ItemID.ToString());
}

bool UFPMEquipSlotWidget::NativeOnDrop(const FGeometry &InGeometry,
                                       const FDragDropEvent &InDragDropEvent,
                                       UDragDropOperation *InOperation) {
  if (!InOperation || !InventoryComp)
    return false;

  // Check if the payload is from a backpack item tile → equip it
  UFPMInventoryItemWidget *DraggedTile =
      Cast<UFPMInventoryItemWidget>(InOperation->Payload);
  if (DraggedTile) {
    const FFPMInventoryItem &DraggedItem = DraggedTile->GetItemData();
    UE_LOG(LogFPMEquipSlot, Log,
           TEXT("FPMEquipSlot: Item '%s' dropped on slot %d — requesting "
                "equip."),
           *DraggedItem.ItemID.ToString(), static_cast<int32>(Slot));

    InventoryComp->Server_EquipItem(Slot, DraggedItem.ItemID);
    return true;
  }

  // If another equip slot widget is dropped here, ignore for now
  // (could implement slot-to-slot swap later)
  return false;
}

// ---------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------

void UFPMEquipSlotWidget::NativeOnMouseEnter(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  bIsHovered = true;
  if (HoverOverlay) {
    HoverOverlay->SetBrushColor(FLinearColor(1.0f, 0.9f, 0.6f, 0.15f));
  }
}

void UFPMEquipSlotWidget::NativeOnMouseLeave(
    const FPointerEvent &InMouseEvent) {
  bIsHovered = false;
  if (HoverOverlay) {
    HoverOverlay->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
  }
}
