# Agent Prompt Library

Ready-to-paste prompts for each pending task.  
🤖 = agent task | 👤 = manual editor step

---

## STEP 1 — Test Water & Interaction (👤 15 min)

**1A. Test Water Fix**
1. PIE → F key to fly → find a river
2. Look for teal water in carved channels
3. If wrong: adjust `RiverBedHeight` in `Config/WorldGen.ini` (no recompile needed)

**1B. Test Interaction System**
1. Place Actors → `FPMInteractableResource` → drag into level → assign any mesh
2. Play → walk to object → verify `[E] Pick up Rock` appears
3. Press E → object disappears; press I → Output Log: `"Inventory has 1/40 slots in use"`

---

## STEP 2 — PCG World Population (👤)

**2A. Import Meshes**
- Trees → `Content/Environment/Trees/` (2–3 varieties, with LODs)
- Rocks → `Content/Environment/Rocks/` (2–3 varieties)

**2B. Create PCG Config Data Asset**
1. Content Browser → Right-click → Data Asset → `UFPMBiomePCGConfig` → name `DA_BiomePCGConfig`
2. Fill biome arrays: Meadows (grass, sparse trees), Forest (dense trees), Mountains (rocks)
3. Select `AFPMWorldChunkManager` → assign `DA_BiomePCGConfig` → Play and verify

---

## STEP 3 — Species Data Assets (👤 + 🤖)

**3A. Create Data Assets (👤)**
1. Data Asset → `UFPMSpeciesDataAsset` — one per species:

| Asset | Species | MeshScale | CapsuleHH | CapsuleR | WalkSpeed | BoomLen |
|-------|---------|-----------|-----------|----------|-----------|---------|
| DA_Species_Human | Human | 1.0 | 90 | 34 | 1.00 | 400 |
| DA_Species_HalfElf | HalfElf | 1.05 | 94 | 34 | 1.02 | 420 |
| DA_Species_Elf | Elf | 1.08 | 97 | 32 | 1.05 | 440 |
| DA_Species_Dwarf | Dwarf | 0.80 | 72 | 38 | 0.90 | 320 |
| DA_Species_Halfling | Halfling | 0.65 | 58 | 30 | 0.85 | 260 |
| DA_Species_HalfOrc | HalfOrc | 1.15 | 103 | 40 | 0.95 | 460 |
| DA_Species_Gnome | Gnome | 0.55 | 50 | 26 | 0.80 | 220 |
| DA_Species_Kethari | Kethari | 0.95 | 86 | 32 | 1.10 | 380 |
| DA_Species_Rauken | Rauken | 1.10 | 99 | 38 | 1.05 | 440 |

2. Data Asset → `UFPMSpeciesRegistry` → add all 9 species assets to `Species` array

**3B. Wire Species to Player Character (🤖)**
```
CONVERSATION TITLE: Wire Species Data Assets to Player Character

CONTEXT:
UFPMSpeciesDataAsset and UFPMSpeciesRegistry exist in Public/Character/FPMSpeciesDataAsset.h.
AFPMPlayerCharacter has ApplySpeciesScaling() reading from a static FSpeciesScaling array.

TASK:
Refactor AFPMPlayerCharacter to read from UFPMSpeciesRegistry instead of the static array.

REQUIREMENTS:
1. Add UPROPERTY for UFPMSpeciesRegistry (or look up via GameInstance/WorldChunkManager)
2. Modify ApplySpeciesScaling() to read MeshScale, CapsuleHalfHeight, CapsuleRadius,
   WalkSpeedMultiplier, CameraBoomLength, DefaultMorphTargets from the data asset
3. Keep old static array as fallback
4. Build Editor + Server targets — both must succeed

Validation: Build succeeds. Human species works identically to before.
```

---

## STEP 4 — Inventory UI (🤖)

```
CONVERSATION TITLE: Build Inventory UI Widget

CONTEXT:
UFPMInventoryComponent (Public/Gameplay/FPMInventoryComponent.h):
- TArray<FFPMInventorySlot> Slots (40 slots, 8x5 grid)
- FFPMOnInventoryChanged delegate
- GetSlots(), GetSlotCount(), SwapSlots(), AddItem(), RemoveItem()
- Replicated COND_OwnerOnly

AFPMPlayerCharacter has ToggleInventory(), bInventoryOpen, InventoryComponent.

TASK: Create UMG inventory widget wired to the player character.

REQUIREMENTS:
1. UFPMInventoryWidget (UUserWidget) in Public/UI/FPMInventoryWidget.h
2. Layout: 8 columns × 5 rows grid
3. Each slot: item icon, stack count (hidden if ≤1), rarity-colored border
4. Bind to UFPMInventoryComponent::OnInventoryChanged
5. Drag-and-drop between slots (calls SwapSlots)
6. Modify ToggleInventory() to create/show/hide this widget
7. Style: dark semi-transparent, rounded borders, subtle hover glow
8. Build Editor + Server targets

Validation: I key shows grid. Pick up rock → slot shows "Rock x1". Drag works.
```

---

