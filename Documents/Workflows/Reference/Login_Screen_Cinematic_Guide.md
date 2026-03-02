# Login Screen — Editor Setup Guide
**Last updated:** 2026-02-27  
**Scope:** Minimal working Login Level + image-based background

---

## Overview

The Login Screen consists of two things:

| Layer | What | Where |
|---|---|---|
| **Level** | An empty or simple level that loads at startup | `Content/Maps/L_LoginLevel` |
| **UI Overlay** | The Glass & Gold login panel (fully built in C++) | `WBP_LoginScreen` → `UFPMLoginWidget` |

The background behind the panel is currently a **solid deep-navy colour** (`#0a0c10`).  
You can replace it with a **static PNG/texture or an animated Media Texture** at any time by assigning `BackgroundTexture` — no code changes needed.

> [!NOTE]
> The cinematic dolly-camera actors (`AFPMLoginCinematicCamera`, `AFPMLoginLevelSetup`) are compiled and available for future use. They are **not required** for the current setup.

---

## Step 1 — Create the Login Level

1. In the **Content Browser**, go to `Content/Maps/`
2. **File → New Level → Empty Level**
3. Save it as `Content/Maps/L_LoginLevel`
4. No actors or setup required — the level can stay completely empty for now

> [!IMPORTANT]
> **Set it as the default map:**  
> **Edit → Project Settings → Maps & Modes**  
> - **Game Default Map** = `L_LoginLevel`  
> - **Editor Startup Map** = your dev level (keep this separate so the editor doesn't boot into the login screen every time)

---

## Step 2 — Configure WBP_LoginScreen

> **C++ class:** `UFPMLoginWidget`  
> **Source:** `Source/FaldoranPrimeMMO/Public/UI/FPMLoginWidget.h`

The widget builds its entire UI tree in C++ via `BuildUI()`.

**The Blueprint (`WBP_LoginScreen`) must have:**
- Root widget: a single **Canvas Panel** — completely empty, no children

> [!CAUTION]
> Do NOT add any children to the Canvas Panel in the Blueprint editor. C++ creates all children at runtime. Adding anything in the editor will cause conflicts.

### Widget hierarchy (built automatically by C++)

```
Canvas Panel (Blueprint root — must be empty)
│
├── UImage (BackgroundImage)          ZOrder 0  — full-screen opaque background
│     └─ Solid #0a0c10 by default
│        OR your BackgroundTexture if assigned
│
├── UOverlay (CenterOverlay)          ZOrder 1  — centers all content
│   └── UVerticalBox
│       ├── TitleText      "FALDORAN PRIME"          (size 48, animated gold glow)
│       ├── SubtitleText   "THE CIVILIZATION EVOLUTION" (size 11, muted)
│       ├── USizeBox (440px wide)
│       │   └── UBorder (dark glassmorphism panel, 48px padding)
│       │       ├── StatusText    "● SERVER ONLINE"  (green)
│       │       ├── [Label] ACCOUNT IDENTITY
│       │       ├── UsernameInput
│       │       ├── [Label] ACCESS CIPHER
│       │       ├── PasswordInput
│       │       ├── LoginButton      "CONNECT TO WORLD"
│       │       ├── CreateAccountButton "CREATE NEW IDENTITY"
│       │       └── ResultText    (error / success message)
│       ├── FooterSpacer (30px)
│       ├── VersionText    "VERSION 0.1.0-ALPHA-PROTOTYPE"
│       └── CopyrightText  "© 2026 CELTIC TRINITY STUDIOS"
│
└── UImage (ShimmerLine)              ZOrder 10 — animated 3px gold sweep
```

### Animations (run automatically — no Blueprint required)

| Effect | Period | Description |
|---|---|---|
| **Title breathing glow** | 3 sec | Gold drop-shadow pulses 0.15 → 0.55 opacity via sine wave |
| **Gold shimmer sweep** | 4 sec | 3px gold line sweeps left→right across the panel, fades at edges |

---

## Step 3 — Assign a Background Image (optional, any time)

> **Property:** `UFPMLoginWidget::BackgroundTexture`  
> **Type:** `UTexture2D` (also accepts Media Textures for animated/video backgrounds)

### Option A — Set in WBP_LoginScreen class defaults (easiest)

1. Open `WBP_LoginScreen` in the Blueprint editor
2. With nothing selected (the root or the class), look in **Details → FPM|Visual**
3. Set **Background Texture** to your imported image asset

### Option B — Set from PlayerController at runtime

```cpp
// In AFPMPlayerController, before calling ShowLoginWidget():
if (LoginWidget)
{
    LoginWidget->BackgroundTexture = LoadObject<UTexture2D>(
        nullptr, TEXT("/Game/UI/Login/T_LoginBackground"));
}
```

### Supported asset types

| Type | How |
|---|---|
| **Static PNG / JPG** | Import → `UTexture2D` → assign directly |
| **Looping video / animated** | Create a `Media Player` + `Media Texture` → assign the `Media Texture` (it is a `UTexture2D` subclass) |
| **Render Target** | Any `UTextureRenderTarget2D` works too |

---

## Step 4 — Verify the Login Flow

The login widget is created and shown by `AFPMPlayerController`. Check these:

| Setting | Location | Must be |
|---|---|---|
| `LoginWidgetClass` | `AFPMPlayerController` constructor | `WBP_LoginScreen` (or its child class) |
| Game Default Map | Project Settings → Maps & Modes | `L_LoginLevel` |
| Server connectivity | `FPMPlayerController.cpp` → `ShowLoginWidget()` | Called in `BeginPlay` on clients |

---

## Checklist

- [ ] `Content/Maps/L_LoginLevel` created and saved
- [ ] `L_LoginLevel` set as **Game Default Map** in Project Settings
- [ ] `WBP_LoginScreen` exists with a single empty **Canvas Panel** root
- [ ] `LoginWidgetClass` on `AFPMPlayerController` = `WBP_LoginScreen`
- [ ] *(Optional)* Background texture asset imported and assigned

---

## Files Reference

| Purpose | File |
|---|---|
| Login widget header | `Source/FaldoranPrimeMMO/Public/UI/FPMLoginWidget.h` |
| Login widget source | `Source/FaldoranPrimeMMO/Private/UI/FPMLoginWidget.cpp` |
| Player controller | `Source/FaldoranPrimeMMO/Private/Player/FPMPlayerController.cpp` |
| Module build rules | `Source/FaldoranPrimeMMO/FaldoranPrimeMMO.Build.cs` |
| *(Dormant)* Atmosphere actor | `Source/FaldoranPrimeMMO/Public/World/FPMLoginLevelSetup.h` |
| *(Dormant)* Cinematic camera actor | `Source/FaldoranPrimeMMO/Public/World/FPMLoginCinematicCamera.h` |

---

## Future — Cinematic Camera (later sprint)

When you're ready to replace the static image with a live 3D dolly shot:

1. Drop `AFPMLoginLevelSetup` into the level (golden hour atmosphere)
2. Drop `AFPMLoginCinematicCamera` into the level and sculpt the spline
3. Wire `Set View Target with Blend` in the Level Blueprint
4. Remove the `BackgroundTexture` assignment (or leave it as a fade-in layer)

All the code is already compiled and waiting.
