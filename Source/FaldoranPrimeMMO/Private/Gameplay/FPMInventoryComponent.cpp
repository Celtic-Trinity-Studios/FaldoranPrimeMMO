// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Gameplay/FPMInventoryComponent.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMInventory, Log, All);

/** Default max stack size when item definition is not available (e.g. DB load).
 */
static constexpr int32 DefaultMaxStackSize = 99;

// ---------------------------------------------------------------------------
// Shared equip slot display name (used by FPMEquipSlotWidget and
// FPMInventoryGridWidget)
// ---------------------------------------------------------------------------
FText FPMGetEquipSlotDisplayName(EFPMEquipSlot Slot) {
  switch (Slot) {
  case EFPMEquipSlot::Crown:
    return FText::FromString(TEXT("Crown"));
  case EFPMEquipSlot::Face:
    return FText::FromString(TEXT("Face"));
  case EFPMEquipSlot::Neck:
    return FText::FromString(TEXT("Neck"));
  case EFPMEquipSlot::ChestCore:
    return FText::FromString(TEXT("Chest"));
  case EFPMEquipSlot::BackUpper:
    return FText::FromString(TEXT("Back U"));
  case EFPMEquipSlot::BackLower:
    return FText::FromString(TEXT("Back L"));
  case EFPMEquipSlot::ShoulderL:
    return FText::FromString(TEXT("Shldr L"));
  case EFPMEquipSlot::ShoulderR:
    return FText::FromString(TEXT("Shldr R"));
  case EFPMEquipSlot::ForearmL:
    return FText::FromString(TEXT("Arm L"));
  case EFPMEquipSlot::ForearmR:
    return FText::FromString(TEXT("Arm R"));
  case EFPMEquipSlot::Belt:
    return FText::FromString(TEXT("Belt"));
  case EFPMEquipSlot::HipL:
    return FText::FromString(TEXT("Hip L"));
  case EFPMEquipSlot::HipR:
    return FText::FromString(TEXT("Hip R"));
  case EFPMEquipSlot::ThighL:
    return FText::FromString(TEXT("Thigh L"));
  case EFPMEquipSlot::ThighR:
    return FText::FromString(TEXT("Thigh R"));
  case EFPMEquipSlot::ShinL:
    return FText::FromString(TEXT("Shin L"));
  case EFPMEquipSlot::ShinR:
    return FText::FromString(TEXT("Shin R"));
  case EFPMEquipSlot::FootL:
    return FText::FromString(TEXT("Foot L"));
  case EFPMEquipSlot::FootR:
    return FText::FromString(TEXT("Foot R"));
  case EFPMEquipSlot::MainHand:
    return FText::FromString(TEXT("Main"));
  case EFPMEquipSlot::OffHand:
    return FText::FromString(TEXT("Off"));
  case EFPMEquipSlot::RingL:
    return FText::FromString(TEXT("Ring L"));
  case EFPMEquipSlot::RingR:
    return FText::FromString(TEXT("Ring R"));
  case EFPMEquipSlot::GloveL:
    return FText::FromString(TEXT("Glove L"));
  case EFPMEquipSlot::GloveR:
    return FText::FromString(TEXT("Glove R"));
  default:
    return FText::FromString(TEXT("?"));
  }
}

// ---------------------------------------------------------------------------
// Console Command: FPM.Debug_SpawnTestItems
// ---------------------------------------------------------------------------

static UWorld *FindServerWorldForInventoryDebug() {
#if WITH_EDITOR
  if (GEngine) {
    for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
      UWorld *W = Context.World();
      if (W) {
        const ENetMode NetMode = W->GetNetMode();
        if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer) {
          return W;
        }
      }
    }
  }
#endif
  if (GEngine) {
    for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
      if (Context.World())
        return Context.World();
    }
  }
  return nullptr;
}