## STEP 5 — Animation Blueprint (👤 + 🤖)

**5A. Import Animations (👤)**
- Mixamo.com: Idle, Walk, Run, Jump Start, Jump Loop, Jump Land
- Import to `Content/Characters/CC5/Animations/`
- IK Retarget if source skeleton differs from CC5

**5B. Create Animation Blueprint (👤)**
1. Right-click → Animation → Animation Blueprint → parent `UFPMCharacterAnimInstance`, skeleton CC5
2. Name: `ABP_CC5_Character`
3. State Machine: Idle → Walk/Run (Speed > 10) → JumpStart (bIsJumping) → JumpLoop → JumpLand → Idle

**5C. Assign AnimBP to Player Character (🤖)**
```
CONVERSATION TITLE: Assign Animation Blueprint to Player Character

CONTEXT:
UFPMCharacterAnimInstance (C++ anim instance base) exists.
ABP_CC5_Character created in editor.
Player currently uses AnimationSingleNode mode.

TASK: Switch AFPMPlayerCharacter to use AnimationBlueprint mode.

REQUIREMENTS:
1. Change animation mode to AnimationBlueprint in constructor
2. Set AnimBP class on the skeletal mesh component
3. Remove CC5IdleAnimation single-node playback
4. Build Editor + Server targets

Validation: Walk/run trigger correct animations. Jump animations work.
```

---

## STEP 6 — Login Screen Showcase (🤖)

```
CONVERSATION TITLE: Login Screen Showcase

CONTEXT:
FPMLoginWidget exists (login UI). AFPMLoginCinematicCamera + AFPMLoginLevelSetup compiled.
Login screen currently shows static/default background.

TASK: Set up cinematic 3D world view behind login UI.

REQUIREMENTS:
1. Create/modify Login level — spawn AFPMWorldChunkManager
2. Slow-panning camera over mountain + forest at golden hour
3. FPMLoginWidget background: glassmorphism/transparent so 3D world shows through
4. Directional Light: golden hour angle (sun near horizon, warm tone)
5. Build both targets

Validation: Game launch → login screen with live 3D world behind UI.
```

---

## STEP 7 — Character Creation UI (🤖)

```
CONVERSATION TITLE: Character Creation UI

CONTEXT:
FFPMCharacterCreationRequest, AFPMCharacterPreviewActor (morph target support),
UFPMCharacterCreationSubsystem, UFPMSpeciesDataAsset,
EFPMPlaystyleAffinity (6 affinities, 600 pts), EFPMMagicalAffinity (8 affinities, 800 pts)

TASK: Build polished tabbed character creation UI.

REQUIREMENTS:
1. UFPMCharacterCreationWidget tabs:
   - [Race] species grid with icons + lore
   - [Body] sliders: Height, Muscle, Weight
   - [Face] sliders: Jaw, Nose, Brow, Lips
   - [Style] color pickers: Skin, Eye, Hair + hair style selector
   - [Affinities] point allocation (6 Playstyle + 8 Magical, pool counters)
2. All sliders instantly update 3D preview via SetMorphValue
3. Species selection applies species morphs, scales preview
4. "Create Character" bundles into FFPMCharacterCreationRequest
5. Style: dark theme, gradient headers, glow on active tab
6. Build both targets

Validation: Create character with custom species/appearance/affinities. Real-time preview. Submit transitions to world.
```

---

## STEP 8 — Crafting System (🤖)

```
CONVERSATION TITLE: Crafting System

CONTEXT:
UFPMInventoryComponent has AddItem/RemoveItem/HasItem.
AFPMInteractableResource handles gathering. Items identified by FName ItemID.

TASK: Implement data-driven crafting.

REQUIREMENTS:
1. FFPMCraftingRecipe struct: TArray<FName,int32> Inputs, FName OutputItemID, int32 OutputCount, float CraftTime
2. UFPMCraftingComponent (player): KnownRecipes, CanCraft(recipe), Craft(recipe) server-authoritative
3. Simple crafting UI showing available recipes
4. Starter recipes: 3 Rock→Stone Axe · 5 Wood→Wooden Shield · 2 Rock+3 Wood→Campfire
5. Build both targets

Validation: Gather rocks/wood → open crafting UI → craft Stone Axe → inputs consumed, output in inventory.
```

---

## STEP 9 — Building System (🤖)

```
CONVERSATION TITLE: Building System Foundation

CONTEXT:
Inventory, crafting, and interaction systems exist.

TASK: Basic building placement system.

REQUIREMENTS:
1. AFPMPlaceableStructure base class (replicated)
2. UFPMBuildingComponent (player):
   - EnterBuildMode(StructureClass) — ghost preview
   - Ghost follows cursor (camera line trace), snaps to grid
   - Green=valid, Red=invalid (collision/slope check)
   - Left Click = place (spawn actor, consume inventory items)
   - Right Click/Escape = cancel
3. 3 test structures: Campfire, Wooden Wall, Foundation
4. Server-authoritative: client sends request, server validates + spawns
5. Build both targets

Validation: Craft campfire → build mode → ghost preview → place on ground → visible to all players.
```
