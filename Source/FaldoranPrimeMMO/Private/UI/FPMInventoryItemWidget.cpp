// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMInventoryItemWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMItemWidget, Log, All);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FLinearColor UFPMInventoryItemWidget::RarityToColor(EFPMItemRarity Rarity) {
  switch (Rarity) {
  case EFPMItemRarity::Common:
    return FLinearColor(0.60f, 0.60f, 0.60f, 1.0f);
  case EFPMItemRarity::Uncommon:
    return FLinearColor(0.12f, 0.78f, 0.22f, 1.0f);
  case EFPMItemRarity::Rare:
    return FLinearColor(0.12f, 0.45f, 0.92f, 1.0f);
  case EFPMItemRarity::Epic:
    return FLinearColor(0.58f, 0.10f, 0.85f, 1.0f);
  case EFPMItemRarity::Legendary:
    return FLinearColor(1.00f, 0.65f, 0.05f, 1.0f);
  default:
    return FLinearColor(0.50f, 0.50f, 0.50f, 1.0f);
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UFPMInventoryItemWidget::SetItemData(const FFPMInventoryItem &InItem,
                                          float InCellSize) {
  ItemData = InItem;
  CellSize = InCellSize;
  RefreshVisuals();
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UFPMInventoryItemWidget::RebuildWidget() {
  if (!WidgetTree->RootWidget) {
    BuildUI();
  }
  return Super::RebuildWidget();
}

void UFPMInventoryItemWidget::NativeConstruct() {
  Super::NativeConstruct();
  RefreshVisuals();

  UE_LOG(LogFPMItemWidget, Log,
         TEXT("FPMItemWidget: NativeConstruct — '%s' count=%d "
              "size=%dx%d cellSize=%.0f visible=%d opacity=%.2f"),
         *ItemData.ItemID.ToString(), ItemData.Count, ItemData.SizeX,
         ItemData.SizeY, CellSize, static_cast<int32>(GetVisibility()),
         GetRenderOpacity());
}

void UFPMInventoryItemWidget::NativeTick(const FGeometry &MyGeometry,
                                         float InDeltaTime) {
  Super::NativeTick(MyGeometry, InDeltaTime);
  AnimTime += InDeltaTime;

  const float Target = bIsHovered ? 1.0f : 0.0f;
  HoverAlpha = FMath::FInterpTo(HoverAlpha, Target, InDeltaTime, 12.0f);

  if (HoverOverlay) {
    HoverOverlay->SetRenderOpacity(HoverAlpha * 0.25f);
  }

  // Subtle idle pulse on rarity border for Legendary items
  if (RarityBorder && ItemData.Rarity == EFPMItemRarity::Legendary) {
    const float Pulse = 0.6f + 0.4f * FMath::Sin(AnimTime * 2.5f);
    FLinearColor GoldGlow = RarityToColor(EFPMItemRarity::Legendary);
    GoldGlow.A = Pulse;
    RarityBorder->SetBrushColor(GoldGlow);
  }
}

// ---------------------------------------------------------------------------
// Internal – build the widget tree
// ---------------------------------------------------------------------------

void UFPMInventoryItemWidget::BuildUI() {
  // Root canvas via WidgetTree (correct UMG pattern)
  RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
      UCanvasPanel::StaticClass(), TEXT("ItemRoot"));
  WidgetTree->RootWidget = RootCanvas;
  RootCanvas->SetClipping(EWidgetClipping::ClipToBounds);

  // 1 — Background border (filled)
  BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(
      UBorder::StaticClass(), TEXT("BGBorder"));
  BackgroundBorder->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.12f, 0.85f));
  BackgroundBorder->SetPadding(FMargin(0.0f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(BackgroundBorder);
    CS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CS->SetOffsets(FMargin(0.0f));
  }

  // 2 — Rarity border (2px inner glow)
  RarityBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                      TEXT("RarityBorder"));
  RarityBorder->SetPadding(FMargin(2.0f));
  FSlateBrush RarityBrush;
  RarityBrush.DrawAs = ESlateBrushDrawType::Border;
  RarityBrush.Margin = FMargin(2.0f / 64.0f);
  RarityBorder->SetBrush(RarityBrush);
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(RarityBorder);
    CS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CS->SetOffsets(FMargin(0.0f));
  }

  // 3 — Item icon (solid colored rectangle — replace with texture later)
  ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
                                                 TEXT("ItemIcon"));
  // Use a solid white brush so the tint color actually renders
  FSlateBrush IconBrush;
  IconBrush.DrawAs = ESlateBrushDrawType::Image;
  IconBrush.TintColor = FSlateColor(FLinearColor::White);
  ItemIcon->SetBrush(IconBrush);
  ItemIcon->SetColorAndOpacity(FLinearColor(0.3f, 0.6f, 0.9f, 0.8f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(ItemIcon);
    CS->SetAnchors(FAnchors(0.5f, 0.5f));
    CS->SetAlignment(FVector2D(0.5f, 0.5f));
    CS->SetSize(FVector2D(42.0f, 42.0f));
    CS->SetPosition(FVector2D(0.0f, 0.0f));
  }

  // 3b — Item name text (bottom of tile, clipped to cell)
  ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(
      UTextBlock::StaticClass(), TEXT("ItemName"));
  ItemNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 7));
  ItemNameText->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f));
  ItemNameText->SetJustification(ETextJustify::Center);
  ItemNameText->SetText(FText::FromString(TEXT("Item")));
  ItemNameText->SetAutoWrapText(false);
  ItemNameText->SetClipping(EWidgetClipping::ClipToBounds);
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(ItemNameText);
    // Stretch across bottom, height = 14px
    CS->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
    CS->SetAlignment(FVector2D(0.0f, 1.0f));
    CS->SetOffsets(FMargin(1.0f, -14.0f, 1.0f, 1.0f));
    CS->SetAutoSize(false);
  }

  // 4 — Stack badge background (bottom-right corner)
  StackBadgeBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                      TEXT("StackBadge"));
  StackBadgeBG->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
  StackBadgeBG->SetPadding(FMargin(2.0f, 1.0f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(StackBadgeBG);
    CS->SetAnchors(FAnchors(1.0f, 1.0f));
    CS->SetAlignment(FVector2D(1.0f, 1.0f));
    CS->SetAutoSize(true);
    CS->SetPosition(FVector2D(-2.0f, -2.0f));
  }

  // 4b — Stack count text
  StackCountText = WidgetTree->ConstructWidget<UTextBlock>(
      UTextBlock::StaticClass(), TEXT("StackText"));
  StackCountText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
  StackCountText->SetColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.7f, 1.0f));
  StackCountText->SetText(FText::FromString(TEXT("1")));
  StackBadgeBG->SetContent(StackCountText);

  // 5 — Hover highlight overlay
  HoverOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                      TEXT("HoverOverlay"));
  HoverOverlay->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
  HoverOverlay->SetPadding(FMargin(0.0f));
  {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(HoverOverlay);
    CS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CS->SetOffsets(FMargin(0.0f));
  }
}

