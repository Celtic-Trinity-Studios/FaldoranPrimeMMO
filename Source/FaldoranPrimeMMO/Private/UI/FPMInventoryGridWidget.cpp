// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMInventoryGridWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "UI/FPMEquipSlotWidget.h"
#include "UI/FPMInventoryItemWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMInventoryGrid, Log, All);

static FSlateFontInfo MakeFont(int32 Size) {
  return FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UFPMInventoryGridWidget::InitializeInventory(
    UFPMInventoryComponent *InInventory) {
  if (!InInventory)
    return;
  InventoryComp = InInventory;
  InventoryComp->OnInventoryChanged.AddDynamic(
      this, &UFPMInventoryGridWidget::RefreshGrid);
  InventoryComp->OnEquipmentChanged.AddDynamic(
      this, &UFPMInventoryGridWidget::RefreshEquipment);
  // NOTE: Don't call RefreshGrid()/RefreshEquipment() here!
  // GridCanvas doesn't exist yet — BuildUI() runs in RebuildWidget()
  // which is triggered by AddToViewport(). We refresh in
  // NativeConstruct() instead, which runs AFTER RebuildWidget().
}

void UFPMInventoryGridWidget::RefreshGrid() {
  if (!GridCanvas || !InventoryComp) {
    UE_LOG(LogFPMInventoryGrid, Warning,
           TEXT("FPMInventoryGrid: RefreshGrid — GridCanvas=%p, "
                "InventoryComp=%p — aborting."),
           GridCanvas.Get(), InventoryComp.Get());
    return;
  }

  for (UFPMInventoryItemWidget *W : ItemWidgets) {
    if (W)
      W->RemoveFromParent();
  }
  ItemWidgets.Reset();

  const TArray<FFPMInventoryItem> &Items = InventoryComp->GetItems();
  UE_LOG(LogFPMInventoryGrid, Log,
         TEXT("FPMInventoryGrid: RefreshGrid — %d items to render, "
              "CellSize=%.0f, GridCanvas children=%d"),
         Items.Num(), CellSize, GridCanvas->GetChildrenCount());

  for (const FFPMInventoryItem &Item : Items) {
    if (!Item.IsValid()) {
      UE_LOG(LogFPMInventoryGrid, Warning,
             TEXT("FPMInventoryGrid: Skipping invalid item"));
      continue;
    }
    UFPMInventoryItemWidget *Tile = CreateWidget<UFPMInventoryItemWidget>(
        GetOwningPlayer(), UFPMInventoryItemWidget::StaticClass());
    if (!Tile) {
      UE_LOG(LogFPMInventoryGrid, Error,
             TEXT("FPMInventoryGrid: Failed to create tile for %s"),
             *Item.ItemID.ToString());
      continue;
    }
    Tile->SetItemData(Item, CellSize);
    UCanvasPanelSlot *CS = GridCanvas->AddChildToCanvas(Tile);
    if (CS) {
      CS->SetAnchors(FAnchors(0.0f, 0.0f));
      CS->SetPosition(FVector2D(Item.GridX * CellSize, Item.GridY * CellSize));
      CS->SetSize(FVector2D(Item.SizeX * CellSize, Item.SizeY * CellSize));
      CS->SetZOrder(1);
      UE_LOG(LogFPMInventoryGrid, Log,
             TEXT("FPMInventoryGrid:   Tile '%s' at grid(%d,%d) "
                  "px(%.0f,%.0f) size(%.0f,%.0f) ZOrder=1"),
             *Item.ItemID.ToString(), Item.GridX, Item.GridY,
             Item.GridX * CellSize, Item.GridY * CellSize,
             Item.SizeX * CellSize, Item.SizeY * CellSize);
    }
    ItemWidgets.Add(Tile);
  }
  UpdateCapacityText();
}

void UFPMInventoryGridWidget::RefreshEquipment() {
  if (!InventoryComp)
    return;
  for (auto &Pair : EquipSlotWidgets) {
    if (UFPMEquipSlotWidget *SlotWidget = Pair.Value) {
      SlotWidget->RefreshVisual();
    }
  }
}

// ---------------------------------------------------------------------------
// RebuildWidget / lifecycle
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UFPMInventoryGridWidget::RebuildWidget() {
  if (!WidgetTree->RootWidget) {
    BuildUI();
  }
  return Super::RebuildWidget();
}

void UFPMInventoryGridWidget::NativeConstruct() {
  Super::NativeConstruct();

  // At this point BuildUI() has already run (via RebuildWidget),
  // so GridCanvas exists. Now we can safely populate the grid.
  RefreshGrid();
  RefreshEquipment();
}

void UFPMInventoryGridWidget::NativeTick(const FGeometry &MyGeometry,
                                         float InDeltaTime) {
  Super::NativeTick(MyGeometry, InDeltaTime);
  AnimTime += InDeltaTime;
  if (HeaderBG) {
    const float A = 0.60f + 0.15f * FMath::Sin(AnimTime * 1.2f);
    HeaderBG->SetBrushColor(FLinearColor(0.08f, 0.06f, 0.03f, A));
  }

  // --- Cursor shadow: follows the mouse everywhere when holding ---
  if (bHoldingItem && CursorShadow) {
    if (FSlateApplication::IsInitialized()) {
      const FVector2D AbsCursorPos = FSlateApplication::Get().GetCursorPos();
      const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(AbsCursorPos);

      if (UCanvasPanelSlot *CS = Cast<UCanvasPanelSlot>(CursorShadow->Slot)) {
        // Offset so it doesn't sit right under the cursor
        CS->SetPosition(LocalPos + FVector2D(16.0f, 16.0f));
      }
      CursorShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
  } else if (CursorShadow) {
    CursorShadow->SetVisibility(ESlateVisibility::Collapsed);
  }
}

// ---------------------------------------------------------------------------
// CreateEquipSlotBlock
// ---------------------------------------------------------------------------

UFPMEquipSlotWidget *UFPMInventoryGridWidget::CreateEquipSlotBlock(
    EFPMEquipSlot InSlot, float X, float Y, float InSlotW, float InSlotH) {

  UFPMEquipSlotWidget *SlotWidget = CreateWidget<UFPMEquipSlotWidget>(
      GetOwningPlayer(), UFPMEquipSlotWidget::StaticClass());
  if (!SlotWidget)
    return nullptr;

  SlotWidget->InitSlot(InSlot, InventoryComp);

  UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(SlotWidget);
  CS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
  CS->SetAlignment(FVector2D(0.0f, 0.0f));
  CS->SetOffsets(FMargin(PanelOriginX + X, PanelOriginY + Y, InSlotW, InSlotH));
  CS->SetZOrder(5);

  EquipSlotWidgets.Add(InSlot, SlotWidget);
  return SlotWidget;
}

// ---------------------------------------------------------------------------
// BuildUI
// ---------------------------------------------------------------------------

void UFPMInventoryGridWidget::BuildUI() {
  if (!InventoryComp) {
    UE_LOG(LogFPMInventoryGrid, Warning,
           TEXT("FPMInventoryGrid: BuildUI called with no InventoryComp — skipping layout."));
    return;
  }
  const int32 Cols = InventoryComp->GetGridWidth();
  const int32 Rows = InventoryComp->GetGridHeight();
  const float GridW = Cols * CellSize;
  const float GridH = Rows * CellSize;

  // Section heights
  const float HeaderH = 40.0f;
  const float FooterH = 28.0f;
  const float EquipH = 340.0f;
  const float SepH = 8.0f;
  const float BpLabelH = 22.0f;
  const float Pad = 6.0f;

  // Slot size
  const float SlotW = 44.0f;
  const float SlotH = 44.0f;

  // Panel dimensions
  const float TotalW = FMath::Max(GridW + Pad * 2.0f, 460.0f);
  const float TotalH =
      HeaderH + EquipH + SepH + BpLabelH + GridH + Pad + FooterH;
  const float CX = TotalW * 0.5f; // center X within panel

  // Store panel origin so CreateEquipSlotBlock can use it
  PanelOriginX = -TotalW * 0.5f;
  PanelOriginY = -TotalH * 0.5f;

  // Helper lambda: place widget on RootCanvas at panel-relative coords
  auto Place = [&](UWidget *W, float panelX, float panelY, float w, float h,
                   int32 z) {
    UCanvasPanelSlot *CS = RootCanvas->AddChildToCanvas(W);
    CS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    CS->SetAlignment(FVector2D(0.0f, 0.0f)); // top-left
    CS->SetOffsets(FMargin(PanelOriginX + panelX, PanelOriginY + panelY, w, h));
    CS->SetZOrder(z);
  };

  // --- Root canvas ---
  RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
      UCanvasPanel::StaticClass(), TEXT("InvRoot"));
  check(RootCanvas);
  WidgetTree->RootWidget = RootCanvas;

  // === WINDOW BACKGROUND ===
  {
    UBorder *WindowBG = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("WindowBG"));
    WindowBG->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.06f, 0.94f));
    WindowBG->SetPadding(FMargin(0.0f));
    Place(WindowBG, 0, 0, TotalW, TotalH, 0);
  }

  // === HEADER ===
  {
    HeaderBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                    TEXT("HeaderBG"));
    HeaderBG->SetBrushColor(FLinearColor(0.08f, 0.06f, 0.03f, 0.88f));
    HeaderBG->SetPadding(FMargin(14.0f, 8.0f));
    Place(HeaderBG, 0, 0, TotalW, HeaderH, 10);

    TitleText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("TitleText"));
    TitleText->SetText(FText::FromString(TEXT("INVENTORY")));
    TitleText->SetFont(MakeFont(16));
    TitleText->SetColorAndOpacity(FLinearColor(0.95f, 0.80f, 0.40f, 1.0f));
    HeaderBG->SetContent(TitleText);
  }

  // Close button
  {
    UBorder *CloseBG = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("CloseBG"));
    CloseBG->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    CloseBG->SetPadding(FMargin(6.0f, 4.0f));
    Place(CloseBG, TotalW - 30.0f, 6.0f, 24.0f, HeaderH - 12.0f, 11);

    CloseButtonText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("CloseBtn"));
    CloseButtonText->SetText(FText::FromString(TEXT("X")));
    CloseButtonText->SetFont(MakeFont(14));
    CloseButtonText->SetColorAndOpacity(
        FLinearColor(0.85f, 0.25f, 0.20f, 1.0f));
    CloseBG->SetContent(CloseButtonText);
  }

  // === EQUIPMENT SECTION ===
  const float EqTop = HeaderH;
  const float EqCY =
      EqTop + EquipH * 0.5f; // center Y of equip area (panel-relative)

  // "EQUIPMENT" label
  {
    UTextBlock *EqLbl = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("EquipLabel"));
    EqLbl->SetText(FText::FromString(TEXT("EQUIPMENT")));
    EqLbl->SetFont(MakeFont(10));
    EqLbl->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f, 0.8f));
    Place(EqLbl, CX - 38.0f, EqTop + 4.0f, 76.0f, 16.0f, 6);
  }

  // Silhouette background
  {
    UBorder *Sil = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                        TEXT("SilhouetteBG"));
    Sil->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.12f, 0.3f));
    Sil->SetPadding(FMargin(0.0f));
    Place(Sil, CX - 80.0f, EqCY - 130.0f, 160.0f, 270.0f, 2);
  }

  // --- Equipment slots ---
  // Lambda: sx,sy = offset from equipment center. Calculates top-left for slot.
  auto ESlot = [&](EFPMEquipSlot S, float sx, float sy, float sw, float sh) {
    const float tlx = CX + sx - sw * 0.5f;   // top-left X
    const float tly = EqCY + sy - sh * 0.5f; // top-left Y
    CreateEquipSlotBlock(S, tlx, tly, sw, sh);
  };

  // Head
  ESlot(EFPMEquipSlot::Crown, 0, -130, SlotW, SlotH);
  ESlot(EFPMEquipSlot::Face, 0, -85, SlotW, SlotH);
  ESlot(EFPMEquipSlot::Neck, 0, -42, SlotW, SlotH);

  // Torso
  ESlot(EFPMEquipSlot::ChestCore, 0, 2, SlotW, SlotH);
  ESlot(EFPMEquipSlot::BackUpper, 52, -18, SlotW, SlotH);
  ESlot(EFPMEquipSlot::BackLower, 52, 28, SlotW, SlotH);

  // Shoulders
  ESlot(EFPMEquipSlot::ShoulderL, -95, -35, SlotW, SlotH);
  ESlot(EFPMEquipSlot::ShoulderR, 95, -35, SlotW, SlotH);

  // Forearms
  ESlot(EFPMEquipSlot::ForearmL, -110, 12, SlotW, SlotH);
  ESlot(EFPMEquipSlot::ForearmR, 110, 12, SlotW, SlotH);

  // Gloves
  ESlot(EFPMEquipSlot::GloveL, -125, 56, SlotW, SlotH);
  ESlot(EFPMEquipSlot::GloveR, 125, 56, SlotW, SlotH);

  // Weapons
  ESlot(EFPMEquipSlot::MainHand, -175, 22, SlotW, SlotH);
  ESlot(EFPMEquipSlot::OffHand, 175, 22, SlotW, SlotH);

  // Waist
  ESlot(EFPMEquipSlot::Belt, 0, 48, SlotW, SlotH);
  ESlot(EFPMEquipSlot::HipL, -60, 58, SlotW, SlotH);
  ESlot(EFPMEquipSlot::HipR, 60, 58, SlotW, SlotH);

  // Rings
  ESlot(EFPMEquipSlot::RingL, -175, 70, SlotW, SlotH);
  ESlot(EFPMEquipSlot::RingR, 175, 70, SlotW, SlotH);

  // Thighs
  ESlot(EFPMEquipSlot::ThighL, -40, 98, SlotW, SlotH);
  ESlot(EFPMEquipSlot::ThighR, 40, 98, SlotW, SlotH);

  // Shins
  ESlot(EFPMEquipSlot::ShinL, -40, 128, SlotW, SlotH);
  ESlot(EFPMEquipSlot::ShinR, 40, 128, SlotW, SlotH);

  // Feet
  ESlot(EFPMEquipSlot::FootL, -40, 154, SlotW, 36.0f);
  ESlot(EFPMEquipSlot::FootR, 40, 154, SlotW, 36.0f);

  // === BACKPACK SECTION ===
  const float BpTop = EqTop + EquipH + SepH;

  // "BACKPACK" label
  {
    UTextBlock *BpLbl = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("BpLabel"));
    BpLbl->SetText(FText::FromString(TEXT("BACKPACK")));
    BpLbl->SetFont(MakeFont(10));
    BpLbl->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f, 0.8f));
    Place(BpLbl, CX - 36.0f, BpTop, 72.0f, BpLabelH, 6);
  }

  const float GridTop = BpTop + BpLabelH;
  const float GridLeft = CX - GridW * 0.5f;

  // Grid background (slightly larger for border effect)
  GridBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                TEXT("GridBG"));
  GridBG->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.95f));
  GridBG->SetPadding(FMargin(0.0f));
  Place(GridBG, GridLeft - 2.0f, GridTop - 2.0f, GridW + 4.0f, GridH + 4.0f, 1);

  // Grid canvas (for cell borders and item tiles)
  GridCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
      UCanvasPanel::StaticClass(), TEXT("GridCanvas"));
  Place(GridCanvas, GridLeft, GridTop, GridW, GridH, 2);

  DrawCellLines();

  // === FOOTER ===
  const float FootTop = GridTop + GridH + Pad;
  {
    FooterBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                    TEXT("FooterBG"));
    FooterBG->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.06f, 0.85f));
    FooterBG->SetPadding(FMargin(14.0f, 5.0f));
    Place(FooterBG, 0, FootTop, TotalW, FooterH, 10);

    CapacityText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("CapacityText"));
    CapacityText->SetFont(MakeFont(9));
    CapacityText->SetColorAndOpacity(FLinearColor(0.60f, 0.60f, 0.70f, 1.0f));
    FooterBG->SetContent(CapacityText);
  }

  UpdateCapacityText();

  // === CURSOR SHADOW (floating item preview, follows mouse) ===
  {
    CursorShadow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                                                        TEXT("CursorShadow"));
    CursorShadow->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.18f, 0.88f));
    CursorShadow->SetPadding(FMargin(6.0f, 3.0f));
    CursorShadow->SetVisibility(ESlateVisibility::Collapsed);

    UCanvasPanelSlot *ShadowCS = RootCanvas->AddChildToCanvas(CursorShadow);
    ShadowCS->SetAnchors(FAnchors(0.0f, 0.0f));
    ShadowCS->SetAutoSize(true);
    ShadowCS->SetZOrder(50);

    CursorShadowText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("CursorShadowText"));
    CursorShadowText->SetFont(MakeFont(10));
    CursorShadowText->SetColorAndOpacity(
        FLinearColor(0.95f, 0.90f, 0.70f, 1.0f));
    CursorShadowText->SetText(FText::FromString(TEXT("Item")));
    CursorShadow->SetContent(CursorShadowText);
  }

  UE_LOG(LogFPMInventoryGrid, Log,
         TEXT("FPMInventoryGrid: BuildUI -- %dx%d grid, 26 equip slots, "
              "panel %.0fx%.0f px"),
         Cols, Rows, TotalW, TotalH);
}

