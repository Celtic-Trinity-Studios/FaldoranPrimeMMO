# Workflow: Cinematic Login Screen Implementation

This guide provides the exact steps to recreate the high-fidelity, animated login screen in Unreal Engine 5, matching the "Faldoran Prime" concept art.

## Phase 1: The 3D Environment (Backdrop)

The login screen is not a static 2D image; it is a dedicated 3D level (`L_LoginLevel`) that uses high-end lighting and VFX.

### 1.1 Level Setup
1. **Create a New Level**: `Content/Maps/L_LoginLevel`.
2. **Environment Assets** (Megascans recommended):
   - **Stone Arch**: Place a large stone arch asset as the foreground framing element.
   - **Crystals**: Scatter crystal assets in the foreground.
   - **Landscape**: Use a simple landscape or distant static meshes for mountains.
3. **Lighting (The "Golden Hour")**:
   - **Directional Light**: Set to a low angle (Sunset/Sunrise). Intensity: 10-15 lux. Color: Warm Orange/Gold.
   - **Exponential Height Fog**: Enable **Volumetric Fog**. Set Fog Inscattering Color to a soft blue to create distance.
   - **Sky Atmosphere**: Adjust the Rayleigh scattering to give that purple/pink twilight sky.

### 1.2 The "Mana Beam" VFX
1. **Mesh**: Create a tall, thin cylinder mesh.
2. **Material**: Create an Emissive Material.
   - Use a **Fresnel** node to make the edges soft.
   - Add a **Panner** node with a noise texture to make the beam "flicker" upward.
   - Color: Cyan/Light Blue. Emissive Power: 50-100.
3. **Placement**: Place it in the distance, centered in the stone arch.

---

## Phase 2: Post-Processing & Camera

To get the "Premium" look, the camera and post-processing must be tuned.

1. **Post-Process Volume**:
   - **Bloom**: Set Method to **Convolution** for cinematic starbursts on light sources.
   - **Color Grading**: Increase Contrast (1.2) and add a slight Blue tint to shadows and Orange tint to highlights.
   - **Lens Flare**: Enable Bokeh lens flares.
2. **Cine Camera Actor**:
   - **Lens**: 35mm or 50mm for a cinematic field of view.
   - **Focus**: Set Focus to the distant tower (creating a soft blur/bokeh on the foreground arch).

---

## Phase 3: The UMG UI (Styling)

Recreating the "Glass & Gold" panel from the visual mockup.

> **UPDATE (2026-02-11):** The `FPMLoginWidget.cpp` now builds the entire UI
> **programmatically** in `NativeConstruct` → `BuildUI()`. The Blueprint
> `WBP_LoginScreen` should have a **single, empty Canvas Panel** as its root.
> All visual children (background, title, inputs, buttons, footer) are
> created in C++ code. This ensures a full-screen opaque dark background
> that hides whatever level is loaded behind it.

### 3.1 Hierarchy in `WBP_LoginScreen` (Built by C++)
- **Canvas Panel** (Blueprint root — must be empty)
  - **UImage** — Full-screen stretch, deep navy (`#0a0c10`), hides the world.
  - **UOverlay** — Full-screen centered layout container.
    - **UVerticalBox** — Main content column.
      - **Title Text**: "FALDORAN PRIME" (Size 48, gold shadow, white).
      - **Subtitle Text**: "THE CIVILIZATION EVOLUTION" (Size 11, muted).
      - **USizeBox** (440px wide) → **UBorder** (dark panel with padding).
        - **Status Text**: "● SERVER ONLINE" (green).
        - **Labels + EditableTextBoxes**: Gold labels, dark inputs.
        - **Buttons**: Gold gradient login, outlined create-account.
        - **Result Text**: Error/success message display.
      - **Footer**: Version + Copyright text (muted, small).

---

## Phase 4: The Parallax Logic (Blueprint)

To make the screen feel "alive," we add a parallax effect where the UI and Camera react to mouse movement.

### 4.1 In `AFPMPlayerController` or `L_LoginLevel` Blueprint
1. **Get Mouse Position**: Every `Tick`, get the mouse position in viewport space.
2. **Normalize**: Map the position to a range of `-1.0 to 1.0`.
3. **Apply Offsets**:
   - **UI Offset**: Shift the `WBP_LoginScreen` widget by `-10 to 10` pixels.
   - **Camera Offset**: Slightly rotate the **Cine Camera Actor** (Pitch/Yaw) by `0.5` degrees.
4. **Smoothness**: Use **FInterp To** nodes to ensure the movement isn't jerky.

---

## Phase 5: Implementation Checklist

- [ ] Assets imported (Stone Arch, Crystals, Distant Tower).
- [ ] `L_LoginLevel` lighting matches the sunset aesthetic.
- [ ] `WBP_LoginScreen` hierarchy matches the functional requirements in `FPMLoginWidget.cpp`.
- [ ] Parallax logic implemented for subtle depth.
- [ ] Emissive "Mana Beam" material applied.

> **Pro Tip:** In the `FPMLoginWidget.cpp`, add a **Widget Animation** that fades the entire screen in from black on `NativeConstruct` to give a polished, cinematic transition when the game starts.
