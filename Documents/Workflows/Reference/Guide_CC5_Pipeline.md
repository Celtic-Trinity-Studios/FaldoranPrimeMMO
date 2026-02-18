# Pillar 04 — Character Creation with CC5

**Goal:** Replace the UE5 mannequin with fully customizable CC5 (Character Creator 5) characters. Players design their character's appearance, race, and animations in a polished creation screen before entering the game world.

**Status:** Phase 4A Complete ✅ | Phase 4B Planned
**Dependencies:** Pillar 01 (Affinity System + Data Contracts) ✅

---

## What is CC5?

**Character Creator 5** (by Reallusion) is a standalone character creation tool that produces game-ready 3D characters with:
- **HD morphs** — thousands of body/face sliders for infinite variation
- **ActorMIXER** — blend up to 6 characters to create unique looks
- **Subdivision levels** — Lv0 (game-ready), Lv1 (balanced), Lv2 (cinematic)
- **UE5 skeleton preset** — exports with bones matching UE5 Mannequin / MetaHuman
- **Automatic LOD** — polygon reduction + texture atlasing for game performance

**You will use CC5 to:**
1. Create base character templates (male/female/race variants)
2. Export them as FBX with the UE5 skeleton preset
3. Import into UE5 via the Reallusion Auto Setup plugin
4. At runtime, apply morph targets + material parameters to customize appearance

---

## Software & Plugins Required