// ---------------------------------------------------------------------------
// DrawCellLines — draw visible grid lines via 1px-gap cells
// ---------------------------------------------------------------------------

void UFPMInventoryGridWidget::DrawCellLines() {
  if (!GridCanvas)
    return;

  const int32 Cols = InventoryComp ? InventoryComp->GetGridWidth() : 8;
  const int32 Rows = InventoryComp ? InventoryComp->GetGridHeight() : 5;
  const float Gap = 2.0f; // 2px gap between cells for visible grid lines
  const float CellW = CellSize - Gap;

  for (int32 r = 0; r < Rows; ++r) {
    for (int32 c = 0; c < Cols; ++c) {
      FName CellName = *FString::Printf(TEXT("Cell_%d_%d"), c, r);
      UBorder *Cell = WidgetTree->ConstructWidget<UBorder>(
          UBorder::StaticClass(), CellName);
      Cell->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.16f, 1.0f));
      Cell->SetPadding(FMargin(0.0f));

      UCanvasPanelSlot *CS = GridCanvas->AddChildToCanvas(Cell);
      CS->SetAnchors(FAnchors(0.0f, 0.0f));
      CS->SetPosition(
          FVector2D(c * CellSize + Gap * 0.5f, r * CellSize + Gap * 0.5f));
      CS->SetSize(FVector2D(CellW, CellW));
      CS->SetZOrder(0);
    }
  }
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