static FAutoConsoleCommand CmdDebugSpawnTestItems(
    TEXT("FPM.Debug_SpawnTestItems"),
    TEXT("Spawn a variety of test items into the first player's inventory."),
    FConsoleCommandDelegate::CreateLambda([]() {
      UWorld *World = FindServerWorldForInventoryDebug();
      if (!World) {
        UE_LOG(LogFPMInventory, Error,
               TEXT("FPMInventory: Debug_SpawnTestItems — no world found."));
        return;
      }

      UFPMInventoryComponent *Inv = nullptr;
      for (FConstPlayerControllerIterator It =
               World->GetPlayerControllerIterator();
           It; ++It) {
        APlayerController *PC = It->Get();
        if (!PC)
          continue;
        APawn *Pawn = PC->GetPawn();
        if (!Pawn)
          continue;
        Inv = Pawn->FindComponentByClass<UFPMInventoryComponent>();
        if (Inv)
          break;
      }

      if (!Inv) {
        UE_LOG(LogFPMInventory, Error,
               TEXT("FPMInventory: Debug_SpawnTestItems — no player with "
                    "inventory found. Make sure you're in-world first."));
        return;
      }

      struct FTestItem {
        FName ID;
        int32 Count;
        int32 SzX;
        int32 SzY;
      };

      // clang-format off
      const FTestItem TestItems[] = {
          {FName(TEXT("Item_Rock")),       5,  1, 1},
          {FName(TEXT("Item_Stick")),      3,  1, 2},
          {FName(TEXT("Item_Herb")),      10,  1, 1},
          {FName(TEXT("Item_Sword_Iron")), 1,  1, 3},
          {FName(TEXT("Item_Shield")),     1,  2, 2},
          {FName(TEXT("Item_Potion_HP")),  3,  1, 1},
          {FName(TEXT("Item_Ore_Iron")),   8,  1, 1},
          {FName(TEXT("Item_Axe_Great")),  1,  2, 4},
      };
      // clang-format on

      int32 Added = 0;
      for (const FTestItem &TI : TestItems) {
        if (Inv->AddItem(TI.ID, TI.Count, TI.SzX, TI.SzY)) {
          ++Added;
        }
      }

      UE_LOG(LogFPMInventory, Log,
             TEXT("FPMInventory: Debug_SpawnTestItems — added %d / %d item "
                  "types. Open inventory (I key) to verify."),
             Added, static_cast<int32>(UE_ARRAY_COUNT(TestItems)));

      if (GEngine) {
        GEngine->AddOnScreenDebugMessage(
            -1, 5.f, FColor::Green,
            FString::Printf(
                TEXT("FPM: Spawned %d / %d test item types into inventory."),
                Added, static_cast<int32>(UE_ARRAY_COUNT(TestItems))));
      }
    }));

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

UFPMInventoryComponent::UFPMInventoryComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

void UFPMInventoryComponent::BeginPlay() {
  Super::BeginPlay();
  // Grid is purely virtual — no slot array to pre-allocate.
  // Items starts empty; the server populates it from the DB on spawn.
}

void UFPMInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  // Only the owning client needs their own inventory/equipment contents.
  DOREPLIFETIME_CONDITION(UFPMInventoryComponent, Items, COND_OwnerOnly);
  DOREPLIFETIME_CONDITION(UFPMInventoryComponent, EquippedItems,
                          COND_OwnerOnly);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool UFPMInventoryComponent::AddItem(FName ItemID, int32 Count, int32 SzX,
                                     int32 SzY) {
  if (ItemID.IsNone() || Count <= 0 || SzX < 1 || SzY < 1) {
    return false;
  }

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: AddItem called on client — ignored. "
                "Use Server_MoveItem RPC for client-initiated changes."));
    return false;
  }

  int32 Remaining = Count;

  // --- Pass 1: Try to top up an existing stack of the same item ---
  // Only meaningful for 1×1 stackable items (multi-cell items are never
  // stacked).
  //
  // We record per-stack deltas so we can roll back if pass 2 fails.
  // This guarantees atomicity: the caller sees true only if ALL items
  // were placed, and false means nothing was mutated.
  TArray<TPair<int32, int32>> StackDeltas; // (Items index, amount added)

  if (SzX == 1 && SzY == 1) {
    // TODO: look up MaxStackSize from a DataTable once the registry is wired
    const int32 MaxStack = DefaultMaxStackSize;

    for (int32 si = 0; si < Items.Num() && Remaining > 0; ++si) {
      FFPMInventoryItem &Item = Items[si];

      if (Item.ItemID == ItemID && Item.SizeX == 1 && Item.SizeY == 1 &&
          Item.Count < MaxStack) {
        const int32 Space = MaxStack - Item.Count;
        const int32 ToAdd = FMath::Min(Remaining, Space);
        Item.Count += ToAdd;
        Remaining -= ToAdd;
        StackDeltas.Emplace(si, ToAdd);
      }
    }
  }

  if (Remaining <= 0) {
    OnInventoryChanged.Broadcast();
    UE_LOG(LogFPMInventory, Log,
           TEXT("FPMInventory: Stacked %d x %s into existing stack(s)."), Count,
           *ItemID.ToString());
    return true;
  }

  // --- Pass 2: Find first open position and place a new entry ---
  int32 PlaceX = 0, PlaceY = 0;
  if (!FindFirstOpen(SzX, SzY, PlaceX, PlaceY)) {
    // Roll back any partial stacking so the caller sees a clean failure.
    for (const TPair<int32, int32> &Delta : StackDeltas) {
      Items[Delta.Key].Count -= Delta.Value;
    }
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: No space for %s (%d×%d). Inventory full."),
           *ItemID.ToString(), SzX, SzY);
    return false;
  }

  FFPMInventoryItem NewItem;
  NewItem.ItemID = ItemID;
  NewItem.Count = Remaining;
  NewItem.GridX = PlaceX;
  NewItem.GridY = PlaceY;
  NewItem.SizeX = SzX;
  NewItem.SizeY = SzY;
  Items.Add(NewItem);

  OnInventoryChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Added %d x %s at (%d, %d) footprint %d×%d."),
         Remaining, *ItemID.ToString(), PlaceX, PlaceY, SzX, SzY);

  return true;
}

