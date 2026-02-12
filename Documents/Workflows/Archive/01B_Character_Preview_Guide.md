# Phase 1B — 3D Character Preview Build Guide

**Created:** 2026-02-09  
**Status:** ✅ Code Complete — Blueprint widget update required  
**Pillar:** 01 — Full Character Creation  
**Prerequisite:** Phase 1A (Affinity Backend) complete

---

## What Was Built

### New Files Created

| File | Lines | Purpose |
|------|-------|---------|
| `Public/Character/Preview/FPMCharacterPreviewActor.h` | 112 | Header for client-only 3D character preview actor |
| `Private/Character/Preview/FPMCharacterPreviewActor.cpp` | 173 | Implementation — mannequin mesh, lighting, scene capture, orbit/zoom |

### Modified Files

| File | Lines | Changes |
|------|-------|---------|
| `Public/UI/FPMCharacterCreationWidget.h` | 122 | Added preview actor management, mouse input overrides, slider callbacks, PreviewImage bind |
| `Private/UI/FPMCharacterCreationWidget.cpp` | 312 | Added preview spawn/destroy, slider→preview wiring, mouse orbit/zoom |

---

## Architecture

### AFPMCharacterPreviewActor

A `NotPlaceable` AActor that is spawned client-only (never replicated) when the character creation screen opens. It lives at a far offset (`-50000, -50000, -5000`) to avoid interfering with the game world.

**Components:**
- `USkeletalMeshComponent` — UE5 mannequin mesh (`SKM_Manny` placeholder)
- `UMaterialInstanceDynamic` — Created at runtime on slot 0 for skin/hair color
- `UPointLightComponent` × 3 — Key (front-right), Fill (front-left), Rim (behind)
- `USpringArmComponent` — Camera distance control (zoom)
- `USceneCaptureComponent2D` — Renders to a `UTextureRenderTarget2D` (512×768)

**Appearance Setters:**
- `SetSkinTone(FLinearColor)` — Sets "SkinTone" vector param on MID
- `SetHairColor(FLinearColor)` — Sets "HairColor" vector param on MID
- `SetHairStyle(uint8)` — PROTOTYPE: logs only (no mesh swap yet)
- `SetBodyType(uint8)` — PROTOTYPE: logs only (no morph target yet)

**Camera Controls:**
- `AddYawRotation(float)` — Rotates the preview root component
- `AddZoom(float)` — Adjusts spring arm length (clamped 80–400)

### Widget Integration

The `UFPMCharacterCreationWidget` now:
1. **Spawns** the preview actor in `NativeConstruct()`
2. **Destroys** it in `NativeDestruct()`
3. **Binds** slider `OnValueChanged` delegates to update the preview in real-time
4. **Binds** the scene capture's render target to a `UImage` widget called `PreviewImage`
5. **Handles** mouse right-drag for orbit and mouse wheel for zoom

---

## Blueprint Widget Update Required

The `WBP_CharacterCreation` Widget Blueprint needs one new widget added:

### Add PreviewImage

1. Open `Content/UI/WBP_CharacterCreation` in UMG Editor
2. Add an **Image** widget to the hierarchy
3. Name it exactly: `PreviewImage`
4. Position it on the left side of the screen (or wherever the 3D preview should appear)
5. Set size to approximately 512×768 (or constrain with a Size Box)
6. The C++ code will automatically assign the render target to this image at runtime

> **IMPORTANT:** The widget will crash if `PreviewImage` is missing because it's a `BindWidget`. Add it before launching.

---

## Material Parameter Setup

For the `SetSkinTone` and `SetHairColor` functions to visually change the mannequin:

1. The mannequin's material (slot 0) must have vector parameters named:
   - `SkinTone` — Controls body color
   - `HairColor` — Controls hair color
2. If the default mannequin material doesn't have these parameters, create a Material Instance:
   - Right-click the mannequin material → Create Material Instance
   - Add the vector parameters
   - Assign the MI to SKM_Manny's slot 0

> **PROTOTYPE NOTE:** The default mannequin material may not have these parameters. The `SetVectorParameterValue` calls will simply have no visual effect until a proper material is set up. The system is ready — it just needs the right material.

---

## Testing

### Quick Test (PIE)

1. Launch PIE (Play In Editor) with Net Mode set to Play as Listen Server or Play as Client
2. Log in → Character creation screen should appear
3. **Verify:**
   - The PreviewImage shows the mannequin with three-point lighting
   - Moving skin color sliders calls `SetSkinTone` (check Output Log for `LogFPMCharacterPreview` messages)
   - Moving hair color sliders calls `SetHairColor`
   - Changing hair style combo box calls `SetHairStyle`
   - Moving body type slider calls `SetBodyType`
   - Right-click drag on the widget rotates the preview
   - Mouse wheel zooms in/out
   - Submitting a character still works correctly
   - Clicking Back destroys the preview actor (check Output Log)

### Log Categories

- `LogFPMCharacterPreview` — Preview actor lifecycle and appearance changes
- `LogFPMCharacterCreationWidget` — Widget lifecycle and preview binding

---

## What's Next

- **Phase 1C:** Expanded appearance (facial features, voice type, tabbed UI layout)
- **Material setup:** Create a proper material with SkinTone/HairColor parameters
- **Mutable/CC5:** Replace the mannequin with a full character customization pipeline

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
