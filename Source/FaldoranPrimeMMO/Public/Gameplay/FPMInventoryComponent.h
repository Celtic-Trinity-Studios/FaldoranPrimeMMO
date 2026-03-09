// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Net/UnrealNetwork.h"

#include "FPMInventoryComponent.generated.h"

/**
 * EFPMItemRarity
 *
 * Rarity tiers for inventory items. Drives UI colour borders,
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
 * EFPMEquipSlot
 *
 * Piecemeal equipment slots — every body part is individually armoured.
 * Characters have 24 attachment points (skeletal sockets).
 */
UENUM(BlueprintType)
enum class EFPMEquipSlot : uint8 {
  None = 0 UMETA(Hidden),

  // --- Head (3) ---
  Crown, // top of head: helmets, crowns, circlets
  Face,  // face: masks, visors, goggles
  Neck,  // neck: gorgets, necklaces, scarves

  // --- Torso (3) ---
  ChestCore, // chest: breastplates, vests, robes
  BackUpper, // upper back: capes, quivers, wings
  BackLower, // lower back: packs, cloaks

  // --- Arms (4) ---
  ShoulderL, // left pauldron
  ShoulderR, // right pauldron
  ForearmL,  // left bracer / vambrace
  ForearmR,  // right bracer / vambrace

  // --- Waist (3) ---
  Belt, // belt / sash
  HipL, // left hip: pouches, side weapons
  HipR, // right hip: pouches, side weapons

  // --- Legs (4) ---
  ThighL, // left thigh: tassets, leg plates
  ThighR, // right thigh
  ShinL,  // left greave
  ShinR,  // right greave

  // --- Feet (2) ---
  FootL, // left boot / sabatons
  FootR, // right boot / sabatons

  // --- Weapons (2) ---
  MainHand, // primary weapon (right hand)
  OffHand,  // shield / secondary (left hand)

  // --- Accessories (2) ---
  RingL, // left ring
  RingR, // right ring

  // --- Hands (2) ---
  GloveL, // left glove / gauntlet
  GloveR, // right glove / gauntlet

  MAX UMETA(Hidden)
};

/** Human-readable display name for an equip slot (shared by UI widgets). */
FALDORANPRIMEMMO_API FText FPMGetEquipSlotDisplayName(EFPMEquipSlot Slot);

/**
 * FFPMItemDefinition
 *
 * Static design data shared across all instances of the same item type.
 * In a full implementation these come from a DataTable; for now they are
 * used as inline defaults and passed in at AddItem time.
 *
 * SizeX / SizeY define the Tetris-style footprint on the inventory grid.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMItemDefinition {
  GENERATED_BODY()

  /** Unique identifier (e.g. "Item_Sword_Iron", "Item_Rock"). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  FName ItemID;

  /** Human-readable display name. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  FText DisplayName;

  /** Tooltip description. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item",
            meta = (MultiLine = true))
  FText Description;

  /** Icon texture for the inventory cell. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  TSoftObjectPtr<UTexture2D> Icon;

  /** Rarity tier — controls UI border colour. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  EFPMItemRarity Rarity = EFPMItemRarity::Common;

  /** Maximum items per stack (1 = unstackable). */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  int32 MaxStackSize = 99;

  /** Weight per unit in kilograms. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  float Weight = 0.1f;

  /** Whether this item can be dropped into the world. */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item")
  bool bCanDrop = true;

  /**
   * Grid footprint — width in cells (columns).
   * A dagger = 1, iron sword = 1×3, great-axe = 2×4, etc.
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item",
            meta = (ClampMin = 1, UIMin = 1))
  int32 SizeX = 1;

  /**
   * Grid footprint — height in cells (rows).
   */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPM|Item",
            meta = (ClampMin = 1, UIMin = 1))
  int32 SizeY = 1;
};

/**
 * FFPMInventoryItem
 *
 * A single item instance placed on the 2D inventory grid.
 * Stores its top-left origin (GridX, GridY) and its footprint (SizeX, SizeY)
 * so the UI can render it without needing to query the item registry every
 * tick.
 *
 * Replicated to the owning client only.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMInventoryItem {
  GENERATED_BODY()

  /** Item type. NAME_None = this entry is invalid. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  FName ItemID;

  /** Current stack count. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 Count = 0;

  /** Top-left column on the grid (0-based). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 GridX = 0;

  /** Top-left row on the grid (0-based). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 GridY = 0;

  /** Width of this item in grid cells (snapshotted from definition at
   * placement). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 SizeX = 1;

  /** Height of this item in grid cells (snapshotted from definition at
   * placement). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  int32 SizeY = 1;

  /** Rarity tier — snapshotted from the item definition at placement.
   *  Used by the UI to render the correct border glow colour. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Inventory")
  EFPMItemRarity Rarity = EFPMItemRarity::Common;

  bool IsValid() const { return !ItemID.IsNone() && Count > 0; }
};

/**
 * FFPMEquippedItem
 *
 * An item equipped in a specific body slot.
 * Lightweight struct — just the slot + what's in it.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMEquippedItem {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "FPM|Equipment")
  EFPMEquipSlot Slot = EFPMEquipSlot::None;

  UPROPERTY(BlueprintReadOnly, Category = "FPM|Equipment")
  FName ItemID;

  UPROPERTY(BlueprintReadOnly, Category = "FPM|Equipment")
  EFPMItemRarity Rarity = EFPMItemRarity::Common;

  /** Original grid footprint width (preserved so unequip restores it). */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Equipment")
  int32 SizeX = 1;

  /** Original grid footprint height. */
  UPROPERTY(BlueprintReadOnly, Category = "FPM|Equipment")
  int32 SizeY = 1;

  bool IsValid() const {
    return Slot != EFPMEquipSlot::None && !ItemID.IsNone();
  }
};