int32 UFPMInventoryComponent::RemoveItem(FName ItemID, int32 Count) {
  if (ItemID.IsNone() || Count <= 0)
    return 0;

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: RemoveItem called on client — ignored."));
    return 0;
  }

  int32 Remaining = Count;

  // Remove from the last matching stack first (preserves earlier stacks).
  for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i) {
    if (Items[i].ItemID != ItemID)
      continue;

    const int32 ToRemove = FMath::Min(Remaining, Items[i].Count);
    Items[i].Count -= ToRemove;
    Remaining -= ToRemove;

    if (Items[i].Count <= 0) {
      Items.RemoveAt(i);
    }
  }

  const int32 Removed = Count - Remaining;
  if (Removed > 0) {
    OnInventoryChanged.Broadcast();
    UE_LOG(LogFPMInventory, Log, TEXT("FPMInventory: Removed %d x %s."),
           Removed, *ItemID.ToString());
  }

  return Removed;
}

void UFPMInventoryComponent::RemoveAt(int32 X, int32 Y) {
  if (GetOwnerRole() != ROLE_Authority)
    return;

  const int32 Idx = FindItemIndexAt(X, Y);
  if (Idx != INDEX_NONE) {
    const FName RemovedID = Items[Idx].ItemID;
    Items.RemoveAt(Idx);
    OnInventoryChanged.Broadcast();
    UE_LOG(LogFPMInventory, Log, TEXT("FPMInventory: Removed %s at (%d, %d)."),
           *RemovedID.ToString(), X, Y);
  }
}

bool UFPMInventoryComponent::MoveItem(int32 FromX, int32 FromY, int32 ToX,
                                      int32 ToY) {
  if (GetOwnerRole() != ROLE_Authority)
    return false;

  const int32 Idx = FindItemIndexAt(FromX, FromY);
  if (Idx == INDEX_NONE) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: MoveItem — no item at (%d, %d)."), FromX, FromY);
    return false;
  }

  FFPMInventoryItem &Item = Items[Idx];

  // --- Stack merge: if both items are 1×1 and the same type, merge counts ---
  const int32 TargetIdx = FindItemIndexAt(ToX, ToY);
  if (TargetIdx != INDEX_NONE && TargetIdx != Idx) {
    FFPMInventoryItem &Target = Items[TargetIdx];

    if (Item.ItemID == Target.ItemID && Item.SizeX == 1 && Item.SizeY == 1 &&
        Target.SizeX == 1 && Target.SizeY == 1) {
      const int32 Space = DefaultMaxStackSize - Target.Count;
      const int32 ToMerge = FMath::Min(Item.Count, Space);

      if (ToMerge > 0) {
        Target.Count += ToMerge;
        Item.Count -= ToMerge;

        // If the source stack is fully consumed, remove it.
        if (Item.Count <= 0) {
          Items.RemoveAt(Idx);
        }

        OnInventoryChanged.Broadcast();

        UE_LOG(LogFPMInventory, Log,
               TEXT("FPMInventory: Merged %d x %s from (%d,%d) into stack at "
                    "(%d,%d) (now %d)."),
               ToMerge, *Target.ItemID.ToString(), FromX, FromY, ToX, ToY,
               Target.Count);
        return true;
      }

      // Target stack is already full — cannot merge.
      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: MoveItem — target stack at (%d,%d) is full "
                  "(%d/%d)."),
             ToX, ToY, Target.Count, DefaultMaxStackSize);
      return false;
    }
  }

  // --- Normal move: validate the target position (ignoring own footprint) ---
  if (!CanPlace(ToX, ToY, Item.SizeX, Item.SizeY, Idx)) {
    UE_LOG(
        LogFPMInventory, Warning,
        TEXT("FPMInventory: MoveItem — cannot place %s (%d×%d) at (%d, %d)."),
        *Item.ItemID.ToString(), Item.SizeX, Item.SizeY, ToX, ToY);
    return false;
  }

  Item.GridX = ToX;
  Item.GridY = ToY;
  OnInventoryChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Moved %s from (%d,%d) to (%d,%d)."),
         *Item.ItemID.ToString(), FromX, FromY, ToX, ToY);

  return true;
}

void UFPMInventoryComponent::Server_MoveItem_Implementation(int32 FromX,
                                                            int32 FromY,
                                                            int32 ToX,
                                                            int32 ToY) {
  MoveItem(FromX, FromY, ToX, ToY);
}

