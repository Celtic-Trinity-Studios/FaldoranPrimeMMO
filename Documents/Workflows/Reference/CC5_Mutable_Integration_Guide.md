# CC5 → Mutable Integration Workflow

**Purpose:** Complete pipeline from Reallusion Character Creator 5 to runtime character customization via UE 5.7.1's Mutable plugin.

**Status:** 🚧 PLANNED (2026-02-06)

**Engine Version:** Unreal Engine 5.7.1
**Mutable Version:** 1.8.0 (Beta)
**CC5 Version:** Character Creator 5 (August 2025+)

---

## Why CC5 + Mutable?

| Requirement | CC5 + Mutable Solution |
|------------|------------------------|
| Real vertex morph targets for body customization | CC5 exports **real vertex morph targets** in FBX |
| Standard materials compatible with Mutable | CC5 exports standard FBX materials (Auto Setup converts) |
| High-quality base character models | CC5 subscription provides full character design tools |
| `SetMorphTarget()` must work at runtime | CC5 morphs are vertex-based — `SetMorphTarget()` works ✅ |
| Ethnic diversity for MMO characters | CC5 Pro includes ActorMIXER with 44 scanned heads |

**Proven Foundation:** Mutable + standard FBX skeletal mesh + morph targets = ✅ WORKING
(See: `Mutable_Setup_MicroSteps.md`)

---

## Table of Contents

