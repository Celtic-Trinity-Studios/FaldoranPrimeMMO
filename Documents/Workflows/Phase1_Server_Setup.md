# Phase 1 — Server Setup Walkthrough

**Goal:** Create a test map, configure Project Settings, and launch the dedicated server.  
**Prerequisites:** Phase 0 complete. `AFPMGameMode` and `UFPMGameInstance` compiled successfully.  
**Created:** 2026-02-07

---

## Part A: Close the Editor (if open)

The Editor build was blocked by Live Coding. Before proceeding:

1. **Close Unreal Editor** completely if it is currently open
2. Rebuild the Editor target from Visual Studio:
   - Open `E:\FaldoranPrimeMMO\FaldoranPrimeMMO.sln` in Visual Studio
   - Set configuration to **Development Editor | Win64** (toolbar dropdowns)
   - **Build → Build Solution** (`Ctrl+Shift+B`)
   - Wait for "Build succeeded" in the Output window
3. Once the build succeeds, reopen the editor by double-clicking:
   `E:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject`

---

## Part B: Create the Test Map — `L_PrototypeWorld`

### B1. Create the Maps folder

1. Look at the **Content Browser** panel (bottom of the editor)
2. In the left sidebar of the Content Browser, you should see **Content** (the root)
3. **Right-click** on `Content` in the left sidebar → **New Folder**
4. Name the folder: **`Maps`**
5. **Double-click** the `Maps` folder to open it

### B2. Create a new Level