void UFPMInventoryComponent::Server_MoveAndRotateItem_Implementation(
    int32 FromX, int32 FromY, int32 ToX, int32 ToY, int32 NewSizeX,
    int32 NewSizeY) {
  if (GetOwnerRole() != ROLE_Authority)
    return;

  const int32 Idx = FindItemIndexAt(FromX, FromY);
  if (Idx == INDEX_NONE) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: MoveAndRotateItem — no item at (%d, %d)."),
           FromX, FromY);
    return;
  }

  FFPMInventoryItem &Item = Items[Idx];
  const int32 OldSizeX = Item.SizeX;
  const int32 OldSizeY = Item.SizeY;

  // --- Validate client-provided dimensions ---
  // A rotation is a 90° swap of the existing footprint.  The only legal
  // values for (NewSizeX, NewSizeY) are the current dimensions in either
  // order.  Reject anything else to prevent a malicious or buggy client
  // from writing arbitrary footprints into authoritative state.
  const bool bSameOrientation = (NewSizeX == OldSizeX && NewSizeY == OldSizeY);
  const bool bSwappedOrientation =
      (NewSizeX == OldSizeY && NewSizeY == OldSizeX);
  if (!bSameOrientation && !bSwappedOrientation) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: MoveAndRotateItem — invalid dimensions "
                "(%d×%d) for %s (expected %d×%d or %d×%d). Rejected."),
           NewSizeX, NewSizeY, *Item.ItemID.ToString(), OldSizeX, OldSizeY,
           OldSizeY, OldSizeX);
    return;
  }

  // --- Stack merge: if both items are 1×1 and the same type, merge counts ---
  // (This can fire even for 1×1 items if the client sent a rotate request.)
  const int32 TargetIdx = FindItemIndexAt(ToX, ToY);
  if (TargetIdx != INDEX_NONE && TargetIdx != Idx) {
    FFPMInventoryItem &Target = Items[TargetIdx];

    if (Item.ItemID == Target.ItemID && NewSizeX == 1 && NewSizeY == 1 &&
        Target.SizeX == 1 && Target.SizeY == 1) {
      const int32 Space = DefaultMaxStackSize - Target.Count;
      const int32 ToMerge = FMath::Min(Item.Count, Space);

      if (ToMerge > 0) {
        Target.Count += ToMerge;
        Item.Count -= ToMerge;

        if (Item.Count <= 0) {
          Items.RemoveAt(Idx);
        }

        OnInventoryChanged.Broadcast();

        UE_LOG(LogFPMInventory, Log,
               TEXT("FPMInventory: MoveAndRotate-merged %d x %s from (%d,%d) "
                    "into stack at (%d,%d) (now %d)."),
               ToMerge, *Target.ItemID.ToString(), FromX, FromY, ToX, ToY,
               Target.Count);
        return;
      }

      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: MoveAndRotateItem — target stack at (%d,%d) "
                  "is full (%d/%d)."),
             ToX, ToY, Target.Count, DefaultMaxStackSize);
      return;
    }
  }

  // Apply rotation
  Item.SizeX = NewSizeX;
  Item.SizeY = NewSizeY;

  if (!CanPlace(ToX, ToY, Item.SizeX, Item.SizeY, Idx)) {
    Item.SizeX = OldSizeX;
    Item.SizeY = OldSizeY;
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: MoveAndRotateItem — cannot place %s "
                "(%d×%d) at (%d, %d)."),
           *Item.ItemID.ToString(), NewSizeX, NewSizeY, ToX, ToY);
    return;
  }

  Item.GridX = ToX;
  Item.GridY = ToY;
  OnInventoryChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Moved+Rotated %s from (%d,%d) to (%d,%d) "
              "size now %d×%d."),
         *Item.ItemID.ToString(), FromX, FromY, ToX, ToY, NewSizeX, NewSizeY);
}

