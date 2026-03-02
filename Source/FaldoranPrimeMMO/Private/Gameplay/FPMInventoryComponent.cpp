// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Gameplay/FPMInventoryComponent.h"
#include "Net/UnrealNetwork.h"

UFPMInventoryComponent::UFPMInventoryComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

void UFPMInventoryComponent::BeginPlay() {
  Super::BeginPlay();

  // Initialize slots on the server
  if (GetOwnerRole() == ROLE_Authority) {
    Slots.SetNum(NumSlots);
    for (auto &Slot : Slots) {
      Slot.Clear();
    }
  }
}

void UFPMInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  // Only replicate to the owning client (no need for all players to
  // see each other's inventory contents).
  DOREPLIFETIME_CONDITION(UFPMInventoryComponent, Slots, COND_OwnerOnly);
}

// -------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------

int32 UFPMInventoryComponent::AddItem(FName ItemID, int32 Count) {
  if (ItemID.IsNone() || Count <= 0)
    return 0;

  // Server-only operation
  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM Inventory: AddItem called on client — ignored."));
    return 0;
  }

  int32 Remaining = Count;

  // First pass: stack with existing slots of the same item
  // TODO: Look up MaxStackSize from a DataTable/DataAsset.
  //       For now, hardcoded to 99.
  constexpr int32 MaxStack = 99;

  for (auto &Slot : Slots) {
    if (Remaining <= 0)
      break;
    if (Slot.ItemID == ItemID && Slot.Count < MaxStack) {
      const int32 Space = MaxStack - Slot.Count;
      const int32 ToAdd = FMath::Min(Remaining, Space);
      Slot.Count += ToAdd;
      Remaining -= ToAdd;
    }
  }

  // Second pass: fill empty slots
  for (auto &Slot : Slots) {
    if (Remaining <= 0)
      break;
    if (Slot.IsEmpty()) {
      Slot.ItemID = ItemID;
      const int32 ToAdd = FMath::Min(Remaining, MaxStack);
      Slot.Count = ToAdd;
      Remaining -= ToAdd;
    }
  }

  const int32 Added = Count - Remaining;
  if (Added > 0) {
    OnInventoryChanged.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("FPM Inventory: Added %d x %s (%d couldn't fit)"),
           Added, *ItemID.ToString(), Remaining);
  }

  return Added;
}

int32 UFPMInventoryComponent::RemoveItem(FName ItemID, int32 Count) {
  if (ItemID.IsNone() || Count <= 0)
    return 0;

  if (GetOwnerRole() != ROLE_Authority) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM Inventory: RemoveItem called on client — ignored."));
    return 0;
  }

  int32 Remaining = Count;

  // Remove from last slots first (preserves early stacks)
  for (int32 i = Slots.Num() - 1; i >= 0 && Remaining > 0; --i) {
    if (Slots[i].ItemID == ItemID) {
      const int32 ToRemove = FMath::Min(Remaining, Slots[i].Count);
      Slots[i].Count -= ToRemove;
      Remaining -= ToRemove;
      if (Slots[i].Count <= 0) {
        Slots[i].Clear();
      }
    }
  }

  const int32 Removed = Count - Remaining;
  if (Removed > 0) {
    OnInventoryChanged.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("FPM Inventory: Removed %d x %s"), Removed,
           *ItemID.ToString());
  }

  return Removed;
}

int32 UFPMInventoryComponent::GetItemCount(FName ItemID) const {
  int32 Total = 0;
  for (const auto &Slot : Slots) {
    if (Slot.ItemID == ItemID) {
      Total += Slot.Count;
    }
  }
  return Total;
}

bool UFPMInventoryComponent::HasItem(FName ItemID, int32 Count) const {
  return GetItemCount(ItemID) >= Count;
}

void UFPMInventoryComponent::SwapSlots(int32 SlotA, int32 SlotB) {
  if (GetOwnerRole() != ROLE_Authority)
    return;
  if (!Slots.IsValidIndex(SlotA) || !Slots.IsValidIndex(SlotB))
    return;
  if (SlotA == SlotB)
    return;

  Swap(Slots[SlotA], Slots[SlotB]);
  OnInventoryChanged.Broadcast();
}

// -------------------------------------------------------------------
// Replication
// -------------------------------------------------------------------

void UFPMInventoryComponent::OnRep_Slots() {
  // Client-side: fire change event so UI refreshes
  OnInventoryChanged.Broadcast();
}

// -------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------

int32 UFPMInventoryComponent::FindStackableSlot(FName ItemID) const {
  constexpr int32 MaxStack = 99;
  for (int32 i = 0; i < Slots.Num(); ++i) {
    if (Slots[i].ItemID == ItemID && Slots[i].Count < MaxStack) {
      return i;
    }
  }
  return -1;
}

int32 UFPMInventoryComponent::FindEmptySlot() const {
  for (int32 i = 0; i < Slots.Num(); ++i) {
    if (Slots[i].IsEmpty()) {
      return i;
    }
  }
  return -1;
}