1. Inside `Content/Maps/`, **right-click** in the empty area of the Content Browser
2. In the context menu, click **Level** (it's at the top, under "Create Basic Asset")
   - If you don't see "Level", look for **Maps & Levels → Level**
3. A new level asset appears — name it: **`L_PrototypeWorld`**
4. **Double-click** `L_PrototypeWorld` to open it in the viewport
   - The editor may ask "Save current level?" — click **Save** or **Don't Save** as appropriate

### B3. Add a floor and basic lighting

You should now be looking at an empty black level. Let's add the minimum to make it usable:

**Add a floor:**

1. In the top toolbar, click the **Place Actors** button (cube icon with a plus sign, or use the shortcut: **Shift+1**)
   - Alternatively: top menu → **Edit → Place Actors**
2. In the Place Actors panel, go to the **Shapes** category (or search)
3. Search for: **`Plane`**
4. **Drag** the Plane from the panel into the center of the viewport
5. With the Plane selected, look at the **Details** panel (right side):
   - Under **Transform → Location**: set to `X=0, Y=0, Z=0`
   - Under **Transform → Scale**: set to `X=100, Y=100, Z=1`
   - This creates a 10,000 × 10,000 cm (100m × 100m) flat floor

**Add a light source:**

6. Back in the **Place Actors** panel, go to **Lights** category
7. **Drag** a **Directional Light** into the viewport
   - Position doesn't matter much — it's a sun-like light that illuminates everything directionally
8. In the **Details** panel for the Directional Light:
   - Under **Transform → Rotation**: set `X=0, Y=-45, Z=0` (angled sunlight)

**Add sky (optional but recommended):**

9. In the **Place Actors** panel, search for **`Sky Atmosphere`** → drag it into the viewport
10. Search for **`Sky Light`** → drag it into the viewport
    - In its Details panel, check **Real Time Capture** (so the sky reflects properly)

**Add a Player Start:**

11. In the **Place Actors** panel, search for **`Player Start`**
12. **Drag** it into the viewport, positioned slightly above the floor:
    - **Location**: `X=0, Y=0, Z=100` (100 cm above the floor so the player doesn't spawn inside it)

### B4. Save the map

1. Press **Ctrl+S** to save
2. Verify in your file explorer that the file exists at:
   `E:\FaldoranPrimeMMO\Content\Maps\L_PrototypeWorld.umap`

---

## Part C: Set Project Settings

### C1. Open Project Settings

1. In the UE Editor, go to the top menu: **Edit → Project Settings**
2. A large settings window opens

### C2. Set Default GameMode

1. In the **left sidebar** of Project Settings, find and click: **Maps & Modes**
   - It's under the **Project** section (near the top)
2. You'll see a section called **Default Modes**
3. Find the **Default GameMode** dropdown
4. Click the dropdown → type **`FPMGameMode`** in the search box
5. Select **`FPMGameMode`** from the list
6. After selecting it, the fields below should auto-populate:
   - Default Pawn Class: `DefaultPawn`
   - HUD Class: `HUD`
   - Player Controller Class: `PlayerController`
   - Game State Class: `GameStateBase`
   - Player State Class: `PlayerState`
   - Spectator Class: `SpectatorPawn`

### C3. Set Default GameInstance

1. Still on the **Maps & Modes** page, scroll down
2. Find the **Game Instance Class** field (it's below the Default Modes section)
3. Click the dropdown → type **`FPMGameInstance`** in the search box
4. Select **`FPMGameInstance`**

### C4. Set Default Maps

1. Still on the **Maps & Modes** page, look at the **Default Maps** section (at the top)
2. Set all three map fields:
   - **Editor Startup Map**: click the dropdown → select **`L_PrototypeWorld`**
     - If it doesn't appear, click the arrow (▶) next to the dropdown → browse to `/Game/Maps/L_PrototypeWorld`
   - **Game Default Map**: set to **`L_PrototypeWorld`** (same as above)
   - **Server Default Map**: set to **`L_PrototypeWorld`** (same as above)

### C5. Verify the config file was updated

1. Close the Project Settings window
2. The editor automatically saves these settings to config. You can verify by checking:
   `E:\FaldoranPrimeMMO\Config\DefaultEngine.ini`
3. It should now contain lines similar to:
   ```ini
   [/Script/EngineSettings.GameMapsSettings]
   GameDefaultMap=/Game/Maps/L_PrototypeWorld
   ServerDefaultMap=/Game/Maps/L_PrototypeWorld
   EditorStartupMap=/Game/Maps/L_PrototypeWorld
   GlobalDefaultGameMode=/Script/FaldoranPrimeMMO.FPMGameMode
   GameInstanceClass=/Script/FaldoranPrimeMMO.FPMGameInstance
   ```

---

## Part D: Test in PIE (Play In Editor)

Before launching the standalone server, do a quick PIE test:

1. In the UE Editor, make sure `L_PrototypeWorld` is open in the viewport
2. Click the **Play** button (▶) in the top toolbar
3. Open the **Output Log**: top menu → **Window → Developer Tools → Output Log**
4. In the Output Log, search for these two lines:
   - `FPM: GameInstance initialized`
   - `FPM: Server started, waiting for connections`
5. If you see both lines: ✅ The GameMode and GameInstance are working
6. Press **Esc** or click **Stop** to end the play session

---

## Part E: Launch the Dedicated Server (Development Mode)

> **⚠️ IMPORTANT:** The standalone `FaldoranPrimeMMOServer.exe` requires **cooked content** to run.
> During development, you have **uncooked** (raw editor) assets, so that binary will crash.
> Use one of the two methods below instead.

### Method 1: PIE with Dedicated Server (Fastest — Recommended)

This is the easiest way to test server functionality during development:

1. In the **top toolbar**, find the green **Play (▶)** button
2. Click the **small dropdown arrow (▾)** to the right of the Play button
3. Look for **Multiplayer Options** or **Net Mode** in the dropdown:
   - Change **Net Mode** to: **Play As Client**
   - Check: **Run Dedicated Server** ✅
   - Set **Number of Players** to: **1**
4. Click the **Play (▶)** button
5. Open **Output Log**: **Window → Developer Tools → Output Log**
6. Filter for `FPM:` — you should see:
   - `FPM: GameInstance initialized`
   - `FPM: Server started, waiting for connections`
7. Press **Esc** or click **Stop** to end

### Method 2: Editor Executable in Server Mode (Standalone Window)

This launches the Editor binary as a headless server. It can load uncooked assets.

1. **Close the UE Editor** if it's open
2. **Open PowerShell** and run (all one line):
   ```powershell
   & "E:\UEInstalled\Windows\Engine\Binaries\Win64\UnrealEditor.exe" "E:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject" L_PrototypeWorld -server -log -port=7777
   ```
3. Wait for the server to fully load (~10-30 seconds)
4. Look for the log messages:
   - `FPM: GameInstance initialized`
   - `FPM: Server started, waiting for connections`
5. Verify it's listening — in a **second** PowerShell window:
   ```powershell
   netstat -an | findstr "7777"
   ```
   Look for `LISTENING` or a UDP entry on port 7777.
6. To stop: Close the log window or press `Ctrl+C`

### Why NOT `FaldoranPrimeMMOServer.exe`?

| Binary | Requires Cooked Content? | Use During Development? |
|--------|--------------------------|------------------------|
| `UnrealEditor.exe -server` | ❌ No (loads raw assets) | ✅ Yes |
| `FaldoranPrimeMMOServer.exe` | ✅ Yes (crashes without it) | ❌ Not until you cook |

You will use `FaldoranPrimeMMOServer.exe` later when deploying to a production server.
To cook content: **UE Editor → Platforms → Windows → Cook Content** (not needed now).

---

## Checklist

- [ ] Editor builds and launches without errors
- [ ] `L_PrototypeWorld` map exists at `Content/Maps/`
- [ ] Map has a floor, light, and Player Start
- [ ] Default GameMode is set to `FPMGameMode`
- [ ] Default GameInstance is set to `FPMGameInstance`
- [ ] All three Default Maps point to `L_PrototypeWorld`
- [ ] PIE (Method 1) shows both `FPM:` log messages in Output Log
- [ ] Editor-as-server (Method 2) shows both `FPM:` log messages
- [ ] `netstat` confirms port 7777 is listening (Method 2)

---

*Document created: 2026-02-07*  
*Updated: 2026-02-07 — Fixed server launch instructions (standalone .exe requires cooked content)*  
*Phase 1 of Prototype Implementation Plan*




