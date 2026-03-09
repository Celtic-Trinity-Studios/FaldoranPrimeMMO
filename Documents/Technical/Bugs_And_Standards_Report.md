# FaldoranPrimeMMO – Bugs & Industry Standards Report

Summary of likely bugs and code-standards issues identified in the codebase, plus fixes applied.

---

## 1. Bugs (fixed or documented)

| Location | Issue | Fix |
|----------|--------|-----|
| **FPMHUD.cpp ~693** | `WorldMapTexture->GetPlatformData()->Mips[0]` – `GetPlatformData()` can be null on a newly created transient texture before `UpdateResource()`. | Guard with null check on `GetPlatformData()` and ensure `Mips.Num() > 0` before use. |
| **FPMHUD** | Async lambdas in `RefreshBiomeCache` and `BeginWorldMapGeneration` capture `[this]`. If HUD is destroyed before the task runs/finishes, lambda may touch destroyed object (use-after-free). | Capture `TWeakObjectPtr<AFPMHUD>` and check `IsValid()` before writing to staging data. |
| **FPMInventoryComponent.cpp LoadFromDB** | `Row["item_id"]`, `Row["count"]`, etc. – missing DB columns can cause undefined behavior with `TMap::operator[]` on const map. | Use `Row.Find("key")`, check for null, skip or default the row. |
| **FPMInventoryComponent.cpp equipment load** | Same as above for `EqRow["slot"]`, `EqRow["item_id"]`, etc. | Use `EqRow.Find("key")` and validate before use. |
| **FPMPlayerCharacter.cpp HandleMovementInput** | Static locals `bMMBWasDown`, `bTabWasDown`, `bKeyWasDown[6]`, `bLMBWasDown`, `bEKeyWasDown` – shared across all instances. With multiple local players (e.g. split screen), input handling would be wrong. | Replace with member variables on the character (or controller). |
| **FPMInventoryGridWidget BuildUI** | If widget is shown before `InitializeInventory()`, `InventoryComp` can be null; grid uses fallback 8×5 but equip slots get null inventory. | Early return in `BuildUI()` when `!InventoryComp` to avoid building a broken layout. |
| **FPMGameMode.cpp ~78–80** | Redundant `if (OwningPawn)` – control flow could be simplified. | **Fixed:** Single `if (OwningPawn)` block; `PawnLoc` computed inside. |

---

## 2. Standards / quality (addressed or recommended)

| Category | Finding | Status |
|----------|---------|--------|
| **Const correctness** | `FPMHUD::DrawHUDLine` was `const` but used `const_cast` to call `DrawText`. | **Fixed:** `DrawHUDLine` made non-const; `const_cast` removed. |
| **Magic numbers** | Many literals for layout, thresholds, timing. | **Fixed:** `DefaultMaxStackSize` (99) in inventory; `BiomeScanStepCm` / `BiomeHeightNormScale` in HUD; terraform radii remain constexpr in function. |
| **Duplication** | Equip slot label duplicated in two widgets; max stack 99 repeated. | **Fixed:** `FPMGetEquipSlotDisplayName(EFPMEquipSlot)` in `FPMInventoryComponent`; both widgets use it. `DefaultMaxStackSize` used throughout inventory. |
| **Error handling** | `SaveToDB` only logged on failure; caller could not react. | **Fixed:** `SaveToDB` returns `bool`; GameMode logs when it returns false. |
| **World map failure** | No explicit failure path if texture/platform data invalid. | **Fixed:** Platform data/mips checked; on failure `bWorldMapPending=true`, `bWorldMapReady=false`. |
| **GEngine / UFont** | `FPMHUD` used `GEngine` and font getters without null checks. | **Fixed:** `DrawHUD()` and helpers check `GEngine`; font use guarded with `if (Font)` where needed. |

---

## 3. Positive patterns already in use

- **Replication**: Inventory uses `COND_OwnerOnly` with `ReplicatedUsing` and `OnRep_*` broadcasting delegates; server RPCs for moves/splits/equip.
- **Authority checks**: Inventory/equipment APIs check `GetOwnerRole() != ROLE_Authority` and log on client.
- **Atomicity**: `AddItem` rolls back stack deltas if `FindFirstOpen` fails.
- **Validation**: `Server_MoveAndRotateItem_Implementation` validates rotation size.
- **Weak refs in timers**: PlayerController and Character use `TWeakObjectPtr` in timer lambdas and clear timers when invalid.
- **DB parsing**: PlayerController uses `Row.Find()` and pointer check before use; same pattern applied to inventory LoadFromDB.
- **Naming**: Unreal prefixes (A/U/F) used consistently.

---

## 4. References

- Exploration and fixes based on codebase review (see agent transcript).
- Unreal coding standards: [Epic’s C++ Coding Standard](https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/) and engine-style naming (F/U/A prefixes, `UPROPERTY`/`UFUNCTION`).