bool UFPMInventoryComponent::SplitStack(int32 FromX, int32 FromY, int32 ToX,
                                        int32 ToY, int32 SplitCount) {
  if (GetOwnerRole() != ROLE_Authority)
    return false;

  const int32 Idx = FindItemIndexAt(FromX, FromY);
  if (Idx == INDEX_NONE) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: SplitStack — no item at (%d, %d)."), FromX,
           FromY);
    return false;
  }

  FFPMInventoryItem &Source = Items[Idx];

  // Only 1×1 stackable items can be split
  if (Source.SizeX != 1 || Source.SizeY != 1) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: SplitStack — %s is not a 1×1 item, "
                "cannot split."),
           *Source.ItemID.ToString());
    return false;
  }

  // Need at least 2 to split
  if (Source.Count < 2) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: SplitStack — %s has count %d, "
                "cannot split."),
           *Source.ItemID.ToString(), Source.Count);
    return false;
  }

  // Clamp split amount
  const int32 ActualSplit = FMath::Clamp(SplitCount, 1, Source.Count - 1);

  // --- Case 1: Placing back on the same cell → no-op ---
  if (ToX == FromX && ToY == FromY) {
    UE_LOG(LogFPMInventory, Log,
           TEXT("FPMInventory: SplitStack — placed back on source (%d,%d), "
                "no-op."),
           FromX, FromY);
    return true; // nothing changes
  }

  // --- Case 2: Target cell has an existing item ---
  const int32 TargetIdx = FindItemIndexAt(ToX, ToY);
  if (TargetIdx != INDEX_NONE) {
    FFPMInventoryItem &Target = Items[TargetIdx];

    // Can only merge with the same item type (1×1 stacks)
    if (Target.ItemID != Source.ItemID || Target.SizeX != 1 ||
        Target.SizeY != 1) {
      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: SplitStack — cell (%d,%d) occupied by "
                  "different item '%s', cannot split here."),
             ToX, ToY, *Target.ItemID.ToString());
      return false;
    }

    // Merge into existing stack (cap at DefaultMaxStackSize)
    const int32 Space = DefaultMaxStackSize - Target.Count;
    const int32 ToMerge = FMath::Min(ActualSplit, Space);

    if (ToMerge <= 0) {
      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: SplitStack — target stack at (%d,%d) is "
                  "full (%d/%d)."),
             ToX, ToY, Target.Count, DefaultMaxStackSize);
      return false;
    }

    Source.Count -= ToMerge;
    Target.Count += ToMerge;

    OnInventoryChanged.Broadcast();

    UE_LOG(LogFPMInventory, Log,
           TEXT("FPMInventory: Split-merged %s — %d merged into stack at "
                "(%d,%d) (now %d), %d remain at (%d,%d)."),
           *Source.ItemID.ToString(), ToMerge, ToX, ToY, Target.Count,
           Source.Count, FromX, FromY);

    return true;
  }

  // --- Case 3: Target cell is empty → create new stack ---
  if (!CanPlace(ToX, ToY, 1, 1, Idx)) {
    UE_LOG(
        LogFPMInventory, Warning,
        TEXT("FPMInventory: SplitStack — cannot place new stack at (%d,%d)."),
        ToX, ToY);
    return false;
  }

  // Reduce source
  Source.Count -= ActualSplit;

  // Create new stack
  FFPMInventoryItem NewStack;
  NewStack.ItemID = Source.ItemID;
  NewStack.Count = ActualSplit;
  NewStack.GridX = ToX;
  NewStack.GridY = ToY;
  NewStack.SizeX = 1;
  NewStack.SizeY = 1;
  NewStack.Rarity = Source.Rarity;
  Items.Add(NewStack);

  OnInventoryChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Split %s — %d remain at (%d,%d), "
              "%d placed at (%d,%d)."),
         *Source.ItemID.ToString(), Source.Count, FromX, FromY, ActualSplit,
         ToX, ToY);

  return true;
}

void UFPMInventoryComponent::Server_SplitStack_Implementation(
    int32 FromX, int32 FromY, int32 ToX, int32 ToY, int32 SplitCount) {
  SplitStack(FromX, FromY, ToX, ToY, SplitCount);
}

int32 UFPMInventoryComponent::GetItemCount(FName ItemID) const {
  int32 Total = 0;
  for (const FFPMInventoryItem &Item : Items) {
    if (Item.ItemID == ItemID)
      Total += Item.Count;
  }
  return Total;
}

bool UFPMInventoryComponent::HasItem(FName ItemID, int32 Count) const {
  return GetItemCount(ItemID) >= Count;
}

FFPMInventoryItem UFPMInventoryComponent::FindItemAt(int32 X, int32 Y) const {
  const int32 Idx = FindItemIndexAt(X, Y);
  return (Idx != INDEX_NONE) ? Items[Idx] : FFPMInventoryItem{};
}