// ---------------------------------------------------------------------------
// Internal – refresh existing widgets with current item data
// ---------------------------------------------------------------------------

void UFPMInventoryItemWidget::RefreshVisuals() {
  if (!RootCanvas)
    return;

  const FLinearColor Colour = RarityToColor(ItemData.Rarity);

  // Background: make items clearly distinguishable from empty cells
  // Use a brighter, more saturated version of the rarity colour
  if (BackgroundBorder) {
    FLinearColor Tint = Colour * 0.35f; // brighter than before (was 0.12)
    Tint.A = 0.92f;
    BackgroundBorder->SetBrushColor(Tint);
  }

  if (RarityBorder) {
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Border;
    Brush.Margin = FMargin(2.0f / (CellSize * ItemData.SizeX));
    Brush.TintColor = FSlateColor(Colour);
    RarityBorder->SetBrush(Brush);
  }

  if (ItemIcon) {
    const float IconW = CellSize * ItemData.SizeX * 0.55f;
    const float IconH = CellSize * ItemData.SizeY * 0.55f;
    if (UCanvasPanelSlot *CS = Cast<UCanvasPanelSlot>(ItemIcon->Slot)) {
      CS->SetSize(FVector2D(IconW, IconH));
    }
    // Tint the icon with the rarity colour
    FLinearColor IconTint = Colour;
    IconTint.A = 0.85f;
    ItemIcon->SetColorAndOpacity(IconTint);
  }

  // Item name label
  if (ItemNameText) {
    // Show a short version of the item name (strip "Item_" prefix)
    FString Name = ItemData.ItemID.ToString();
    Name.RemoveFromStart(TEXT("Item_"));
    Name.ReplaceInline(TEXT("_"), TEXT(" "));
    ItemNameText->SetText(FText::FromString(Name));
  }

  if (StackBadgeBG && StackCountText) {
    const bool bShowBadge = (ItemData.Count > 1);
    StackBadgeBG->SetVisibility(bShowBadge ? ESlateVisibility::HitTestInvisible
                                           : ESlateVisibility::Collapsed);
    if (bShowBadge) {
      StackCountText->SetText(FText::AsNumber(ItemData.Count));
    }
  }
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

FReply UFPMInventoryItemWidget::NativeOnMouseButtonDown(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  // Let the parent grid widget handle all clicks for the
  // click-to-pick-up interaction system.
  return FReply::Unhandled();
}

void UFPMInventoryItemWidget::NativeOnDragDetected(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent,
    UDragDropOperation *&OutOperation) {
  UDragDropOperation *DragOp = NewObject<UDragDropOperation>(this);
  DragOp->Payload = this;
  DragOp->DefaultDragVisual = this;
  DragOp->Pivot = EDragPivot::CenterCenter;
  OutOperation = DragOp;

  UE_LOG(LogFPMItemWidget, Log,
         TEXT("FPMItemWidget: Started drag of '%s' from (%d,%d)"),
         *ItemData.ItemID.ToString(), ItemData.GridX, ItemData.GridY);
}

bool UFPMInventoryItemWidget::NativeOnDrop(
    const FGeometry &InGeometry, const FDragDropEvent &InDragDropEvent,
    UDragDropOperation *InOperation) {
  return false; // Let grid panel handle it
}

// ---------------------------------------------------------------------------
// Hover
// ---------------------------------------------------------------------------

void UFPMInventoryItemWidget::NativeOnMouseEnter(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  bIsHovered = true;
}

void UFPMInventoryItemWidget::NativeOnMouseLeave(
    const FPointerEvent &InMouseEvent) {
  bIsHovered = false;
}