| Tool | Purpose | Where to Get |
|------|---------|-------------|
| **Character Creator 5** | Create base character models | [reallusion.com/character-creator](https://www.reallusion.com/character-creator/) (paid license) |
| **iClone 9** (optional) | Animation preview, motion capture, facial animation | [reallusion.com/iclone](https://www.reallusion.com/iclone/) |
| **Reallusion Auto Setup (All-in-One)** | UE5 import plugin — handles materials, rigs, control rigs | [reallusion.com/auto-setup](https://www.reallusion.com/auto-setup/) (free) |
| **ActorCore** (optional) | High-quality motion capture animation library | [actorcore.reallusion.com](https://actorcore.reallusion.com/) |
| **Mixamo** (free alternative) | Animation library + auto-rigging | [mixamo.com](https://www.mixamo.com/) |

---

## Phase 1: CC5 Setup & Base Character Export

### Step 1.1 — Install Character Creator 5
1. Purchase and download CC5 from Reallusion
2. Install and activate your license
3. Launch CC5 — you'll see the character workspace with a default base

### Step 1.2 — Create Base Character Templates
Create **at minimum 2 base characters** (male body, female body). These serve as the starting point that players customize at runtime using morph sliders.

**In CC5:**
1. Start with a CC5 Base character (not CC3/CC4 legacy)
2. **Body shape:** Set to a neutral/average build (players will morph at runtime)
3. **Face shape:** Set to a neutral base that morphs well
4. **Skin:** Apply a high-quality skin texture (CC5 includes several PBR skin presets)
5. **Eyes:** Use the HD eye system with tear ducts
6. **Hair:** Create a "bald" base (hair will be swapped via separate meshes at runtime)
7. **Clothing:** Export without clothing (clothing is separate equipment in-game)

### Step 1.3 — Set Up Morph Targets for Runtime Customization

**Critical step!** These morphs will be adjustable by the player in the creation screen.

In CC5, go to **Modify > Morph** and ensure these morph categories are included:
- **Body:** Height, weight, muscle mass, proportions (arm length, leg length, torso)
- **Face:** Jaw width, chin length, nose size/bridge/tip, eye spacing/size, lip fullness, cheekbones, brow ridge, ear size
- **Age:** Wrinkle intensity, skin sag
- **Race/Archetype:** Set up custom morph profiles for your fantasy races (Elf ears, Orc jaw, Dwarf proportions, etc.)

> **Important:** All morphs you want players to adjust must be included in the FBX export. In the export dialog, check "Include Morph Targets."

### Step 1.4 — Export from CC5

1. Select your character in CC5
2. Go to **File > Export > FBX > Clothed Character** (even if unclothed)
3. In the Export FBX panel:
   - **Target Tool Preset:** Select `UE 5 (Skeleton)`
   - **Subdivision Level:** Select `SubD 1` (balanced quality/performance)
   - **Include Morph Targets:** ✅ YES (this is critical!)
   - **Include Motion:** Optional — add an idle animation for testing
4. Click the **gear icon** (Advanced Settings):
   - Confirm `UE5 Skeleton` is selected
   - Confirm `A-Pose for UE5 Skeleton` is the bind pose
5. Click **Export** — save as `CC5_Base_Male.fbx` and `CC5_Base_Female.fbx`

---

## Phase 2: Import into Unreal Engine 5

### Step 2.1 — Install Reallusion Auto Setup Plugin

1. Go to [reallusion.com/auto-setup](https://www.reallusion.com/auto-setup/)
2. Download the **Auto Setup (All-in-One)** package for your UE5 version
3. Extract the downloaded archive
4. Copy the `Plugins` folder into your project root: `E:\FaldoranPrimeMMO\Plugins\`
5. Copy the `Content` folder into your project: `E:\FaldoranPrimeMMO\Content\`
6. Open the UE5 editor
7. Go to **Edit > Plugins**, search for "Character Creator"
8. Enable **"CC & iClone Auto Setup"** and **"CC UE Control Rig"**
9. Restart the editor
10. Verify: a **"CC Setup"** button should appear in the toolbar

### Step 2.2 — Import the FBX Characters

1. In Content Browser, create folder: `Content/Characters/CC5/`
2. Drag and drop `CC5_Base_Male.fbx` into the folder
3. The Auto Setup import dialog appears:
   - **Shader:** Select `HQ Shader` for best quality
   - **Use T0 As Ref Pose:** ✅ UNCHECK this (important for CC5!)
   - **Import Morph Targets:** ✅ CHECK (we need these for runtime customization)
   - **Import Animations:** ✅ CHECK if you included motions
4. Click **Import**
5. Auto Setup automatically creates:
   - Skeletal Mesh with correct materials
   - Skeleton asset compatible with UE5 Mannequin
   - Material instances with Reallusion's HQ shaders
   - Morph targets accessible in the editor
6. Repeat for the female base character

### Step 2.3 — Verify the Import

1. Open the imported Skeletal Mesh asset
2. In the **Morph Target** preview panel, verify your morphs are listed
3. Drag sliders to test — face/body should deform correctly
4. Check materials — skin, eyes, teeth should look correct
5. If anything looks wrong, re-export from CC5 with different settings

---

## Phase 3: Animation Pipeline

### Step 3.1 — Animation Sources

You have three main options for character animations:

**Option A: ActorCore (Recommended for quality)**
1. Go to [actorcore.reallusion.com](https://actorcore.reallusion.com/)
2. Browse motion packs: combat, idle, locomotion, emotes
3. Download motions with **Target: Unreal 5** selected
4. Import FBX animations into UE5 under `Content/Characters/CC5/Animations/`

**Option B: Mixamo (Free, good for prototyping)**
1. Go to [mixamo.com](https://www.mixamo.com/)
2. Upload your CC5 character FBX (T-pose, no animation)
3. Browse and download animations (FBX format, "Without Skin")
4. Import into UE5 — assign the CC5 skeleton when prompted

**Option C: iClone 9 (Best for custom animations)**
1. Open iClone 9, import your CC5 character
2. Use motion capture, keyframe animation, or motion library
3. Export animations as FBX with UE5 skeleton target
4. Import into UE5

### Step 3.2 — Create IK Rigs for Retargeting

If your animation source skeleton differs from the CC5 UE5 skeleton:

1. **Create Source IK Rig:**
   - Right-click the source skeleton > Create > IK Rig
   - Define root bone, retarget chains (Spine, LeftArm, RightArm, LeftLeg, RightLeg, Head)
   - Set IK goals for hands and feet

2. **Create Target IK Rig (CC5 character):**
   - Right-click the CC5 skeleton > Create > IK Rig
   - Define matching retarget chains
   - Map bone names to match the CC5 hierarchy

3. **Create IK Retargeter:**
   - Right-click > Animation > IK Retargeter
   - Set Source = animation source IK Rig
   - Set Target = CC5 character IK Rig
   - Preview and adjust offsets until animations look correct

### Step 3.3 — Essential Animation Set

At minimum, you need these animations for the character creation screen and gameplay:

**Character Creation Screen:**
| Animation | Description |
|-----------|-------------|
| `Idle_Standing` | Default idle pose — breathing, slight sway |
| `Idle_LookAround` | Character looks left/right (shows off face morphs) |
| `Turn_Left_90` | Turn in place when player rotates preview |
| `Turn_Right_90` | Turn in place when player rotates preview |
| `Emote_Wave` | Friendly wave (shows off body proportions) |

**Gameplay (Phase 2+):**
| Animation | Description |
|-----------|-------------|
| `Locomotion_Walk` | Walk forward blend space |
| `Locomotion_Run` | Run forward blend space |
| `Locomotion_Sprint` | Sprint forward |
| `Locomotion_Idle` | In-game idle |
| `Jump_Start/Loop/Land` | Jump cycle |
| `Combat_Idle` | Combat stance |

### Step 3.4 — Animation Blueprint

Create `ABP_CC5Character` (Animation Blueprint):

```
AnimGraph:
  ├── State Machine: Locomotion
  │   ├── Idle → Walk (speed > 10)
  │   ├── Walk → Run (speed > 300)
  │   ├── Run → Sprint (speed > 600)
  │   ├── Any → Jump (is_jumping)
  │   └── Any → Idle (speed < 10)
  ├── Slot: UpperBody (for combat/emotes overlay)
  └── Output Pose

Variables:
  - Speed (float, from character movement)
  - IsJumping (bool)
  - Direction (float, for strafe blend)
```

---

## Phase 4: Character Creation Screen (UE5 Implementation)

### Step 4.1 — Preview Scene Setup

Create a dedicated sublevel or scene for character preview:

```
AFPMCharacterCreationScene:
  - USkeletalMeshComponent (CC5 character)
  - USceneCaptureComponent2D (renders to UI texture)
  - UDirectionalLightComponent (key light)
  - UPointLightComponent (fill light)
  - UPointLightComponent (rim light)
  - USkyLightComponent (ambient)
  - UStaticMeshComponent (floor/pedestal)
```

### Step 4.2 — Runtime Morph Target Control

The creation screen applies morphs in real-time:

```cpp
// In AFPMCharacterPreviewActor:
void SetMorphValue(FName MorphName, float Value) {
  if (SkeletalMeshComp) {
    SkeletalMeshComp->SetMorphTarget(MorphName, Value);
  }
}

// Example usage from UI slider:
PreviewActor->SetMorphValue("Jaw_Width", 0.7f);
PreviewActor->SetMorphValue("Nose_Bridge", -0.3f);
PreviewActor->SetMorphValue("Elf_Ears", 1.0f);
```

### Step 4.3 — Material Parameter Control

Skin tone, eye color, hair color via Material Instance Dynamic:

```cpp
void SetSkinTone(FLinearColor Color) {
  if (SkinMID) {
    SkinMID->SetVectorParameterValue("SkinColor", Color);
  }
}

void SetEyeColor(FLinearColor Color) {
  if (EyeMID) {
    EyeMID->SetVectorParameterValue("IrisColor", Color);
  }
}
```

### Step 4.4 — Hair System

Hair as separate skeletal/static mesh attachments:

```cpp
void SetHairStyle(int32 HairIndex) {
  if (HairIndex < HairMeshes.Num()) {
    HairMeshComp->SetSkeletalMesh(HairMeshes[HairIndex]);
  }
}
```

**Hair asset creation in CC5:**
1. In CC5, create several hairstyle variations
2. Export each as a separate FBX (mesh only, no body)
3. Import into UE5 under `Content/Characters/CC5/Hair/`
4. At runtime, swap the hair mesh component

### Step 4.5 — UI Layout

Tabbed creation screen with categories:

```
┌─────────────────────────────────────────────────────┐
│  [Race] [Body] [Face] [Hair] [Skin] [Affinities]   │
├───────────────┬─────────────────────────────────────┤
│               │  ┌──────────────┐                   │
│  Slider:      │  │              │                   │
│  Height ━━━━  │  │   3D Preview │                   │
│  Weight ━━━━  │  │   (rotating) │                   │
│  Muscle ━━━━  │  │              │                   │
│  Age    ━━━━  │  └──────────────┘                   │
│               │                                     │
│  Preset: [Warrior] [Mage] [Rogue]                   │
├───────────────┴─────────────────────────────────────┤
│  [Randomize]              [Name: ________]  [Done]  │
└─────────────────────────────────────────────────────┘
```

---

## Phase 5: Data Persistence & Replication

### Step 5.1 — Character Appearance Data Contract

```cpp
USTRUCT()
struct FFPMCharacterAppearance {
  UPROPERTY() int32 BaseBodyType;        // Male=0, Female=1
  UPROPERTY() int32 RaceIndex;           // Human, Elf, Dwarf, Orc, etc.
  UPROPERTY() TMap<FName, float> Morphs; // All morph target values
  UPROPERTY() FLinearColor SkinTone;
  UPROPERTY() FLinearColor EyeColor;
  UPROPERTY() FLinearColor HairColor;
  UPROPERTY() int32 HairStyleIndex;
  UPROPERTY() int32 FacialHairIndex;     // Beard/mustache
  UPROPERTY() TArray<int32> Tattoos;     // Tattoo layer indices
  UPROPERTY() TArray<int32> Scars;       // Scar overlay indices
};
```

### Step 5.2 — Database Schema

```sql
-- Expanded character appearance storage
ALTER TABLE characters ADD COLUMN base_body_type SMALLINT NOT NULL DEFAULT 0;
ALTER TABLE characters ADD COLUMN race_index SMALLINT NOT NULL DEFAULT 0;
ALTER TABLE characters ADD COLUMN morph_data JSONB NOT NULL DEFAULT '{}';
ALTER TABLE characters ADD COLUMN skin_tone VARCHAR(32) NOT NULL DEFAULT '1.0,0.8,0.7,1.0';
ALTER TABLE characters ADD COLUMN eye_color VARCHAR(32) NOT NULL DEFAULT '0.3,0.5,0.2,1.0';
ALTER TABLE characters ADD COLUMN hair_color VARCHAR(32) NOT NULL DEFAULT '0.2,0.1,0.05,1.0';
ALTER TABLE characters ADD COLUMN hair_style SMALLINT NOT NULL DEFAULT 0;
ALTER TABLE characters ADD COLUMN facial_hair SMALLINT NOT NULL DEFAULT 0;
ALTER TABLE characters ADD COLUMN tattoos SMALLINT[] DEFAULT '{}';
ALTER TABLE characters ADD COLUMN scars SMALLINT[] DEFAULT '{}';
```

### Step 5.3 — Replication

On spawn, the server sends `FFPMCharacterAppearance` to all clients who need to see this character. Clients apply the morphs + materials locally.

```cpp
// On AFPMPlayerCharacter:
UPROPERTY(ReplicatedUsing = OnRep_Appearance)
FFPMCharacterAppearance Appearance;

void OnRep_Appearance() {
  ApplyAppearance(Appearance);  // Sets morphs, materials, hair
}
```

---

## Implementation Order

### Phase 4A: CC5 Base Setup ✅ COMPLETE (2026-02-13)
```
1. Purchase/install Character Creator 5                              ✅
2. Install Reallusion Auto Setup plugin in UE5 project               ✅ (RLPlugin v2.0)
3. Create 2 base characters (male/female) in CC5                     ✅ (CC3+ Base, SubD 0)
4. Export with morph targets + UE5 skeleton preset                    ✅ (UE5 Skeleton, A-Pose)
5. Import into UE5, verify morphs work in editor                     ✅ (240 facial morphs)
6. Create basic Animation Blueprint with idle animation               ✅ (ActorCore idle)
```

**Phase 4A Notes:**
- **Base type used:** CC3+ (game-ready ~30K polys, NOT CC5 HD)
- **Plugin:** `Plugins/RLPlugin/` — Reallusion Auto Setup v2.0 (All-in-One)
- **Shaders:** `Content/CC_Shaders/` — HQ Shader auto-configured by plugin
- **Male assets:** `Content/Characters/CC5/CC5_Base_Male` (Skeletal Mesh + Skeleton)
- **Female assets:** `Content/Characters/CC5/CC5_Base_Female` (Skeletal Mesh + Skeleton)
- **Animation BPs:** `ABP_CC5_Male`, `ABP_CC5_Female` (idle only for now)
- **Idle animations:** Sourced from ActorCore (free pack), UE5 Skeleton target
- **Morph targets (240):** ALL are facial animation blendshapes (ARKit, visemes, expressions). NO body/face SHAPE morphs exported. Character customization morphs (height, jaw width, nose size, etc.) will need custom CC5 morph export or runtime bone scaling in Phase 4B.
- **Raw exports:** `RawAssets/CC5/` (FBX + JSON + textures)

### Phase 4B: Character Preview Actor ✅ COMPLETE (2026-02-13)
```
1. Create AFPMCharacterPreviewActor with CC5 skeletal mesh            ✅
2. Set up preview lighting scene                                      ✅ (3-point + ambient sky)
3. Implement SetMorphValue(), SetSkinTone(), SetEyeColor(),
   SetHairColor(), SetHairStyle()                                     ✅
4. Camera orbit controls (drag to rotate, scroll to zoom,
   double-click to reset)                                             ✅
5. Wire to existing creation widget — sliders drive morphs            ✅
```

**Phase 4B Notes:**
- **Mesh:** Switched from UE5 Mannequin to `Content/Characters/CC5/CC5_Base_Male` with graceful fallback
- **Animation:** Auto-assigns `ABP_CC5_Male` Animation Blueprint (idle from Phase 4A)
- **Morph API:** Generic `SetMorphValue(FName, float)` wraps `SetMorphTarget()` — works with all 240 CC5 morphs
- **Materials:** Per-slot Dynamic Material Instances (SkinMID, EyeMID, HairMID) with keyword-based slot discovery
- **Eye Color:** New `SetEyeColor()` + 3 optional eye color sliders in widget (BindWidgetOptional)
- **Hair System:** Separate `USkeletalMeshComponent` for hair mesh swaps via `SetHairStyle(int32)`
- **Lighting:** 4 lights — Key (warm, front-right), Fill (cool, front-left), Rim (back), Ambient (USkyLightComponent)
- **Camera:** SpringArm orbit via mouse drag, scroll zoom, `ResetCameraToFront()` on double-click
- **Morph Sliders:** 4 optional CC5 morph sliders in widget (JawWidth, NoseBridge, BrowRidge, LipFullness)
- **Line Counts:** All 4 files under 500-line limit (max 492 lines in widget .cpp)
- **Client-only:** `bReplicates=false`, spawned far off-world at (-50000,-50000,-5000)

### Phase 4C: Animation Set
```
1. Acquire base animation set (Mixamo or ActorCore)
2. Set up IK Rigs + IK Retargeter if needed
3. Create Animation Blueprint (idle + locomotion states)
4. Preview character plays idle animation in creation screen
```

### Phase 4D: Full Creation UI
```
1. Build tabbed UI (Race, Body, Face, Hair, Skin, Affinities)
2. Populate slider controls from morph target names
3. Add color pickers for skin/eye/hair
4. Hair mesh selection gallery
5. Race presets (apply batch of morphs for Elf/Dwarf/Orc/etc.)
6. Randomize button
7. Name input + validation
```

### Phase 4E: Persistence & Replication
```
1. Update FFPMCharacterCreationRequest with appearance data
2. Update database schema (morph_data JSONB, colors, indices)
3. Server validation of appearance values
4. Client replication of appearance on spawn
5. Other players see your character correctly
```

---

## Agent Prompts — Pillar 4

### Phase 4A — CC5 Base Setup
```
CONVERSATION TITLE: Pillar 04, Phase 4A — CC5 Base Character Setup

Read Documents/Design/00_Rules_and_Constraints.md for project rules.
Read Documents/Workflows/Pillar_04_CC5_Character_Creation.md for the full plan.

This is a GUIDED SETUP phase. Help me step-by-step through:
1. Installing the Reallusion Auto Setup plugin into the UE5 project
   - I have the plugin downloaded
   - Walk me through copying files to the right directories
   - Verify the plugin loads correctly

2. Importing my CC5 FBX character into UE5
   - Correct import settings (HQ Shader, morph targets ON, T0 ref pose OFF)
   - Verify morph targets appear and work correctly
   - Verify materials/textures look correct

3. Creating a basic Animation Blueprint
   - Idle animation playing on the imported character
   - Verify the character can be spawned and renders correctly

Follow all rules in 00_Rules_and_Constraints.md.
```

### Phase 4B — Character Preview
```
CONVERSATION TITLE: Pillar 04, Phase 4B — Character Preview Actor

Read Documents/Design/00_Rules_and_Constraints.md for project rules.
Read Documents/Workflows/Pillar_04_CC5_Character_Creation.md for the full plan.

TASK: Build the character creation preview system using CC5 models.

Prerequisites: Phase 4A complete. CC5 character imported with morph targets.

Micro-steps (each must compile):
1. Create AFPMCharacterPreviewActor:
   - USkeletalMeshComponent using imported CC5 mesh
   - Plays idle animation
   - SetMorphValue(FName, float) for any morph target
   - SetSkinTone(FLinearColor), SetEyeColor(), SetHairColor()
   - SetHairStyle(int32) for mesh swaps
2. Preview scene lighting:
   - Key light, fill light, rim light, ambient
   - Dark background or stylized environment
3. Camera orbit:
   - Mouse drag rotates character
   - Scroll zooms in/out
   - Double-click resets to front view
4. Wire to creation widget:
   - Sliders change morphs in real-time
   - Color pickers update materials in real-time
5. Compile and test

Follow all rules in 00_Rules_and_Constraints.md. Client-only code.
```

### Phase 4C — Animation Set
```
CONVERSATION TITLE: Pillar 04, Phase 4C — Animation Pipeline

Read Documents/Design/00_Rules_and_Constraints.md for project rules.
Read Documents/Workflows/Pillar_04_CC5_Character_Creation.md for the full plan.

TASK: Set up the animation pipeline for CC5 characters.

Prerequisites: Phase 4B complete. Preview actor works.

This is a GUIDED SETUP phase. Help me:
1. Download animations from Mixamo (or import ActorCore FBX)
   - Essential set: idle, walk, run, jump, combat idle
2. Import into UE5 with correct skeleton assignment
3. Create IK Rigs if retargeting is needed
4. Create IK Retargeter and verify animations play correctly
5. Build Animation Blueprint:
   - State machine: Idle → Walk → Run → Sprint → Jump
   - Speed-driven blend spaces
   - Upper body slot for emotes
6. Apply ABP to the CC5 character
7. Preview character in creation screen with idle animation
8. Test in-game with locomotion

Follow all rules in 00_Rules_and_Constraints.md.
```