TArray<bool> UFPMInventoryComponent::BuildOccupancyMap() const {
  TArray<bool> Map;
  Map.SetNumZeroed(GridWidth * GridHeight);

  for (const FFPMInventoryItem &Item : Items) {
    for (int32 dy = 0; dy < Item.SizeY; ++dy) {
      for (int32 dx = 0; dx < Item.SizeX; ++dx) {
        const int32 cx = Item.GridX + dx;
        const int32 cy = Item.GridY + dy;

        // Guard against items that somehow exceed grid bounds.
        if (cx >= 0 && cx < GridWidth && cy >= 0 && cy < GridHeight) {
          Map[cy * GridWidth + cx] = true;
        }
      }
    }
  }

  return Map;
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void UFPMInventoryComponent::OnRep_Items() { OnInventoryChanged.Broadcast(); }

void UFPMInventoryComponent::OnRep_EquippedItems() {
  OnEquipmentChanged.Broadcast();
}

// ---------------------------------------------------------------------------
// Equipment API
// ---------------------------------------------------------------------------

bool UFPMInventoryComponent::EquipItem(EFPMEquipSlot Slot, FName ItemID) {
  if (Slot == EFPMEquipSlot::None || Slot == EFPMEquipSlot::MAX ||
      ItemID.IsNone()) {
    return false;
  }

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: EquipItem called on client -- ignored."));
    return false;
  }

  // Find the item in the backpack
  int32 BackpackIdx = INDEX_NONE;
  for (int32 i = 0; i < Items.Num(); ++i) {
    if (Items[i].ItemID == ItemID) {
      BackpackIdx = i;
      break;
    }
  }

  if (BackpackIdx == INDEX_NONE) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: EquipItem -- %s not found in backpack."),
           *ItemID.ToString());
    return false;
  }

  // Snapshot the item's original footprint before any mutations.
  const int32 ItemSzX = Items[BackpackIdx].SizeX;
  const int32 ItemSzY = Items[BackpackIdx].SizeY;

  // If slot already occupied, unequip the old item first
  for (int32 i = 0; i < EquippedItems.Num(); ++i) {
    if (EquippedItems[i].Slot == Slot) {
      // Return old item to backpack using its stored footprint.
      const FFPMEquippedItem &OldEq = EquippedItems[i];
      if (!AddItem(OldEq.ItemID, 1, OldEq.SizeX, OldEq.SizeY)) {
        UE_LOG(LogFPMInventory, Warning,
               TEXT("FPMInventory: EquipItem -- no backpack space for old %s."),
               *OldEq.ItemID.ToString());
        return false;
      }
      EquippedItems.RemoveAt(i);

      // AddItem may have shifted the Items array; re-find our item.
      BackpackIdx = INDEX_NONE;
      for (int32 j = 0; j < Items.Num(); ++j) {
        if (Items[j].ItemID == ItemID) {
          BackpackIdx = j;
          break;
        }
      }
      if (BackpackIdx == INDEX_NONE) {
        UE_LOG(LogFPMInventory, Warning,
               TEXT("FPMInventory: EquipItem -- %s vanished after "
                    "returning old item."),
               *ItemID.ToString());
        return false;
      }
      break;
    }
  }

  // Remove from backpack (just 1 count)
  FFPMInventoryItem &BpItem = Items[BackpackIdx];
  EFPMItemRarity ItemRarity = BpItem.Rarity;
  BpItem.Count -= 1;
  if (BpItem.Count <= 0) {
    Items.RemoveAt(BackpackIdx);
  }

  // Add to equipment — preserve original footprint.
  FFPMEquippedItem Eq;
  Eq.Slot = Slot;
  Eq.ItemID = ItemID;
  Eq.Rarity = ItemRarity;
  Eq.SizeX = ItemSzX;
  Eq.SizeY = ItemSzY;
  EquippedItems.Add(Eq);

  OnInventoryChanged.Broadcast();
  OnEquipmentChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log, TEXT("FPMInventory: Equipped %s in slot %d."),
         *ItemID.ToString(), (uint8)Slot);
  return true;
}

bool UFPMInventoryComponent::UnequipItem(EFPMEquipSlot Slot) {
  if (Slot == EFPMEquipSlot::None || Slot == EFPMEquipSlot::MAX) {
    return false;
  }

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: UnequipItem called on client -- ignored."));
    return false;
  }

  // Find the equipped item
  int32 EquipIdx = INDEX_NONE;
  for (int32 i = 0; i < EquippedItems.Num(); ++i) {
    if (EquippedItems[i].Slot == Slot) {
      EquipIdx = i;
      break;
    }
  }

  if (EquipIdx == INDEX_NONE) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: UnequipItem -- slot %d is empty."), (uint8)Slot);
    return false;
  }

  // Try to return to backpack — use the stored footprint, not 1×1.
  const FFPMEquippedItem &EqItem = EquippedItems[EquipIdx];
  const FName ItemID = EqItem.ItemID;
  const int32 EqSzX = EqItem.SizeX;
  const int32 EqSzY = EqItem.SizeY;
  if (!AddItem(ItemID, 1, EqSzX, EqSzY)) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: UnequipItem -- no backpack space for %s."),
           *ItemID.ToString());
    return false;
  }

  EquippedItems.RemoveAt(EquipIdx);
  OnEquipmentChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Unequipped %s from slot %d."), *ItemID.ToString(),
         (uint8)Slot);
  return true;
}

void UFPMInventoryComponent::Server_EquipItem_Implementation(EFPMEquipSlot Slot,
                                                             FName ItemID) {
  EquipItem(Slot, ItemID);
}

void UFPMInventoryComponent::Server_UnequipItem_Implementation(
    EFPMEquipSlot Slot) {
  UnequipItem(Slot);
}

