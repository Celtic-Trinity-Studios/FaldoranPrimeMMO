# Mutable Character Customization — Setup & Reference

**Purpose:** Enable Epic's Mutable plugin for runtime character customization with morph targets, using CC5-exported meshes as the base.

**Status:** ✅ PROVEN WORKING — Morph targets through Mutable confirmed functional.

**Engine Version:** UE 5.7.1  
**Mutable Version:** 1.8.0 (Beta)  
**Last Updated:** 2026-02-07

### Why Mutable (vs. Raw Morph Targets)
- **Runtime mesh generation** — Build character meshes on-the-fly from component parts
- **Memory efficient** — Shares base mesh data, generates variants procedurally
- **Parameter-driven** — Change float/color/bool parameters → mesh regenerates
- **LOD-aware** — Automatically handles LOD transitions
- **Modular assembly** — Combine head, body, armor pieces via a single Customizable Object graph
- **MMO-ready** — Designed for multiplayer: server sends parameter values, client generates mesh locally

---

## Mesh Compatibility with Mutable

| Mesh Type | Mutable Compatible? | Notes |
|-----------|-------------------|-------|
| CC5-exported FBX skeletal mesh | ✅ Yes | Primary pipeline — real vertex morph targets |
| Standard FBX skeletal mesh | ✅ Yes | Works perfectly |
| Mutable Sample SK_BaseBody | ✅ Yes | Proven with morphs |

---

## Step 1: Enable Mutable Plugin ✅ DONE

### 1.1 Open Plugins
- **Where:** Menu bar → `Edit` → `Plugins`
- **What:** Search for `Mutable`

### 1.2 Enable Core Plugin
- **Find:** `Mutable` (Beta, v1.8.0) → ✅ Enable

### 1.3 Enable RigLogic Extension
- **Find:** `RigLogic Extensions For Mutable` (Experimental, v0.1) → ✅ Enable

### 1.4 Optionally Enable Clothing
- **Find:** `Mutable Clothing` (Experimental, v1.0) → ✅ Enable

### 1.5 Restart Editor

---

## Step 2: Add Mutable Module to Build.cs ✅ DONE

### File: `Source/FaldoranPrimeMMO/FaldoranPrimeMMO.Build.cs`

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",
    "UMG",
    "HairStrandsCore",
    "MutableRuntime"      // ← ADDED
});
```

---

## Step 3: Create Customizable Object ✅ DONE

### 3.1 Location
- **Path:** `/Game/Test/Mutable/CO_TestBody`

### 3.2 Correct Node Graph (Proven Working)

```
[Skeletal Mesh]  →  [Mesh Morph]  →  [Mesh Section]  →  LOD 0 [Skeletal Mesh Component]  →  Components [Base Object]
     ↑                    ↑                ↑                        ↑                              ↑
 SK_BaseBody        Deforms mesh     MI_MaleBodyYoung          Name: "Full"                   Name: any
                         ↑               Material           Ref: SK_BaseBody
                   [Float Parameter]
                   Factor (0.0-1.0)