/** Fired whenever inventory contents change.  UI binds to this to refresh. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFPMOnInventoryChanged);

/** Fired whenever equipment changes. UI binds to this to refresh. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFPMOnEquipmentChanged);

/**
 * UFPMInventoryComponent
 *
 * Server-authoritative Tetris-style 2D grid inventory.
 *
 * Grid layout:  GridWidth (columns) × GridHeight (rows) — default 8 × 5.
 * Each item occupies a rectangular region defined by (GridX, GridY, SizeX,
 * SizeY).
 *
 * Design principles:
 *  - The server is the sole authority on all placement, removal, and movement.
 *  - Clients send requests via Server RPCs; the server validates and applies.
 *  - The full item list replicates to the owning client via OnRep_Items.
 *  - The UI listens to OnInventoryChanged and rebuilds its visual grid from
 * Items.
 *
 * Stacking: 1×1 stackable items (SizeX=SizeY=1, MaxStackSize>1) merge into
 *           existing stacks before a new grid slot is claimed.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FALDORANPRIMEMMO_API UFPMInventoryComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UFPMInventoryComponent();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // ---------------------------------------------------------------
  // Public API  (all writes must be called on the server)
  // ---------------------------------------------------------------

  /**
   * Add an item to the grid.
   * Stacks with an existing 1×1 stack first; otherwise finds the first open
   * region.
   *
   * @param ItemID   Item type to add.
   * @param Count    Number to add.
   * @param SizeX    Grid footprint width  (from item definition). Defaults to
   * 1×1.
   * @param SizeY    Grid footprint height (from item definition). Defaults to
   * 1×1.
   * @return true if at least one item was placed.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  bool AddItem(FName ItemID, int32 Count = 1, int32 SizeX = 1, int32 SizeY = 1);

  /**
   * Remove Count of ItemID from inventory (searches all stacks).
   * @return Number actually removed.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  int32 RemoveItem(FName ItemID, int32 Count = 1);

  /**
   * Remove the item whose footprint includes grid cell (X, Y).
   * The entire stack is removed regardless of count.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  void RemoveAt(int32 X, int32 Y);

  /**
   * Move an item from one grid origin to another (drag-and-drop).
   * Validates bounds and overlap before committing.
   * @return true if the move succeeded.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  bool MoveItem(int32 FromX, int32 FromY, int32 ToX, int32 ToY);

  /**
   * Client → Server RPC: request a drag-drop move.
   * Server validates and applies; result replicates back.
   */
  UFUNCTION(Server, Reliable)
  void Server_MoveItem(int32 FromX, int32 FromY, int32 ToX, int32 ToY);

  /**
   * Client → Server RPC: move an item and optionally rotate it.
   * NewSizeX/Y should be the desired dimensions (swap to rotate 90°).
   */
  UFUNCTION(Server, Reliable)
  void Server_MoveAndRotateItem(int32 FromX, int32 FromY, int32 ToX, int32 ToY,
                                int32 NewSizeX, int32 NewSizeY);

  /**
   * Split a stack of items. Takes SplitCount units from the item at
   * (FromX, FromY) and creates a new stack at (ToX, ToY).
   * Only works for 1×1 stackable items with Count > 1.
   * @return true if the split succeeded.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Inventory")
  bool SplitStack(int32 FromX, int32 FromY, int32 ToX, int32 ToY,
                  int32 SplitCount);

  /**
   * Client → Server RPC: request a stack split.
   * SplitCount items are moved from the stack at (FromX,FromY)
   * to a new stack at (ToX,ToY).
   */
  UFUNCTION(Server, Reliable)
  void Server_SplitStack(int32 FromX, int32 FromY, int32 ToX, int32 ToY,
                         int32 SplitCount);

  // ---------------------------------------------------------------
  // Equipment API (server-authoritative)
  // ---------------------------------------------------------------

  /**
   * Equip an item from the backpack into a body slot.
   * Removes the item from the grid and places it in EquippedItems.
   * If the slot is occupied, the old item is returned to the backpack.
   * @return true if equip succeeded.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Equipment")
  bool EquipItem(EFPMEquipSlot Slot, FName ItemID);

  /**
   * Unequip an item from a body slot back to the backpack.
   * @return true if unequip succeeded (slot was occupied AND backpack had
   * room).
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Equipment")
  bool UnequipItem(EFPMEquipSlot Slot);

  /** Client -> Server RPC: request equip from backpack to slot. */
  UFUNCTION(Server, Reliable)
  void Server_EquipItem(EFPMEquipSlot Slot, FName ItemID);

  /** Client -> Server RPC: request unequip from slot to backpack. */
  UFUNCTION(Server, Reliable)
  void Server_UnequipItem(EFPMEquipSlot Slot);

  /** Read the item in a given equipment slot. */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Equipment")
  FFPMEquippedItem GetEquippedItem(EFPMEquipSlot Slot) const;

  /** Read-only access to all equipped items. */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Equipment")
  const TArray<FFPMEquippedItem> &GetEquippedItems() const {
    return EquippedItems;
  }

  // ---------------------------------------------------------------
  // Read-only queries (safe to call on client)
  // ---------------------------------------------------------------

  /** Total count of ItemID across all stacks. */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  int32 GetItemCount(FName ItemID) const;

  /** Returns true if the player holds at least Count of ItemID. */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  bool HasItem(FName ItemID, int32 Count = 1) const;

  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  FFPMInventoryItem FindItemAt(int32 X, int32 Y) const;

  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  TArray<bool> BuildOccupancyMap() const;

  /** Read-only access to all placed items (for UI iteration). */
  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  const TArray<FFPMInventoryItem> &GetItems() const { return Items; }

  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  int32 GetGridWidth() const { return GridWidth; }

  UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FPM|Inventory")
  int32 GetGridHeight() const { return GridHeight; }

  // ---------------------------------------------------------------
  // Events
  // ---------------------------------------------------------------

  /** Bind UI to this to receive inventory change notifications. */
  UPROPERTY(BlueprintAssignable, Category = "FPM|Inventory")
  FFPMOnInventoryChanged OnInventoryChanged;

  /** Bind UI to this to receive equipment change notifications. */
  UPROPERTY(BlueprintAssignable, Category = "FPM|Equipment")
  FFPMOnEquipmentChanged OnEquipmentChanged;

  // ---------------------------------------------------------------
  // DB Persistence (server only)
  // ---------------------------------------------------------------

  /**
   * Load this character's inventory from the database.
   * Clears any existing Items and repopulates from the DB.
   * Called by the GameMode after the character pawn is spawned.
   *
   * @param DB            The database subsystem (must be connected).
   * @param CharacterId   UUID of the owning character.
   */
  void LoadFromDB(class UFPMDatabaseSubsystem *DB, const FGuid &CharacterId);

  /**
   * Persist the current inventory to the database.
   * Deletes all existing rows for this character and re-inserts the
   * current Items list (simple replace-all strategy).
   * Called on logout and optionally on item change.
   *
   * @param DB            The database subsystem (must be connected).
   * @param CharacterId   UUID of the owning character.
   * @return true if all writes succeeded, false on any failure or invalid state.
   */
  bool SaveToDB(class UFPMDatabaseSubsystem *DB, const FGuid &CharacterId);