void UFPMInventoryGridWidget::UpdateCapacityText() {
  if (!CapacityText)
    return;
  const int32 BpItems = InventoryComp ? InventoryComp->GetItems().Num() : 0;
  const int32 EqItems =
      InventoryComp ? InventoryComp->GetEquippedItems().Num() : 0;
  const int32 GridCols = InventoryComp ? InventoryComp->GetGridWidth() : 8;
  const int32 GridRows = InventoryComp ? InventoryComp->GetGridHeight() : 5;
  CapacityText->SetText(FText::FromString(
      FString::Printf(TEXT("%d backpack  |  %d equipped  |  %dx%d grid"),
                      BpItems, EqItems, GridCols, GridRows)));
}

void UFPMInventoryGridWidget::OnCloseClicked() { RemoveFromParent(); }

FIntPoint
UFPMInventoryGridWidget::ScreenToCell(const FVector2D &LocalPos) const {
  return FIntPoint(FMath::FloorToInt(LocalPos.X / CellSize),
                   FMath::FloorToInt(LocalPos.Y / CellSize));
}

// ---------------------------------------------------------------------------
// Click-to-Pick-Up Interaction
// ---------------------------------------------------------------------------

FReply UFPMInventoryGridWidget::NativeOnMouseButtonDown(
    const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) {
  if (!InventoryComp || !GridCanvas)
    return FReply::Unhandled();

  // Convert absolute screen position into GridCanvas-local coordinates.
  // This correctly handles the center-anchored layout without needing
  // to manually reverse-engineer slot offsets.
  const FGeometry GridGeom = GridCanvas->GetCachedGeometry();
  const FVector2D GridLocalPos =
      GridGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

  // Only handle clicks inside the grid area
  const float GridPixelW = InventoryComp->GetGridWidth() * CellSize;
  const float GridPixelH = InventoryComp->GetGridHeight() * CellSize;
  if (GridLocalPos.X < 0 || GridLocalPos.Y < 0 ||
      GridLocalPos.X >= GridPixelW || GridLocalPos.Y >= GridPixelH)
    return FReply::Unhandled();

  const FIntPoint Cell = ScreenToCell(GridLocalPos);
  const int32 GridW = InventoryComp->GetGridWidth();
  const int32 GridH = InventoryComp->GetGridHeight();
  if (Cell.X < 0 || Cell.X >= GridW || Cell.Y < 0 || Cell.Y >= GridH)
    return FReply::Unhandled();

  // --- RIGHT CLICK: cancel held item ---
  if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton) {
    if (bHoldingItem) {
      CancelHeldItem();
      return FReply::Handled();
    }
    return FReply::Unhandled();
  }

  // --- LEFT CLICK ---
  if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
    if (bHoldingItem) {
      // Place the held item
      TryPlaceHeldItem(Cell);
      return FReply::Handled();
    } else {
      // Try to pick up an item at this cell
      FFPMInventoryItem ClickedItem = InventoryComp->FindItemAt(Cell.X, Cell.Y);
      if (ClickedItem.IsValid()) {
        const bool bShiftHeld = InMouseEvent.IsShiftDown();
        const bool bCanSplit = bShiftHeld && ClickedItem.Count > 1 &&
                               ClickedItem.SizeX == 1 && ClickedItem.SizeY == 1;

        bHoldingItem = true;
        HeldItem = ClickedItem;
        HeldItemOrigPos = FIntPoint(ClickedItem.GridX, ClickedItem.GridY);
        bHeldItemRotated = false;
        LastMouseLocalPos = GridLocalPos;

        if (bCanSplit) {
          // Split: pick up half the stack
          bIsSplitPickup = true;
          SplitAmount = ClickedItem.Count / 2;
          HeldItem.Count = SplitAmount;

          UE_LOG(LogFPMInventoryGrid, Log,
                 TEXT("FPMInventoryGrid: Split-picked %d of '%s' "
                      "(total %d) from (%d,%d). Click to place."),
                 SplitAmount, *HeldItem.ItemID.ToString(), ClickedItem.Count,
                 Cell.X, Cell.Y);
        } else {
          bIsSplitPickup = false;
          SplitAmount = 0;

          UE_LOG(LogFPMInventoryGrid, Log,
                 TEXT("FPMInventoryGrid: Picked up '%s' (%d\u00d7%d) "
                      "from (%d,%d). Press R to rotate, click to place."),
                 *HeldItem.ItemID.ToString(), HeldItem.SizeX, HeldItem.SizeY,
                 Cell.X, Cell.Y);
        }

        // Create ghost overlay
        if (!GhostOverlay) {
          GhostOverlay = WidgetTree->ConstructWidget<UBorder>(
              UBorder::StaticClass(), TEXT("GhostOverlay"));
          UCanvasPanelSlot *CS = GridCanvas->AddChildToCanvas(GhostOverlay);
          CS->SetAnchors(FAnchors(0.0f, 0.0f));
          CS->SetZOrder(10);
        }
        GhostOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);

        // Cyan tint for splits, green for regular moves
        if (bIsSplitPickup) {
          GhostOverlay->SetBrushColor(FLinearColor(0.2f, 0.6f, 0.9f, 0.45f));
        } else {
          GhostOverlay->SetBrushColor(FLinearColor(0.3f, 0.9f, 0.3f, 0.35f));
        }

        // Refresh grid (for regular pickup, hides original visually)
        RefreshGrid();

        // Update cursor shadow text with item info
        if (CursorShadowText) {
          FString Name = HeldItem.ItemID.ToString();
          Name.RemoveFromStart(TEXT("Item_"));
          Name.ReplaceInline(TEXT("_"), TEXT(" "));
          if (HeldItem.Count > 1) {
            CursorShadowText->SetText(FText::FromString(
                FString::Printf(TEXT("%s x%d"), *Name, HeldItem.Count)));
          } else {
            CursorShadowText->SetText(FText::FromString(Name));
          }
        }
        if (CursorShadow) {
          // Tint with rarity color
          FLinearColor RarityTint =
              UFPMInventoryItemWidget::RarityToColor(HeldItem.Rarity);
          RarityTint = RarityTint * 0.3f;
          RarityTint.A = 0.88f;
          CursorShadow->SetBrushColor(RarityTint);
        }

        // Set keyboard focus to receive R key
        SetKeyboardFocus();

        return FReply::Handled();
      }
    }
  }

  return FReply::Unhandled();
}