FFPMEquippedItem
UFPMInventoryComponent::GetEquippedItem(EFPMEquipSlot Slot) const {
  for (const FFPMEquippedItem &Eq : EquippedItems) {
    if (Eq.Slot == Slot) {
      return Eq;
    }
  }
  return FFPMEquippedItem{};
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool UFPMInventoryComponent::CanPlace(int32 X, int32 Y, int32 SzX, int32 SzY,
                                      int32 IgnoreIndex) const {
  // Bounds check — the entire footprint must fit within the grid.
  if (X < 0 || Y < 0 || X + SzX > GridWidth || Y + SzY > GridHeight)
    return false;

  // Overlap check — AABB intersection against every other item.
  for (int32 i = 0; i < Items.Num(); ++i) {
    if (i == IgnoreIndex)
      continue;

    const FFPMInventoryItem &Other = Items[i];

    const bool bOverlapX =
        (X < Other.GridX + Other.SizeX) && (X + SzX > Other.GridX);
    const bool bOverlapY =
        (Y < Other.GridY + Other.SizeY) && (Y + SzY > Other.GridY);

    if (bOverlapX && bOverlapY)
      return false;
  }

  return true;
}

bool UFPMInventoryComponent::FindFirstOpen(int32 SzX, int32 SzY, int32 &OutX,
                                           int32 &OutY) const {
  // Scan top-left to bottom-right, stopping as soon as a valid spot is found.
  for (int32 y = 0; y <= GridHeight - SzY; ++y) {
    for (int32 x = 0; x <= GridWidth - SzX; ++x) {
      if (CanPlace(x, y, SzX, SzY)) {
        OutX = x;
        OutY = y;
        return true;
      }
    }
  }
  return false;
}

int32 UFPMInventoryComponent::FindItemIndexAt(int32 X, int32 Y) const {
  for (int32 i = 0; i < Items.Num(); ++i) {
    const FFPMInventoryItem &Item = Items[i];

    if (X >= Item.GridX && X < Item.GridX + Item.SizeX && Y >= Item.GridY &&
        Y < Item.GridY + Item.SizeY) {
      return i;
    }
  }
  return INDEX_NONE;
}

// ---------------------------------------------------------------------------
// DB Persistence
// ---------------------------------------------------------------------------

void UFPMInventoryComponent::LoadFromDB(UFPMDatabaseSubsystem *DB,
                                        const FGuid &CharacterId) {
  if (!DB || !DB->IsConnected()) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: LoadFromDB — DB not available."));
    return;
  }

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: LoadFromDB called on client — ignored."));
    return;
  }

  const FString CId =
      CharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);

  const FFPMDatabaseQueryResult Result = DB->ExecuteQuery(
      TEXT("SELECT item_id, count, grid_x, grid_y, size_x, size_y "
           "FROM inventory "
           "WHERE character_id = $1 "
           "ORDER BY grid_y, grid_x"),
      {CId});

  if (!Result.bSuccess) {
    UE_LOG(LogFPMInventory, Error,
           TEXT("FPMInventory: LoadFromDB query failed for %s — %s"), *CId,
           *Result.ErrorMessage);
    return;
  }

  // Rebuild the Items array from DB rows.
  Items.Reset(Result.Rows.Num());

  for (const TMap<FString, FString> &Row : Result.Rows) {
    const FString *ItemId = Row.Find(TEXT("item_id"));
    const FString *Count = Row.Find(TEXT("count"));
    const FString *GridX = Row.Find(TEXT("grid_x"));
    const FString *GridY = Row.Find(TEXT("grid_y"));
    const FString *SizeX = Row.Find(TEXT("size_x"));
    const FString *SizeY = Row.Find(TEXT("size_y"));
    if (!ItemId || !Count || !GridX || !GridY || !SizeX || !SizeY)
      continue;
    FFPMInventoryItem Item;
    Item.ItemID = FName(**ItemId);
    Item.Count = FCString::Atoi(**Count);
    Item.GridX = FCString::Atoi(**GridX);
    Item.GridY = FCString::Atoi(**GridY);
    Item.SizeX = FCString::Atoi(**SizeX);
    Item.SizeY = FCString::Atoi(**SizeY);
    if (Item.IsValid())
      Items.Add(Item);
  }

  OnInventoryChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Loaded %d item(s) from DB for character %s."),
         Items.Num(), *CId);

  // --- Load equipped items ---
  const FFPMDatabaseQueryResult EqResult =
      DB->ExecuteQuery(TEXT("SELECT slot, item_id, rarity, "
                            "COALESCE(size_x, 1) AS size_x, "
                            "COALESCE(size_y, 1) AS size_y "
                            "FROM equipment "
                            "WHERE character_id = $1 "
                            "ORDER BY slot"),
                       {CId});

  if (!EqResult.bSuccess) {
    UE_LOG(LogFPMInventory, Error,
           TEXT("FPMInventory: LoadFromDB equipment query failed for %s — %s"),
           *CId, *EqResult.ErrorMessage);
    return;
  }

  EquippedItems.Reset(EqResult.Rows.Num());

  for (const TMap<FString, FString> &EqRow : EqResult.Rows) {
    const FString *Slot = EqRow.Find(TEXT("slot"));
    const FString *ItemId = EqRow.Find(TEXT("item_id"));
    const FString *Rarity = EqRow.Find(TEXT("rarity"));
    const FString *SizeX = EqRow.Find(TEXT("size_x"));
    const FString *SizeY = EqRow.Find(TEXT("size_y"));
    if (!Slot || !ItemId || !Rarity || !SizeX || !SizeY)
      continue;
    FFPMEquippedItem Eq;
    Eq.Slot = static_cast<EFPMEquipSlot>(FCString::Atoi(**Slot));
    Eq.ItemID = FName(**ItemId);
    Eq.Rarity = static_cast<EFPMItemRarity>(FCString::Atoi(**Rarity));
    Eq.SizeX = FCString::Atoi(**SizeX);
    Eq.SizeY = FCString::Atoi(**SizeY);
    if (Eq.IsValid())
      EquippedItems.Add(Eq);
  }

  OnEquipmentChanged.Broadcast();

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Loaded %d equipped item(s) from DB for "
              "character %s."),
         EquippedItems.Num(), *CId);
}