protected:
  /** Number of columns in the grid. */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|Inventory",
            meta = (ClampMin = 1, UIMin = 1))
  int32 GridWidth = 8;

  /** Number of rows in the grid. */
  UPROPERTY(EditDefaultsOnly, Category = "FPM|Inventory",
            meta = (ClampMin = 1, UIMin = 1))
  int32 GridHeight = 5;

  virtual void BeginPlay() override;

private:
  /** All placed items, replicated to the owning client only. */
  UPROPERTY(ReplicatedUsing = OnRep_Items)
  TArray<FFPMInventoryItem> Items;

  /** All equipped items, replicated to the owning client only. */
  UPROPERTY(ReplicatedUsing = OnRep_EquippedItems)
  TArray<FFPMEquippedItem> EquippedItems;

  UFUNCTION()
  void OnRep_Items();

  UFUNCTION()
  void OnRep_EquippedItems();

  /**
   * Can an item of size (SzX, SzY) be placed with its top-left at (X, Y)?
   * Checks grid bounds and overlap against all existing items.
   * @param IgnoreIndex  Skip this entry during overlap checks (used for moves).
   */
  bool CanPlace(int32 X, int32 Y, int32 SzX, int32 SzY,
                int32 IgnoreIndex = INDEX_NONE) const;

  /**
   * Find the first open position for an item of size (SzX, SzY).
   * Scans row-by-row, left to right.
   * @return true if a position was found; OutX/OutY are filled.
   */
  bool FindFirstOpen(int32 SzX, int32 SzY, int32 &OutX, int32 &OutY) const;

  /**
   * Return the index into Items[] of the item whose footprint covers (X, Y).
   * Returns INDEX_NONE if the cell is empty.
   */
  int32 FindItemIndexAt(int32 X, int32 Y) const;
};