FReply
UFPMInventoryGridWidget::NativeOnMouseMove(const FGeometry &InGeometry,
                                           const FPointerEvent &InMouseEvent) {
  if (!bHoldingItem || !GridCanvas)
    return FReply::Unhandled();

  const FGeometry GridGeom = GridCanvas->GetCachedGeometry();
  const FVector2D GridLocalPos =
      GridGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
  LastMouseLocalPos = GridLocalPos;

  UpdateGhostOverlay(GridLocalPos);

  return FReply::Handled();
}

FReply UFPMInventoryGridWidget::NativeOnKeyDown(const FGeometry &InGeometry,
                                                const FKeyEvent &InKeyEvent) {
  if (!bHoldingItem)
    return FReply::Unhandled();

  if (InKeyEvent.GetKey() == EKeys::R) {
    // Skip rotation for square items (1×1 or NxN) — no visual effect and
    // it avoids toggling bHeldItemRotated which reroutes the place RPC.
    if (HeldItem.SizeX == HeldItem.SizeY) {
      return FReply::Handled();
    }

    // Rotate: swap SizeX and SizeY
    const int32 Temp = HeldItem.SizeX;
    HeldItem.SizeX = HeldItem.SizeY;
    HeldItem.SizeY = Temp;
    bHeldItemRotated = !bHeldItemRotated;

    UE_LOG(LogFPMInventoryGrid, Log,
           TEXT("FPMInventoryGrid: Rotated '%s' — now %d×%d"),
           *HeldItem.ItemID.ToString(), HeldItem.SizeX, HeldItem.SizeY);

    // Update the ghost overlay size using stored grid-local position
    UpdateGhostOverlay(LastMouseLocalPos);

    return FReply::Handled();
  }

  if (InKeyEvent.GetKey() == EKeys::Escape) {
    CancelHeldItem();
    return FReply::Handled();
  }

  return FReply::Unhandled();
}

