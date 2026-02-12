# WBP_CharacterSelect — UE Editor Build Guide

**Phase:** 6 — Character Select + Spawn  
**Parent C++ Class:** `UFPMCharacterSelectWidget`  
**Content Path:** `Content/UI/WBP_CharacterSelect`  

---

## Step 1: Create the Widget Blueprint

1. Open the UE Editor
2. In the **Content Browser** (bottom panel), navigate to `Content/UI/`
3. Right-click in empty space → **User Interface** → **Widget Blueprint**
4. Name it exactly: `WBP_CharacterSelect`
5. Double-click to open the Widget Blueprint editor

---

## Step 2: Reparent to C++ Class

1. In the Widget Blueprint editor, click **Graph** tab (top-right area)
2. Click **Class Settings** in the toolbar
3. In the **Details** panel on the right, find **Parent Class**
4. Click the dropdown and search for `FPMCharacterSelectWidget`
5. Select it — the widget is now backed by our C++ code
6. **Compile** (green checkmark button) and **Save**

---

## Step 3: Add Required Widgets

Switch to the **Designer** tab. Add the following widgets from the **Palette** panel (left side). **Names must match exactly** (these are BindWidget links — mismatched names will crash!).

### Layout (suggested)

```
[Canvas Panel] (root)
  └── [Vertical Box] — centered, 500×600
        ├── [Text Block] — "CHARACTER SELECT" (title, not bound)
        ├── [Text Block] — Name: "SelectedCharacterText"
        ├── [Vertical Box] — Name: "CharacterListBox"
        │     (empty — populated dynamically by code)
        ├── [Text Block] — Name: "StatusText"
        └── [Horizontal Box]
              ├── [Button] — Name: "EnterWorldButton"
              │     └── [Text Block] — "Enter World"
              ├── [Button] — Name: "DeleteCharacterButton"
              │     └── [Text Block] — "Delete"
              └── [Button] — Name: "CreateNewButton"
                    └── [Text Block] — "Create New"
```

### Widget Details

| Widget Type | Name (exact) | Notes |
|-------------|------|-------|
| **VerticalBox** | `CharacterListBox` | Dynamically filled with character buttons |
| **Button** | `EnterWorldButton` | Sends `ServerRequestEnterWorld` RPC |
| **Button** | `DeleteCharacterButton` | Placeholder (logs only in prototype) |
| **Button** | `CreateNewButton` | Navigates to character creation screen |
| **TextBlock** | `SelectedCharacterText` | Shows "Selected: CharName" |
| **TextBlock** | `StatusText` | Shows loading/error messages |

### How to Name a Widget

1. Click on the widget in the **Hierarchy** panel (left side of Designer)
2. In the **Details** panel (right side), the very first field at the top is the widget name
3. Type the exact name from the table above
4. Press Enter to confirm

---

## Step 4: Style (Optional but Recommended)

- **Title text**: Font size 24-32, bold, white or gold color
- **SelectedCharacterText**: Font size 18, light gray
- **StatusText**: Font size 14, will be colored by code (gray/red)
- **Buttons**: Give them some padding (8-16px), min width 120
- **CharacterListBox**: Set **Padding** to give spacing between entries
- Set the root **Canvas Panel** background to a dark semi-transparent color
  (add a **Border** or **Image** behind everything with opacity 0.8)

---

## Step 5: Compile and Save

1. Click **Compile** (green checkmark) in the Widget Blueprint toolbar
2. Click **Save** (disk icon)
3. Close the Widget Blueprint editor

---

## Step 6: Verify

1. Go to **Edit** → **Project Settings** → **Maps & Modes**
2. Verify **Default GameMode** is set to `FPMGameMode`
3. Close project settings
4. Play in Editor with **Run Dedicated Server** enabled (Play dropdown → Multiplayer Options)
5. You should see:
   - Login screen appears
   - After login: character select screen (or character creation if no characters)
   - After creating a character: character select with the new character listed
   - Click "Enter World": character spawns, UI disappears, game input enabled

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Crash on play | A BindWidget name doesn't match. Check all 6 widget names exactly. |
| Widget not showing | Check that `WBP_CharacterSelect` is at `Content/UI/` (not a subfolder) |
| Buttons don't work | Make sure you reparented to `FPMCharacterSelectWidget` (step 2) |
| "CharacterSelectWidgetClass not set" in log | The ConstructorHelpers path `/Game/UI/WBP_CharacterSelect` doesn't match |
| Character doesn't spawn | Check the Output Log for server-side errors. Verify DB connection works. |
