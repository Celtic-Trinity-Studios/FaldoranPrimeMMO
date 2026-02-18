# Next Steps & Agent Prompts

This document contains "Ready-to-Use" prompts for the upcoming development phases.
**Usage:** Copy and paste the relevant block into a new agent chat to begin that phase.

---

## 🔵 PHASE II: The Living World (Current Focus)

### Prompt 2.1: Terrain 2.0 — "Extreme Verticality"
**Goal:** Upgrade the procedural terrain generation to support massive mountain peaks and deep valleys.

```text
CONVERSATION TITLE: Phase 2.1 - Extreme Verticality (Terrain 2.0)

CONTEXT:
We are building a "Light No Fire" inspired MMO. We have a basic chunk-based C++ terrain system (FPMChunkData, FPMChunkActor).
The current terrain is too flat ("rolling hills"). We need to upgrade it to "Alpine Peaks".

REFERENCES:
- Read Documents/MasterPlan.md (Phase II, Step 2.1)
- Read Documents/Workflows/Archive/Legacy_Plans/Phase_Refactor_Plan.md (Step 2 details)

TASK:
Refactor the FPMTerrainGenerator::CalculateHeight function to implement "Ridge Noise" for sharp mountain peaks.

REQUIREMENTS:
1. Modify the noise algorithm: Use `1.0 - abs(Perlin)` to create sharp ridges instead of smooth hills.
2. Tune scales: Increase `MaxHeightCm` to allow for 1km+ verticality (e.g., -200m ocean to +1200m peaks).
3. Biome Logic: Update `AssignBiomeFromNoise` to handle steep slopes.
   - Slopes > 45 degrees should be ROCK (regardless of noise biome).
   - High altitude (> 800m) should be SNOW.
4. Update the noise settings in `FPMChunkData.cpp` to these new "extreme" values.

Validation:
- Run PIE.
- The terrain should look dramatic and mountainous, not like a golf course.
```

### Prompt 2.2: Biome Population (PCG)
**Goal:** Populate the empty terrain with dense forests and rocks using PCG Framework.

```text
CONVERSATION TITLE: Phase 2.2 - Biome Population (PCG)

CONTEXT:
We have a chunk-based procedural terrain. It is currently empty mesh. We need to add density: trees, rocks, and grass.
We will use Unreal Engine 5.4's PCG Framework (not legacy foliage painting).

REFERENCES:
- Read Documents/MasterPlan.md (Phase II, Step 2.2)

TASK:
Create a PCG Graph that populates the generated chunks.

REQUIREMENTS:
1. Create a PCG Graph asset `PCG_BiomeSpawner`.
2. Input: The PCG graph should take the underlying mesh surface as input.
3. Density Logic:
   - High density in Forest biome areas (green vertex color).
   - Zero density in Ocean/Coast.
   - Sparse rocks in Mountain biome.
4. Integration:
   - Modify `AFPMChunkActor` to include a `UPCGComponent`.
   - On chunk generation complete, trigger the PCG component to generate.
   - Ensure PCG generation happens *after* the collision mesh is cooked.

Validation:
- PIE -> Trees and rocks appear automatically on the chunks.
```

### Prompt 2.3: Visual Integration & Login
**Goal:** Make the login screen a window into this new living world.

```text
CONVERSATION TITLE: Phase 2.3 - Visual Login Showcase

CONTEXT:
We have a Login Screen (FPMLoginWidget) that is currently using static images or an old map.
We want the login screen to be a live 3D view of the new "Extreme Verticality" terrain.

REFERENCES:
- Read Documents/MasterPlan.md (Phase II, Step 2.3)

TASK:
Update the Login Level and Widget to showcase the living world.

REQUIREMENTS:
1. Create a "Showcase" setup in the `L_StarterIsland` (or a dedicated `L_Login` map that uses the chunk system).
2. Camera:
   - Place a `CameraRig_Rail` or a slow-panning camera script.
   - Frame a dramatic view: Looking up at a mountain peak from a forest valley at sunset.
3. UI Update:
   - Ensure the Login Widget background is transparent/glassmorphism so the 3D world is visible.
   - Hide the HUD/Compass during login state.
4. Lighting:
   - Tune the Directional Light / Sky Atmosphere for a "Golden Hour" look.

Validation:
- Launch Game -> Login Screen appears.
- Background is a live, moving 3D world, not a static image.
```

---

## 🟡 PHASE III: Character Depth

### Prompt 3.1: The "Species" Refactor
**Goal:** Allow players to play as Giants or Smallfolk using the same skeleton.