FReply
UFPMInventoryGridWidget::NativeOnMouseWheel(const FGeometry &InGeometry,
                                            const FPointerEvent &InMouseEvent) {
  if (!bHoldingItem || !GridCanvas)
    return FReply::Unhandled();

  // Skip rotation for square items — no visual effect.
  if (HeldItem.SizeX == HeldItem.SizeY) {
    return FReply::Handled();
  }

  // Any scroll direction rotates the held item (swap SizeX / SizeY)
  const int32 Temp = HeldItem.SizeX;
  HeldItem.SizeX = HeldItem.SizeY;
  HeldItem.SizeY = Temp;
  bHeldItemRotated = !bHeldItemRotated;

  UE_LOG(LogFPMInventoryGrid, Log,
         TEXT("FPMInventoryGrid: Scroll-rotated '%s' — now %d×%d"),
         *HeldItem.ItemID.ToString(), HeldItem.SizeX, HeldItem.SizeY);

  // Re-derive grid-local pos from the current mouse position
  const FGeometry GridGeom = GridCanvas->GetCachedGeometry();
  const FVector2D GridLocalPos =
      GridGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
  LastMouseLocalPos = GridLocalPos;
  UpdateGhostOverlay(GridLocalPos);

  return FReply::Handled();
}

void UFPMInventoryGridWidget::TryPlaceHeldItem(FIntPoint Cell) {
  if (!bHoldingItem || !InventoryComp)
    return;

  UE_LOG(LogFPMInventoryGrid, Log,
         TEXT("FPMInventoryGrid: Placing '%s' at (%d,%d) size %d\u00d7%d%s"),
         *HeldItem.ItemID.ToString(), Cell.X, Cell.Y, HeldItem.SizeX,
         HeldItem.SizeY, bIsSplitPickup ? TEXT(" (SPLIT)") : TEXT(""));

  if (bIsSplitPickup) {
    // Split stack: take SplitAmount from original, place at target
    InventoryComp->Server_SplitStack(HeldItemOrigPos.X, HeldItemOrigPos.Y,
                                     Cell.X, Cell.Y, SplitAmount);
  } else {
    // Regular move (with optional rotation)
    if (HeldItem.SizeX != HeldItem.SizeY || bHeldItemRotated) {
      InventoryComp->Server_MoveAndRotateItem(HeldItemOrigPos.X,
                                              HeldItemOrigPos.Y, Cell.X, Cell.Y,
                                              HeldItem.SizeX, HeldItem.SizeY);
    } else {
      InventoryComp->Server_MoveItem(HeldItemOrigPos.X, HeldItemOrigPos.Y,
                                     Cell.X, Cell.Y);
    }
  }

  bHoldingItem = false;
  bHeldItemRotated = false;
  bIsSplitPickup = false;
  SplitAmount = 0;
  if (GhostOverlay) {
    GhostOverlay->SetVisibility(ESlateVisibility::Collapsed);
  }
}