bool UFPMInventoryComponent::SaveToDB(UFPMDatabaseSubsystem *DB,
                                      const FGuid &CharacterId) {
  if (!DB || !DB->IsConnected()) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: SaveToDB — DB not available."));
    return false;
  }

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogFPMInventory, Warning,
           TEXT("FPMInventory: SaveToDB called on client — ignored."));
    return false;
  }

  const FString CId =
      CharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);

  // --- Step 1: Delete all existing rows for this character ---
  // Simple replace-all strategy: delete + re-insert is safe because
  // the server is authoritative and no concurrent writes are expected.
  const FFPMDatabaseQueryResult DelResult = DB->ExecuteQuery(
      TEXT("DELETE FROM inventory WHERE character_id = $1"), {CId});

  if (!DelResult.bSuccess) {
    UE_LOG(LogFPMInventory, Error,
           TEXT("FPMInventory: SaveToDB — DELETE failed for %s — %s"), *CId,
           *DelResult.ErrorMessage);
    return false;
  }

  if (Items.IsEmpty()) {
    UE_LOG(LogFPMInventory, Log,
           TEXT("FPMInventory: SaveToDB — inventory empty for %s, "
                "cleared DB rows."),
           *CId);
    // Note: do NOT return here — equipment still needs to be persisted below.
  }

  // --- Step 2: Insert current grid state ---
  int32 Saved = 0;
  bool bAnyInventoryInsertFailed = false;
  for (const FFPMInventoryItem &Item : Items) {
    if (!Item.IsValid())
      continue;

    const FFPMDatabaseQueryResult InsResult = DB->ExecuteQuery(
        TEXT("INSERT INTO inventory "
             "(character_id, item_id, count, grid_x, grid_y, size_x, size_y) "
             "VALUES ($1, $2, $3, $4, $5, $6, $7)"),
        {CId, Item.ItemID.ToString(), FString::FromInt(Item.Count),
         FString::FromInt(Item.GridX), FString::FromInt(Item.GridY),
         FString::FromInt(Item.SizeX), FString::FromInt(Item.SizeY)});

    if (InsResult.bSuccess) {
      ++Saved;
    } else {
      bAnyInventoryInsertFailed = true;
      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: SaveToDB — INSERT failed for %s "
                  "at (%d,%d) — %s"),
             *Item.ItemID.ToString(), Item.GridX, Item.GridY,
             *InsResult.ErrorMessage);
    }
  }

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Saved %d/%d item(s) to DB for character %s."),
         Saved, Items.Num(), *CId);

  // --- Save equipped items (delete-all + re-insert) ---
  const FFPMDatabaseQueryResult EqDelResult = DB->ExecuteQuery(
      TEXT("DELETE FROM equipment WHERE character_id = $1"), {CId});

  if (!EqDelResult.bSuccess) {
    UE_LOG(LogFPMInventory, Error,
           TEXT("FPMInventory: SaveToDB — equipment DELETE failed for %s — %s"),
           *CId, *EqDelResult.ErrorMessage);
    return false;
  }

  int32 EqSaved = 0;
  bool bAnyEquipmentInsertFailed = false;
  for (const FFPMEquippedItem &Eq : EquippedItems) {
    if (!Eq.IsValid())
      continue;

    const FFPMDatabaseQueryResult EqInsResult = DB->ExecuteQuery(
        TEXT("INSERT INTO equipment "
             "(character_id, slot, item_id, rarity, "
             "size_x, size_y) "
             "VALUES ($1, $2, $3, $4, $5, $6)"),
        {CId, FString::FromInt(static_cast<int32>(Eq.Slot)),
         Eq.ItemID.ToString(), FString::FromInt(static_cast<int32>(Eq.Rarity)),
         FString::FromInt(Eq.SizeX), FString::FromInt(Eq.SizeY)});

    if (EqInsResult.bSuccess) {
      ++EqSaved;
    } else {
      bAnyEquipmentInsertFailed = true;
      UE_LOG(LogFPMInventory, Warning,
             TEXT("FPMInventory: SaveToDB — equipment INSERT failed for "
                  "slot %d — %s"),
             static_cast<int32>(Eq.Slot), *EqInsResult.ErrorMessage);
    }
  }

  UE_LOG(LogFPMInventory, Log,
         TEXT("FPMInventory: Saved %d/%d equipped item(s) to DB for "
              "character %s."),
         EqSaved, EquippedItems.Num(), *CId);

  return !bAnyInventoryInsertFailed && !bAnyEquipmentInsertFailed;
}
