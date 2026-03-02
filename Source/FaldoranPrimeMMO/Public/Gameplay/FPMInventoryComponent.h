// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"


#include "FPMInventoryComponent.generated.h"

/**
 * EFPMItemRarity
 *
 * Rarity tiers for inventory items. Affects UI color coding,
 * drop rates, and vendor pricing.
 */
UENUM(BlueprintType)
enum class EFPMItemRarity : uint8 {
  Common = 0,
  Uncommon,
  Rare,
  Epic,
  Legendary
};

/**
 * FFPMItemDefinition
 *
 * Static definition of an item type. This is design data — shared
 * across all instances of the same item. In a full implementation,
 * these would come from a DataTable or DataAsset.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMItemDefinition {
  GENERATED_BODY()

  /** Unique identifier for this item type (e.g., "Item_Rock", "Item_Wood"). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  FName ItemID;

  /** Human-readable display name. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  FText DisplayName;

  /** Item description shown in tooltips. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item",
            meta = (MultiLine = true))
  FText Description;

  /** Icon texture for UI display. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  TSoftObjectPtr<UTexture2D> Icon;

  /** Item rarity. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  EFPMItemRarity Rarity = EFPMItemRarity::Common;

  /** Maximum stack size (1 = unstackable). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  int32 MaxStackSize = 99;

  /** Weight per unit in kilograms. Affects carry capacity. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  float Weight = 0.1f;

  /** Whether this item can be dropped into the world. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  bool bCanDrop = true;
};

/**
 * FFPMInventorySlot
 *
 * A single slot in the player's inventory.
 * Holds an item ID reference + current stack count.
 * Replicated to the owning client for UI display.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMInventorySlot {
  GENERATED_BODY()

  /** Item type identifier. Empty = slot is empty. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  FName ItemID;

  /** Current stack count. 0 = empty slot. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 Count = 0;

  bool IsEmpty() const { return ItemID.IsNone() || Count <= 0; }

  void Clear() {
    ItemID = NAME_None;
    Count = 0;
  }
};

/**
 * Delegate fired when the inventory changes (item added, removed, moved).
 * UI binds to this to refresh display.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFPMOnInventoryChanged);

/**
 * UFPMInventoryComponent
 *
 * Server-authoritative inventory system attached to the player character.
 *
 * Design principles:
 * - Server is the sole authority on inventory state.
 * - Client sends requests (Server RPCs), server validates and applies.
 * - Changes replicate to the owning client via OnRep.
 * - UI listens to OnInventoryChanged delegate for updates.
 *
 * Slot layout: Fixed-size grid (default 40 slots = 8×5).
 * Stacking: Items with the same ItemID merge up to MaxStackSize.
 *
 * See MasterPlan.md Phase IV, Step 4.1.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FALDORANPRIMEMMO_API UFPMInventoryComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UFPMInventoryComponent();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // --- Public API (Server-authoritative) ---

  /**
   * Attempt to add an item to the inventory.
   * Stacks with existing items of the same type first, then uses empty slots.
   * @param ItemID The item type to add.
   * @param Count  Number to add.
   * @return Number of items actually added (may be less if inventory is full).
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  int32 AddItem(FName ItemID, int32 Count = 1);

  /**
   * Remove items from the inventory.
   * @param ItemID The item type to remove.
   * @param Count  Number to remove.
   * @return Number of items actually removed.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  int32 RemoveItem(FName ItemID, int32 Count = 1);

  /**
   * Check how many of a specific item the player has.
   * @param ItemID The item type to count.
   * @return Total count across all slots.
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  int32 GetItemCount(FName ItemID) const;

  /**
   * Check if the inventory has at least N of an item.
   * @param ItemID The item type to check.
   * @param Count  Minimum required count.
   * @return true if inventory contains >= Count of ItemID.
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  bool HasItem(FName ItemID, int32 Count = 1) const;

  /**
   * Swap the contents of two inventory slots.
   * Used for drag-and-drop UI rearrangement.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  void SwapSlots(int32 SlotA, int32 SlotB);

  /**
   * Get a read-only view of all inventory slots.
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  const TArray<FFPMInventorySlot> &GetSlots() const { return Slots; }

  /**
   * Get the total number of inventory slots.
   */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  int32 GetSlotCount() const { return Slots.Num(); }

  // --- Events ---

  /** Fired whenever inventory contents change. Bind UI to this. */
  UPROPERTY(BlueprintAssignable, Category = "FPM|Inventory")
  FFPMOnInventoryChanged OnInventoryChanged;

protected:
  /** Number of inventory slots (default 40 = 8 columns × 5 rows). */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|Inventory")
  int32 NumSlots = 40;

  virtual void BeginPlay() override;

private:
  /** The actual inventory storage. Replicated to owning client. */
  UPROPERTY(ReplicatedUsing = OnRep_Slots)
  TArray<FFPMInventorySlot> Slots;

  UFUNCTION()
  void OnRep_Slots();

  /** Find the first slot containing ItemID with room to stack, or -1. */
  int32 FindStackableSlot(FName ItemID) const;

  /** Find the first empty slot, or -1 if full. */
  int32 FindEmptySlot() const;
};