```text
CONVERSATION TITLE: Phase 3.1 - Species Refactor

CONTEXT:
We are using CC5 characters with a standard UE5 mannequin skeleton.
 We want to support "Species" that vary wildly in size (e.g., Half-Giant = 1.5x scale, Gnome = 0.5x scale) but share animations.

REFERENCES:
- Read Documents/MasterPlan.md (Phase III, Step 3.1)

TASK:
Implement the `DA_Species` data asset and scaling logic.

REQUIREMENTS:
1. Create `UDA_Species` (Data Asset) with fields:
   - `SpeciesName` (Human, Elf, Giant, Dwarf)
   - `MeshScale` (float, e.g., 1.0, 1.5, 0.7)
   - `WalkSpeedMultiplier` (float)
   - `CapsuleHalfHeight` (float)
   - `DefaultMorphs` (Map of Name -> Value)
2. Update `AFPMCharacter`:
   - Add `SpeciesID` or `DA_Species` reference.
   - On `BeginPlay`/`OnRep_Species`, apply the scale to the Mesh and Capsule Component.
   - Adjust `CameraBoom->TargetArmLength` based on scale.
   - Adjust `CharacterMovement->MaxWalkSpeed`.
3. Create Data Assets for: Human, Giant, Dwarf.

Validation:
- Spawn as a Giant.
- Verify camera is higher.
- Verify running animation works (even if foot sliding exists, we fix that later).
```

### Prompt 3.2: Animation Pipeline (CC5)
**Goal:** Get the character moving with a full locomotion set.

```text
CONVERSATION TITLE: Phase 3.2 - CC5 Animation Pipeline

CONTEXT:
We have `AFPMCharacter` with a CC5 mesh. It currently only idles.
We need a full locomotion state machine.

REFERENCES:
- Read Documents/MasterPlan.md (Phase III, Step 3.2)
- Read Documents/Workflows/Reference/Guide_CC5_Pipeline.md

TASK:
Implement the animation blueprint and state machine.

REQUIREMENTS:
1. Assets: Use the Mixamo or ActorCore animations (Idle, Walk, Run, Jump).
2. Retargeting: Ensure IK Retargeters are correctly set up for the CC5 Skeleton.
3. Animation Blueprint (`ABP_CC5_Character`):
   - State Machine: Idle -> Walk -> Run (based on Speed).
   - Jumping: JumpStart -> JumpLoop -> JumpLand.
   - BlendSpaces: Create 1D BlendSpace for movement speed.
4. Apply to `AFPMCharacter`.

Validation:
- Play as character.
- Walk, Run, Jump smoothly.
```

### Prompt 3.3: Full Creation UI & Affinities
**Goal:** A polished UI to create your unique character.

```text
CONVERSATION TITLE: Phase 3.3 - Character Creation UI

CONTEXT:
We have a layout for character creation and the backend data contracts.
We need to connect the UI sliders to the actual backend and the 3D preview actor.

REFERENCES:
- Read Documents/MasterPlan.md (Phase III, Step 3.3)
- Read Documents/Workflows/Reference/Guide_Database_Setup.md (for affinity tables)

TASK:
Build the Tabbed Character Creation UI.

REQUIREMENTS:
1. UI Layout: Update `WBP_CharacterCreation` to have tabs:
   - [Race] (Selects DA_Species)
   - [Appearance] (Sliders for Morphs: Height, Muscle, Face)
   - [Style] (Hair, Skin Color, Eye Color)
   - [Affinities] (Point allocation for Playstyle/Magic)
2. Affinity Logic:
   - Implement "Point Pool" logic (e.g., 10 points to spend on Magic).
   - Sliders update the `FPMCharacterCreationRequest`.
3. Preview Connection:
   - Dragging Appearance sliders instantly calls `PreviewActor->SetMorphValue`.
4. Submit:
   - "Create" button bundles all data (Appearance + Affinities) and sends to `SubmitCharacterCreation` RPC.

Validation:
- Create a character with custom looks and affinities.
- Log in.
- Verify character in-game matches the creation screen.
```

---

## ⚪ PHASE IV: Gameplay Loop

### Prompt 4.1: Gameplay Foundation
**Goal:** Inventory, Gathering, and Interaction.

```text
CONVERSATION TITLE: Phase 4.1 - Gameplay Foundation

CONTEXT:
We have a world and a character. Now we need to DO things.

REFERENCES:
- Read Documents/MasterPlan.md (Phase IV)

TASK:
Implement the base Inventory and Interaction system.

REQUIREMENTS:
1. Interaction Interface: `IInteractionInterface` (Interact(), GetInteractText()).
2. Inventory Component: `UFPMInventoryComponent` (Server-authoritative, Replicated).
   - TArray<FInventoryItem>
   - Functions: AddItem, RemoveItem, SwapItem.
3. Test Item: Create a "Loose Rock" actor interactable.
   - On Interact -> Adds "Rock" to inventory -> Destroys world actor.
4. UI: Basic Inventory Grid (GridPanel) showing icons.

Validation:
- Walk up to a rock.
- Press 'E'.
- Rock disappears.
- Open Inventory (I).
- See "Rock x1".
```