void UFPMInventoryGridWidget::CancelHeldItem() {
  if (!bHoldingItem)
    return;

  UE_LOG(LogFPMInventoryGrid, Log,
         TEXT("FPMInventoryGrid: Cancelled %s of '%s'"),
         bIsSplitPickup ? TEXT("split") : TEXT("pick-up"),
         *HeldItem.ItemID.ToString());

  bHoldingItem = false;
  bHeldItemRotated = false;
  bIsSplitPickup = false;
  SplitAmount = 0;
  if (GhostOverlay) {
    GhostOverlay->SetVisibility(ESlateVisibility::Collapsed);
  }
  RefreshGrid();
}

void UFPMInventoryGridWidget::UpdateGhostOverlay(
    const FVector2D &LocalMousePos) {
  if (!GhostOverlay || !InventoryComp)
    return;

  const FIntPoint Cell = ScreenToCell(LocalMousePos);
  const int32 GridW = InventoryComp->GetGridWidth();
  const int32 GridH = InventoryComp->GetGridHeight();

  // Position and size the ghost
  UCanvasPanelSlot *CS = Cast<UCanvasPanelSlot>(GhostOverlay->Slot);
  if (!CS)
    return;

  CS->SetPosition(FVector2D(Cell.X * CellSize, Cell.Y * CellSize));
  CS->SetSize(FVector2D(HeldItem.SizeX * CellSize, HeldItem.SizeY * CellSize));

  // Check if placement would be valid
  const bool bFits =
      (Cell.X >= 0 && Cell.Y >= 0 && Cell.X + HeldItem.SizeX <= GridW &&
       Cell.Y + HeldItem.SizeY <= GridH);

  // Green = valid, Red = invalid
  if (bFits) {
    GhostOverlay->SetBrushColor(FLinearColor(0.2f, 0.8f, 0.2f, 0.35f));
  } else {
    GhostOverlay->SetBrushColor(FLinearColor(0.9f, 0.2f, 0.2f, 0.35f));
  }
}