1. [Prerequisites & Subscription](#phase-0-prerequisites--subscription)
2. [CC5 Setup & Configuration](#phase-1-cc5-setup--configuration)
3. [Character Design in CC5](#phase-2-character-design-in-cc5)
4. [Export from CC5](#phase-3-export-from-cc5)
5. [Import into UE 5.7.1](#phase-4-import-into-ue-571)
6. [Mutable Customizable Object Setup](#phase-5-mutable-customizable-object-setup)
7. [C++ Code Refactor](#phase-6-c-code-refactor)
8. [UI Integration](#phase-7-ui-integration)
9. [Testing](#phase-8-testing)
10. [Troubleshooting](#troubleshooting)

---

## Phase 0: Prerequisites & Subscription

### Step 0.1: Choose CC5 Subscription Tier

| Tier | Price | HD Morphs | ActorMIXER Pro | Headshot | SkinGen | Recommendation |
|------|-------|-----------|----------------|----------|---------|----------------|
| **CC 365** | $8.25/mo | ✅ Yes | ❌ No | ❌ No | ❌ No | Budget option — morphs work |
| **CC 365 Pro** | $33.25/mo | ✅ Yes | ✅ 44 scanned heads | ✅ Photo→3D | ✅ Premium skin | Best value for MMO diversity |
| **RL 3D Suite 365** | $49.92/mo | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | Overkill — includes iClone |

**Recommended:** CC 365 Pro ($33.25/mo) for ethnic diversity via ActorMIXER's 44 scanned heads.
**Minimum Viable:** CC 365 ($8.25/mo) if you only need morphs and will sculpt your own heads.

### Step 0.2: Install Required Software

| Software | Source | Notes |
|----------|--------|-------|
| **Character Creator 5** | [reallusion.com](https://www.reallusion.com/character-creator/) | Install after subscribing |
| **Auto Setup 2.0** plugin | Fab Marketplace (free) | Search "Reallusion Auto Setup" |
| **CC UE Control Rig** plugin | Fab Marketplace (free) | Optional, for animation later |

### Step 0.3: Verify UE Project Plugins (Already Done)

These are already enabled in `FaldoranPrimeMMO.uproject`:

| Plugin | Status | Purpose |
|--------|--------|---------|
| `RigLogicMutable` | ✅ Enabled | RigLogic ↔ Mutable bridge |
| `HairStrandsMutable` | ✅ Enabled | Hair strands via Mutable |
| `MutableClothing` | ✅ Enabled | Clothing via Mutable |

### Step 0.4: Verify Build.cs Module (Already Done)

`MutableRuntime` should be in `FaldoranPrimeMMO.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",
    "UMG",
    "HairStrandsCore",
    "MutableRuntime"       // ← Required for Mutable C++ API
});
```

> **Check:** If `MutableRuntime` is not present yet, add it. It was listed in `Mutable_Setup_MicroSteps.md` as DONE but may need verification in the actual Build.cs file.

---

## Phase 1: CC5 Setup & Configuration

### Step 1.1: Install Auto Setup 2.0 Plugin in UE

1. Open **Fab** inside Unreal Editor: `Window → Fab`
2. Search for **"Reallusion Character Creator iClone Auto Setup"**
3. Click **Add to Project** → Select `FaldoranPrimeMMO`
4. Restart Unreal Editor

### Step 1.2: Verify Auto Setup Installation

After restart:
1. Go to **Edit → Plugins**
2. Search for **"Auto Setup"** or **"Reallusion"**
3. Confirm it appears and is enabled

### Step 1.3: Install CC UE Control Rig (Optional)

1. In Fab, search **"CC UE Control Rig"**
2. Add to project (free)
3. This enables UE5-compatible control rig on CC5 characters

---

## Phase 2: Character Design in CC5

### Step 2.1: Launch Character Creator 5

1. Open CC5
2. Create a new project

### Step 2.2: Create Base Character

For an MMO character creator, you need a **neutral base** that morphs can adjust from:

1. Start with a **default human** (or use ActorMIXER Pro if available)
2. Set **neutral/average** body proportions
3. Set neutral skin tone (will be adjusted at runtime)

### Step 2.3: Configure HD Body Morphs (CRITICAL)

This is the core of what you're paying for. Set up body morphs:

1. Go to the **Body** section in CC5
2. Access **HD Morphs** panel
3. The following morph categories should be available:

| Morph Category | What It Controls | For Your Character Creator |
|----------------|-----------------|---------------------------|
| **Body Fat** | Overall weight/heaviness | ✅ Maps to Weight slider |
| **Body Muscle** | Musculature definition | ✅ Maps to Muscle slider |
| **Body Height** | Stature scaling | ✅ Maps to Height slider |
| **Body Thinness** | Lean body shape | ✅ Inverse of fat |
| **Chest/Torso** | Upper body shape | Optional refinement |
| **Arms/Legs** | Limb proportions | Optional refinement |

4. **Set all morphs to 0.5 (neutral)** — this is the default pose
5. The morphs will export as **vertex morph targets** in the FBX

### Step 2.4: Configure Face Morphs

1. Go to the **Face** section
2. CC5 provides **262 blendshapes + 128 corrective morphs**
3. For character creation, focus on structural morphs:
   - Jaw width/length
   - Brow height
   - Nose width/length
   - Cheekbone prominence
   - Eye shape/size

### Step 2.5: Hair Setup

CC5 allows multiple hair options. For export:
1. Apply a **default hairstyle**
2. Note: Hair may need to be exported as separate groom assets or card-based meshes
3. For Mutable integration, **card-based hair** is simpler than strand grooms

### Step 2.6: Save the CC5 Project

Save as your base character project. You'll return here to create variants or adjust morphs.

---

## Phase 3: Export from CC5

### Step 3.1: Open Export Dialog

1. In CC5, go to **File → Export → FBX**

### Step 3.2: Configure Export Settings (CRITICAL)

| Setting | Value | Why |
|---------|-------|-----|
| **Target** | Unreal UE5 Skeleton | Ensures skeleton compatibility |
| **FBX Format** | Binary | Standard for UE import |
| **HD Character SubD** | SubD 0 (Game Ready) | Full HD will be too heavy for MMO |
| **Smooth Mesh** | ✅ Checked | Ensures clean mesh topology |
| **Include Morph Targets** | ✅ Checked | **THIS IS THE KEY SETTING** |
| **Include Skin Weights** | ✅ Checked | Required for skeletal animation |
| **Embed Textures** | ❌ Unchecked | Import separately for better control |
| **Texture Size** | 2048 or 4096 | 4K for face, 2K for body (MMO perf) |

### Step 3.3: Skeleton Preset

1. Under **Skeleton**, select **UE5 Standard** (recommended) or **UE5 MetaHuman** (CC5 skeleton preset name)
2. The UE5 Standard bind pose ensures animation retargeting compatibility
3. This adds spine and hand bones for 1:1 compatibility

### Step 3.4: Export the FBX

1. Click **Export**
2. Save to a working folder (e.g., `E:\CC5_Exports\BaseCharacter\`)
3. You should get:
   - `BaseCharacter.fbx` — The skeletal mesh with morph targets
   - `Textures/` folder — All texture maps

### Step 3.5: Verify Export Contents

Open the FBX in a viewer (or check CC5's export log):
- ✅ Skeleton present
- ✅ Morph targets listed (body_fat, body_muscle, etc.)
- ✅ Skin weights intact
- ✅ UV maps present

---

## Phase 4: Import into UE 5.7.1

### Step 4.1: Import with Auto Setup 2.0

Auto Setup 2.0 intercepts the import process automatically:

1. Drag the exported FBX into UE Content Browser
2. Target folder: `/Game/Characters/CC5/BaseCharacter/`
3. Auto Setup should detect it as a CC5 character and prompt for setup

### Step 4.2: FBX Import Settings (If Manual)

If Auto Setup doesn't activate, use these manual import settings:

| Setting | Value |
|---------|-------|
| **Skeleton** | None (create new) |
| **Import Morph Targets** | ✅ Yes |
| **Import Animations** | ❌ No (import later) |
| **Material Import Method** | Create New Materials |
| **Normal Import Method** | Import Normals |
| **Convert Scene** | ✅ Yes |
| **Force Front XAxis** | ❌ No |
| **Import Uniform Scale** | 1.0 |

### Step 4.3: Verify Import

After import, check the skeletal mesh asset:

1. Double-click the imported skeletal mesh
2. Go to **Window → Morph Target Previewer**
3. **You should see your CC5 morphs listed:**

```
✅ body_fat        (slide to test)
✅ body_muscle     (slide to test)
✅ body_height     (slide to test)
✅ body_thin       (slide to test)
... (and more)
```

> ⚠️ **If Morphs: 0** — Go back to CC5 and ensure "Include Morph Targets" was checked during export.

### Step 4.4: Auto Setup Material Verification

Auto Setup 2.0 should have created:
- Proper skin shaders with subsurface scattering
- Eye shaders with refraction
- Hair shaders (if applicable)

Check the material instances in `/Game/Characters/CC5/BaseCharacter/Materials/`

### Step 4.5: Note Morph Target Names

**IMPORTANT:** Write down the exact morph target names from the Morph Target Previewer. CC5 uses specific naming conventions. You'll need these for the Mutable and C++ wiring.

Example expected names (may vary):
```
body_fat
body_muscle
body_height
body_thin
chest_size
waist_size
hip_size
arm_muscle
leg_muscle
```

Record the exact names here after import:

| CC5 Morph Name | UI Slider | Default Value |
|----------------|-----------|---------------|
| ________________ | Weight | 0.5 |
| ________________ | Muscle | 0.5 |
| ________________ | Height | 0.5 |
| ________________ | (TBD) | 0.5 |

---

## Phase 5: Mutable Customizable Object Setup

This follows the **proven pattern** from `Mutable_Setup_MicroSteps.md`, replacing `SK_BaseBody` with the CC5 mesh.

### Step 5.1: Create New Customizable Object

1. In Content Browser, navigate to `/Game/Characters/CC5/`
2. Right-click → **Mutable → Customizable Object**
3. Name it: `CO_CC5BaseCharacter`

### Step 5.2: Build the Node Graph

Double-click `CO_CC5BaseCharacter` to open the Mutable graph editor.

Build this graph (same proven structure, new mesh):

```
[Skeletal Mesh]  →  [Mesh Morph]  →  [Mesh Section]  →  LOD 0 [SK Component]  →  Components [Base Object]
     ↑                    ↑               ↑                     ↑                         ↑
 CC5 Mesh           Weight morph    CC5 Material         Name: "Body"            Name: "CC5Character"
                         ↑            (from Auto Setup)  Ref: CC5 SK Mesh
                   [Float Parameter]
                   "BodyFat" (0.0-1.0)
```

### Step 5.3: Wire Morph Chains (One Per Morph)

For EACH morph target, create a chain:

#### Chain 1: Body Fat (Weight)
1. **Mesh Morph** node: Select `body_fat` (or exact CC5 name) from dropdown
2. **Float Parameter**: Name = `BodyFat`, Default = 0.5, Range = 0.0–1.0
3. Connect: Float Parameter → Mesh Morph Factor pin

#### Chain 2: Body Muscle
1. **Mesh Morph** node: Select `body_muscle`
2. **Float Parameter**: Name = `BodyMuscle`, Default = 0.5, Range = 0.0–1.0
3. Connect: Float Parameter → Mesh Morph Factor pin

#### Chain 3: Body Height
1. **Mesh Morph** node: Select `body_height`
2. **Float Parameter**: Name = `BodyHeight`, Default = 0.5, Range = 0.0–1.0
3. Connect: Float Parameter → Mesh Morph Factor pin

#### Chain N: (Repeat for each morph)

### Step 5.4: Enable Real-Time Morph Targets

1. Click the **Skeletal Mesh data node**
2. In Details panel: Check **`Select All Morphs`** ✅
3. This preserves all morph targets in the generated output mesh

### Step 5.5: Add Skin Color Parameter (Optional)

For runtime skin color via Mutable:
1. Right-click → **Add Parameter → Color Parameter**
2. Name = `SkinColor`, Default = neutral skin tone
3. Wire to a **Material** node that overrides the tint parameter

> **Alternative:** Handle skin color via `UMaterialInstanceDynamic` in C++ (simpler, and already working in your codebase).

### Step 5.6: Compile

1. Click **Compile** in the Mutable editor toolbar
2. Check for errors in the output log
3. Test in the **Preview Instance** tab — drag morph sliders to verify

---

## Phase 6: C++ Code Integration

### Current State (2026-02-07)

The C++ codebase is already refactored for Mutable:

| File | Status |
|------|--------|
| `FPMCharacterPreviewActor.h/.cpp` | ✅ Uses `UCustomizableSkeletalComponent` + `UCustomizableObjectInstance` |
| `FPMCharacterCreationShellWidget.h/.cpp` | ✅ Routes slider changes through preview actor |
| `FPMMetaHumanCustomizer.h/.cpp` | 🗑️ **DELETED** — no longer needed |
| `FaldoranPrimeMMO.Build.cs` | ✅ `MutableRuntime` dependency present |

### Step 6.1: Preview Actor API (Already Implemented)

The `AFPMCharacterPreviewActor` already exposes the Mutable-based API:

```cpp
// Body morphs via Mutable
void AFPMCharacterPreviewActor::SetBodyMorphs(float Weight, float Muscle, float Height)
{
    if (MutableInstance)
    {
        MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyFat"), Weight);
        MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyMuscle"), Muscle);
        MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyHeight"), Height);
        MutableInstance->UpdateSkeletalMeshAsync(true);
    }
}
```

### Step 6.2: Remaining TODO

Once the CC5 base character is exported and the Customizable Object is created:

1. Set `CustomizableObjectAsset` in the preview actor’s Blueprint defaults to `CO_CC5BaseCharacter`
2. Add slider debouncing (100ms delay before `UpdateSkeletalMeshAsync`)
3. Handle dynamic material re-application after mesh regeneration

### Step 6.5: Skin Color (Keep Dynamic Material Approach)

Skin color can stay as a `UMaterialInstanceDynamic` parameter:

```cpp
void AFPMCharacterPreviewActor::SetSkinTone(FLinearColor SkinColor)
{
    // Apply via dynamic material instances on the Mutable-generated mesh
    if (MutableSkeletalComponent)
    {
        USkeletalMeshComponent* SkMesh = MutableSkeletalComponent->GetSkeletalMeshComponent();
        if (SkMesh)
        {
            int32 NumMaterials = SkMesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i)
            {
                UMaterialInstanceDynamic* DynMat =
                    Cast<UMaterialInstanceDynamic>(SkMesh->GetMaterial(i));
                if (!DynMat)
                {
                    DynMat = SkMesh->CreateDynamicMaterialInstance(i);
                }
                if (DynMat)
                {
                    DynMat->SetVectorParameterValue(TEXT("SkinColor"), SkinColor);
                }
            }
        }
    }
}
```

> **Note:** After each `UpdateSkeletalMeshAsync()`, the mesh is regenerated. You may need to re-apply dynamic material instances. Handle this via a delegate on Mutable's update completion.

### Step 6.6: Hair (Separate from Body Mutable Object)

Hair can be handled in two ways:

**Option A: Hair as part of Mutable Customizable Object**
- Add hair meshes as additional components in the CO graph
- Switch between them via Mutable parameters
- Best for card-based hair

**Option B: Hair as separate groom component**
- Keep the existing `SetHairStyle()` groom swapping logic
- Attach groom to the Mutable-generated skeleton
- Best for strand-based hair

---

## Phase 7: UI Integration

### Step 7.1: No UI Changes Required

Your existing UI sliders call through `FPMCharacterPreviewActor`:
- `SetBodyMorphs(Weight, Muscle, Height)` → Now routes through Mutable
- `SetSkinTone(Color)` → Still uses dynamic materials
- `SetHairStyle(Index)` → Still swaps groom/mesh assets

The public API is unchanged — only the internal implementation differs.

### Step 7.2: Debounce Slider Updates

Mutable regenerates the mesh on each parameter change. Add debouncing:

```cpp
// Don't call UpdateSkeletalMeshAsync on every slider tick
// Instead, batch changes with a timer:

void AFPMCharacterPreviewActor::SetBodyMorphs(float Weight, float Muscle, float Height)
{
    if (!MutableInstance) return;

    MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyFat"), Weight);
    MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyMuscle"), Muscle);
    MutableInstance->SetFloatParameterSelectedOption(TEXT("BodyHeight"), Height);

    // Debounce: only trigger update every 100ms
    GetWorld()->GetTimerManager().SetTimer(
        MutableUpdateTimerHandle,
        [this]() {
            if (MutableInstance)
            {
                MutableInstance->UpdateSkeletalMeshAsync(true);
            }
        },
        0.1f,    // 100ms delay
        false    // Non-repeating
    );
}
```

---

## Phase 8: Testing

### Step 8.1: Standalone Mutable Test (Before C++ Integration)

1. Place a **Blueprint Actor** in a test level
2. Add a `UCustomizableSkeletalComponent`
3. Set its Customizable Object to `CO_CC5BaseCharacter`
4. Play in Editor
5. Open the CO's **Preview Instance** and drag sliders
6. Verify body deforms correctly

### Step 8.2: Full Pipeline Test

| Test | Expected Result |
|------|-----------------|
| Import CC5 mesh → open Morph Target Previewer | 20+ morph targets visible |
| Mutable CO compile | No errors |
| Mutable Preview Instance → drag BodyFat slider | Body visibly changes |
| PIE → Open Character Creation UI | Character renders in preview |
| Move Weight slider | Body shape changes (logs show Mutable parameter update) |
| Move Muscle slider | Musculature changes |
| Change Skin Color sliders | Skin tint updates |

### Step 8.3: Log Verification

Look for these logs:

```
LogMutable: Parameter BodyFat set to 0.70
LogMutable: Parameter BodyMuscle set to 0.30
LogTemp: AFPMCharacterPreviewActor: SetBodyMorphs(0.70, 0.30, 0.50)
```

### Step 8.4: Performance Check

| Metric | Target | Notes |
|--------|--------|-------|
| Mutable update time | < 50ms | Async, shouldn't block UI |
| Memory per character | < 100MB | Check with `stat Memory` |
| Draw calls | < 50 | Check with `stat SceneRendering` |

---

## Troubleshooting

### CC5 Morphs Don't Appear After Import

| Cause | Fix |
|-------|-----|
| "Include Morph Targets" was unchecked | Re-export from CC5 with it checked |
| FBX import setting missed it | Re-import with "Import Morph Targets" ✅ |
| SubD level too high | Use SubD 0 for export |

### Mutable CO Won't Compile

| Cause | Fix |
|-------|-----|
| Material incompatible (VT) | Use Auto Setup material, not VT materials |
| Missing mesh reference | Set `Reference Skeletal Mesh` on the SK Component node |
| Morph node not connected | Verify Mesh Morph sits between SK Data and Mesh Section |

### Body Doesn't Deform at Runtime

| Cause | Fix |
|-------|-----|
| `Select All Morphs` not checked | Check it on the Skeletal Mesh data node |
| Wrong parameter name in C++ | Match C++ string exactly to Mutable Float Parameter name |
| `UpdateSkeletalMeshAsync` not called | Always call after setting parameters |
| Instance is null | Verify `CustomizableBody->GetCustomizableObjectInstance()` returns valid |

### Materials Look Wrong After Auto Setup

| Cause | Fix |
|-------|-----|
| Auto Setup not installed | Install from Fab marketplace |
| Wrong shader model | Ensure project uses SM5 or SM6 |
| Missing textures | Re-import textures from CC5 export folder |

### Skin Color Resets After Morph Change

| Cause | Fix |
|-------|-----|
| Mutable mesh regeneration clears dynamic materials | Re-apply after `UpdateSkeletalMeshAsync` completes |
| Need to use Mutable Color Parameter instead | Wire skin color through CO graph, not dynamic materials |

---

## File Reference

### Existing Files (Already Updated)

| File | Status |
|------|--------|
| `FaldoranPrimeMMO.Build.cs` | ✅ `MutableRuntime` in dependencies |
| `FPMCharacterPreviewActor.h/.cpp` | ✅ Uses `UCustomizableSkeletalComponent` |
| `FPMCharacterCreationShellWidget.h/.cpp` | ✅ Routes UI to preview actor |

### New Files/Assets to Create

| Asset | Location | Type |
|-------|----------|------|
| CC5 Base Character Mesh | `/Game/Characters/CC5/BaseCharacter/` | Skeletal Mesh (FBX Import) |
| CC5 Materials | `/Game/Characters/CC5/BaseCharacter/Materials/` | Material Instances (Auto Setup) |
| `CO_CC5BaseCharacter` | `/Game/Characters/CC5/` | Mutable Customizable Object |

### Documents (Already Updated)

| Document | Status |
|----------|--------|
| `Mutable_Setup_MicroSteps.md` | ✅ Updated — CC5 is canonical pipeline |
| `Mutable_Character_Customization_Setup.md` | ✅ Updated — CC5 as mesh source |
| `MetaHuman_CharacterCreatorIntegration.md` | 🗑️ **DELETED** |
| `MetaHuman_Fresh_Setup_571.md` | 🗑️ **DELETED** |

---

## Cost Analysis

| Item | Cost | Frequency | Notes |
|------|------|-----------|-------|
| CC 365 subscription | $8.25/mo | Monthly | Minimum for HD Morphs |
| CC 365 Pro subscription | $33.25/mo | Monthly | Adds ActorMIXER, Headshot, SkinGen |
| Auto Setup 2.0 plugin | Free | One-time | From Fab marketplace |
| CC UE Control Rig | Free | One-time | From Fab marketplace |
| Mutable plugin | Free | Built-in | Part of UE 5.7.1 |

**Break-even:** Even 1 month of CC 365 ($8.25) gets you a quality base mesh with morphs. You can cancel after creating and exporting your characters.

> **Pro tip:** Subscribe for 1-2 months, create all the base character variations you need, export everything, then cancel. The exported FBX files are yours to keep — no ongoing license needed for runtime use.

---

## Checklist

### Phase 0: Setup
- [ ] Choose CC5 subscription tier
- [ ] Install Character Creator 5
- [ ] Install Auto Setup 2.0 plugin in UE
- [ ] Verify `MutableRuntime` in Build.cs

### Phase 2-3: CC5 Design & Export
- [ ] Create neutral base character in CC5
- [ ] Configure HD body morphs (fat, muscle, height, etc.)
- [ ] Export FBX with UE5 skeleton preset + morph targets
- [ ] Verify exported FBX has morph targets

### Phase 4: UE Import
- [ ] Import FBX into `/Game/Characters/CC5/BaseCharacter/`
- [ ] Verify morph targets in Morph Target Previewer (20+)
- [ ] Verify Auto Setup materials applied correctly
- [ ] Record exact morph target names

### Phase 5: Mutable Wiring
- [ ] Create `CO_CC5BaseCharacter` Customizable Object
- [ ] Wire Mesh Morph + Float Parameter chains for each morph
- [ ] Enable `Select All Morphs` on SK data node
- [ ] Compile CO — no errors
- [ ] Test in Preview Instance — sliders deform body ✅

### Phase 6-7: Code Integration (Partially Done)
- [x] `FPMCharacterPreviewActor` uses `UCustomizableSkeletalComponent`
- [x] `FPMMetaHumanCustomizer` deleted (no longer needed)
- [ ] Add slider debouncing (100ms) to preview actor
- [ ] Handle material re-application after mesh update
- [ ] Test full UI → Mutable → visual pipeline with CC5 mesh

### Phase 8: Validation
- [ ] Body morphs work from UI sliders
- [ ] Skin color changes
- [ ] Hair style/color works
- [ ] Performance within targets (<50ms update, <100MB memory)

---

*Document created: 2026-02-06*
*Engine Version: UE 5.7.1*
*Mutable Version: 1.8.0 (Beta)*
*CC5: Character Creator 5 (Reallusion, August 2025+)*
*Author: AI Assistant + Celtic Trinity Studios*