```

### 3.3 Node Setup Step-by-Step

1. **Base Object** — Created automatically, set Name
2. **Add Skeletal Mesh Component:**
   - Drag from `Components` pin → Select `Skeletal Mesh Component`
   - Set Name (e.g., `SKM_MH_BaseCreator_BodyMesh`)
   - In Details: Set `Reference Skeletal Mesh` to your mesh
3. **Add Mesh Section:**
   - Drag from `LOD 0` pin → Select `Mesh Section`
   - In Details: Set `Material` to the body material
4. **Add Skeletal Mesh Data Node:**
   - Drag from Mesh Section's `Mesh` pin → Select `Mesh → Skeletal Mesh`
   - In Details: Set `Skeletal Mesh` to same mesh
5. **Add Mesh Morph (for morph targets):**
   - Insert between Skeletal Mesh and Mesh Section
   - Disconnect SK data → Mesh Section wire
   - Drag from Mesh Section's `Mesh` pin → `Mesh → Mesh Morph`
   - Reconnect: SK data → Mesh Morph → Mesh Section
   - In Details: Select which morph target to drive
6. **Add Float Parameter:**
   - Drag from Mesh Morph's `Factor` pin → `Float → Float Parameter`
   - In Details: Set Name, Default (0.0), Range (0.0 - 1.0)
7. **Enable Real Time Morph Targets:**
   - Click Skeletal Mesh data node
   - In Details: Check `Select All Morphs`
8. **Compile** — Click Compile button in toolbar

---

## Step 4: Test in Preview Instance ✅ WORKING

After compiling:
1. Go to `Preview Instance` tab
2. Under `Instance Parameters`, find the morph slider
3. Drag the slider — body shape should deform in real-time

---

## Common Errors & Solutions

| Error | Cause | Fix |
|-------|-------|-----|
| `Missing reference Skeletal Mesh` | Skeletal Mesh Component missing mesh ref | Set `Reference Skeletal Mesh` in Details |
| `no data to stream` | Mesh incompatible (VT/Nanite) or Mesh Section not connected | Use standard FBX mesh, verify all connections |
| `Required connection: Morph factor` | Float Parameter not connected | Connect Float Parameter to Mesh Morph Factor pin |
| Empty preview, no errors | Camera position | Zoom in with scroll, press F to focus |

---

## Key Lessons Learned

1. **Standard FBX skeletal meshes work perfectly** — tested with Mutable Sample's SK_BaseBody
2. **CC5-exported meshes are the canonical pipeline** — CC5 exports real vertex morph targets compatible with Mutable
3. **The Mutable editor context menus are PIN-SENSITIVE** — right-clicking empty space shows different options than dragging from specific pins
4. **Mesh Morph nodes** go BETWEEN the Skeletal Mesh data and the Mesh Section
5. **Each morph target needs its own Mesh Morph + Float Parameter chain**

---

## Migrated Assets (from Mutable Sample Project)

- **Source:** `E:\Samples\Mutable\MutableSample\Content\Character\Body\`
- **Migrated to:** `E:\FaldoranPrimeMMO\Content\Character\Body\`
- **Key files:**
  - `SK_BaseBody` — 40MB base body skeletal mesh with morph targets
  - `MI_MaleBodyYoung` — Body material
  - `MI_MaleHeadYoung` — Head material
  - `SKEL_BaseBody` — Skeleton
  - `SK_ReshapeMesh` — Reshape mesh with blend shapes

---

## Next Steps (Not Yet Done)

1. **Add more Mesh Morph chains** for additional morph targets (body_fat, body_muscle, etc.)
2. **CC5 Pipeline:** Export base character from CC5 with full morph library (see `CC5_Mutable_Integration.md`)
3. **C++ Integration:** `FPMCharacterPreviewActor` already uses `UCustomizableSkeletalComponent` ✅
4. **Place Customizable Object in test level** with actor + Customizable Skeletal Component

---

## Runtime Control

### Blueprint Approach
```
[Slider Value Changed Event]
    │
    ▼
[Get Customizable Object Instance]
    │
    ▼
[Set Float Parameter Value]
    - Parameter Name: "BodyFat"
    - Value: (slider value 0.0-1.0)
    │
    ▼
[Update Skeletal Mesh]  ← Forces regeneration
```

### C++ Approach
```cpp
void AFPMCharacterPreviewActor::SetBodyMorph(FName MorphName, float Value)
{
    if (MutableInstance)
    {
        MutableInstance->SetFloatParameterSelectedOption(MorphName, Value);
        MutableInstance->UpdateSkeletalMeshAsync(true);
    }
}
```

### Performance Notes
- Mutable mesh generation is **async** — use `UpdateSkeletalMeshAsync(true)` for non-blocking updates
- **Throttle slider updates** — debounce to ~100ms to avoid excessive regeneration
- Reduce LOD complexity for faster generation
- Cache generated meshes for common configurations

---

## Reference Links

- [Mutable Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-in-unreal-engine)
- [Mutable Quick Start](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-quick-start-in-unreal-engine)
- [Mutable Sample Project](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-samples-in-unreal-engine)

---

*Engine Version: UE 5.7.1*  
*Proven: Mutable + standard skeletal mesh + morph targets = ✅ WORKING*

