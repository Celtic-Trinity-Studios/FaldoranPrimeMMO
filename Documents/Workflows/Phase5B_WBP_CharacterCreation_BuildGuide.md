# WBP_CharacterCreation — UE Editor Build Guide

**Purpose:** Step-by-step instructions to build the Character Creation Widget Blueprint in Unreal Engine 5.7.1.  
**Prerequisite:** Phase 5B C++ code compiled successfully.  
**Reference:** `Phase5B_CharacterCreation_Layout.md` for the wireframe.

---

## Step 1: Create the Widget Blueprint

1. Open UE Editor → double-click `FaldoranPrimeMMO.uproject`
2. In the **Content Browser** (bottom panel), navigate to `Content/UI/`
   - If `UI` folder doesn't exist: right-click `Content/` → **New Folder** → name `UI`
3. Right-click inside `Content/UI/` → **User Interface** → **Widget Blueprint**
4. Name it exactly: **`WBP_CharacterCreation`**
5. Double-click to open it

---

## Step 2: Reparent to C++ Class

1. In the Widget Blueprint Editor, click the **Graph** tab (top right area)
2. Click **Class Settings** in the toolbar
3. In the **Details** panel on the right, find **Class Options** → **Parent Class**
4. Click the dropdown and type `FPMCharacterCreationWidget`
5. Select it → click **Compile** (top toolbar)
6. You'll see warnings about missing bound widgets — this is expected

---

## Step 3: Build the Layout — Root Structure

Go back to the **Designer** tab.

### 3a. Delete the default Canvas Panel
- In the **Hierarchy** panel (top left), right-click the default `[Canvas Panel]` → **Delete**

### 3b. Add root Overlay
- From **Palette** (left panel), search `Overlay` → drag onto canvas
- This is now the root widget

### 3c. Add background (optional but nice)
- Drag a **Border** into the Overlay
- Details panel → **Brush** → **Brush Color** → set to dark: `(R=0.02, G=0.02, B=0.05, A=0.95)`
- Set **Horizontal Alignment** and **Vertical Alignment** both to **Fill**

### 3d. Add main vertical structure
- Drag a **Vertical Box** into the Overlay (on top of the Border)
- Set **Padding** to `40` on all sides
- Set **Horizontal Alignment** to **Fill**, **Vertical Alignment** to **Fill**

---

## Step 4: Title Row

1. Inside the Vertical Box, drag a **Text Block**
2. Set **Text** to: `CREATE YOUR CHARACTER`
3. Set **Font Size** to `28`
4. Set **Color** to gold: `(R=1.0, G=0.85, B=0.3, A=1.0)`
5. Set **Justification** to **Center**
6. Under **Slot (Vertical Box Slot)** → **Padding** → Bottom = `20`
7. **Horizontal Alignment** → **Fill**

---

## Step 5: Three-Column Content Area

1. Drag a **Horizontal Box** into the Vertical Box (below the title)
2. Under **Slot** → set **Size** to **Fill**

This Horizontal Box will hold 3 columns: Left, Center, Right.

---

## Step 6: LEFT COLUMN — Physical Attributes

### 6a. Column container
1. Inside the Horizontal Box, drag a **Vertical Box**
2. Under **Slot (Horizontal Box Slot)** → **Size** → **Fill** → **Fill Ratio** = `1.0`
3. **Padding** → Right = `10`

### 6b. Section header
1. Inside the left Vertical Box, drag a **Text Block**
2. **Text** = `PHYSICAL ATTRIBUTES`
3. **Font Size** = `16`, **Color** = white
4. **Padding** → Bottom = `15`

### 6c. Character Name
1. Drag **Text Block** → **Text** = `Character Name:`, **Font Size** = `12`
2. **Padding** → Bottom = `4`
3. Drag **Editable Text Box** below it
4. **⚠️ CRITICAL: In the Details panel, set Name to exactly:** `NameInput`
5. **Padding** → Bottom = `12`
6. Set **Hint Text** to `Enter name (3-20 chars)`

### 6d. Body Type
1. Drag **Text Block** → **Text** = `Body Type:`, **Font Size** = `12`, Padding Bottom = `4`
2. Drag **Slider** below it
3. **⚠️ Name:** `BodyTypeSlider`
4. **Min Value** = `0`, **Max Value** = `3`, **Step Size** = `1`
5. **Padding** → Bottom = `12`

### 6e. Skin Color
1. Drag **Text Block** → **Text** = `Skin Color:`, **Font Size** = `12`, Padding Bottom = `4`

For each of the 3 color sliders, add a **Horizontal Box** containing:
- **Text Block** with text `R:` (or `G:` or `B:`) **Font Size** = `11`, Width = `20`
- **Slider** next to it

2. **RED:** Horizontal Box → Text Block (`R:`) + Slider
   - **⚠️ Slider Name:** `SkinRedSlider`
   - Slider slot: **Size** → **Fill**
   - Horizontal Box **Padding** → Bottom = `4`

3. **GREEN:** Same pattern
   - **⚠️ Slider Name:** `SkinGreenSlider`
   - Horizontal Box **Padding** → Bottom = `4`

4. **BLUE:** Same pattern
   - **⚠️ Slider Name:** `SkinBlueSlider`
   - Horizontal Box **Padding** → Bottom = `12`

### 6f. Hair Style
1. Drag **Text Block** → **Text** = `Hair Style:`, **Font Size** = `12`, Padding Bottom = `4`
2. Drag **Combo Box (String)** below it
3. **⚠️ Name:** `HairStyleComboBox`
4. **Padding** → Bottom = `12`
5. No need to add options — the C++ code populates them automatically

### 6g. Hair Color
1. Drag **Text Block** → **Text** = `Hair Color:`, **Font Size** = `12`, Padding Bottom = `4`

Same pattern as Skin Color — 3 Horizontal Boxes:

2. **RED:** Text Block (`R:`) + Slider
   - **⚠️ Slider Name:** `HairColorRedSlider`
   - **Padding** → Bottom = `4`

3. **GREEN:** Text Block (`G:`) + Slider
   - **⚠️ Slider Name:** `HairColorGreenSlider`
   - **Padding** → Bottom = `4`

4. **BLUE:** Text Block (`B:`) + Slider
   - **⚠️ Slider Name:** `HairColorBlueSlider`
   - **Padding** → Bottom = `4`

---

## Step 7: CENTER COLUMN — Character Preview

### 7a. Column container
1. Inside the main Horizontal Box (same level as left Vertical Box), drag another **Vertical Box**
2. Under **Slot (Horizontal Box Slot)** → **Size** → **Fill** → **Fill Ratio** = `1.5`
3. **Padding** → Left = `10`, Right = `10`

### 7b. Section header
1. Drag **Text Block** → **Text** = `CHARACTER PREVIEW`
2. **Font Size** = `16`, **Color** = white, **Justification** = **Center**
3. **Padding** → Bottom = `10`

### 7c. Preview placeholder
1. Drag a **Border** into this Vertical Box
2. Set **Slot** → **Size** → **Fill**
3. **Brush Color** = dark gray: `(R=0.08, G=0.08, B=0.1, A=1.0)`
4. Inside the Border, drag a **Text Block**:
   - **Text** = `3D Mannequin Preview\n(Coming Soon — Mutable/CC5)`
   - **Font Size** = `14`
   - **Color** = `(R=0.4, G=0.4, B=0.4, A=1.0)` (dim gray)
   - **Justification** = **Center**
   - **Horizontal Alignment** = **Center**
   - **Vertical Alignment** = **Center**

---

## Step 8: RIGHT COLUMN — Affinities (Visual Only for Prototype)

### 8a. Column container
1. Inside the main Horizontal Box, drag another **Vertical Box**
2. Under **Slot** → **Size** → **Fill** → **Fill Ratio** = `1.0`
3. **Padding** → Left = `10`

### 8b. Section header
1. Drag **Text Block** → **Text** = `AFFINITIES`
2. **Font Size** = `16`, **Color** = white, **Padding** → Bottom = `10`

### 8c. Playstyle Affinities sub-header
1. Drag **Text Block** → **Text** = `Playstyle (Total: 600)`
2. **Font Size** = `13`, **Color** = gold `(R=1.0, G=0.85, B=0.3)`, **Padding** → Bottom = `6`

### 8d. Six Playstyle Affinity Rows
For each of the 6 affinities, add a **Horizontal Box** containing:
- **Text Block** with the affinity name, Width override = `80`, Font Size = `11`
- **Slider** (fill remaining space)
- **Text Block** showing `100` as default value, Width override = `30`

Affinities to create (in order):
1. `Martial` — Slider value `0.5` (maps to 100/200 range visually)
2. `Ranged`
3. `Magic`
4. `Crafting`
5. `Social`
6. `Survival`

**⚠️ NOTE:** These sliders are NOT BindWidgets yet — they are cosmetic/placeholder for the prototype. They will become BindWidgets in a future phase when affinity redistribution is implemented. For now, set all sliders to **Is Enabled** = **false** (grayed out) so players can see them but can't adjust them.

Set each Horizontal Box **Padding** → Bottom = `3`

### 8e. Spacer
1. Drag a **Spacer** → **Size** = `15`

### 8f. Magical Affinities sub-header
1. Drag **Text Block** → **Text** = `Magical (Total: 800)`
2. **Font Size** = `13`, **Color** = cyan `(R=0.3, G=0.8, B=1.0)`, **Padding** → Bottom = `6`

### 8g. Eight Magical Affinity Rows
Same pattern as Playstyle — Horizontal Box with label + disabled slider + value:

1. `Fire`
2. `Water`
3. `Earth`
4. `Air`
5. `Light`
6. `Shadow`
7. `Nature`
8. `Arcane`

All disabled, all defaulting to `100`.

---

## Step 9: BOTTOM ROW — Buttons and Status

### 9a. Spacer
1. Back in the **main Vertical Box** (below the Horizontal Box with 3 columns):
2. Drag a **Spacer** → **Size** = `15`

### 9b. Bottom bar
1. Drag a **Horizontal Box** into the main Vertical Box
2. **Padding** → Top = `10`

### 9c. Back Button
1. Inside the bottom Horizontal Box, drag a **Button**
2. **⚠️ Name:** `BackButton`
3. Inside the Button, drag a **Text Block** → **Text** = `BACK`
4. Set **Font Size** = `14`
5. Set Button **Padding** → Right = `20`

### 9d. Status Text (center)
1. Drag a **Text Block** into the bottom Horizontal Box
2. **⚠️ Name:** `ResultText`
3. **Text** = blank (cleared by code)
4. **Font Size** = `14`
5. Under **Slot** → **Size** → **Fill** (takes remaining space)
6. **Justification** → **Center**
7. **Vertical Alignment** → **Center**

### 9e. Submit Button
1. Drag a **Button** into the bottom Horizontal Box
2. **⚠️ Name:** `SubmitButton`
3. Inside the Button, drag a **Text Block** → **Text** = `CREATE CHARACTER`
4. Set **Font Size** = `14`

---

## Step 10: Final Checks

1. Click **Compile** in the toolbar — should show **NO errors**
   - If there are errors, they'll say "Missing required widget 'X'" — find the misspelled widget and rename it
2. Verify the **Hierarchy** panel shows all 12 required BindWidgets:
   ```
   ✅ NameInput (Editable Text Box)
   ✅ BodyTypeSlider (Slider)
   ✅ SkinRedSlider (Slider)
   ✅ SkinGreenSlider (Slider)
   ✅ SkinBlueSlider (Slider)
   ✅ HairStyleComboBox (Combo Box String)
   ✅ HairColorRedSlider (Slider)
   ✅ HairColorGreenSlider (Slider)
   ✅ HairColorBlueSlider (Slider)
   ✅ SubmitButton (Button)
   ✅ BackButton (Button)
   ✅ ResultText (Text Block)
   ```
3. **Ctrl+S** to save

---

## Hierarchy Reference (what your tree should look like)

```
[Overlay]
├── [Border] (dark background)
└── [VerticalBox] (main layout)
    ├── [TextBlock] "CREATE YOUR CHARACTER" (title)
    ├── [HorizontalBox] (3-column content)
    │   ├── [VerticalBox] (LEFT — Physical)
    │   │   ├── [TextBlock] "PHYSICAL ATTRIBUTES"
    │   │   ├── [TextBlock] "Character Name:"
    │   │   ├── [NameInput] (EditableTextBox)
    │   │   ├── [TextBlock] "Body Type:"
    │   │   ├── [BodyTypeSlider] (Slider)
    │   │   ├── [TextBlock] "Skin Color:"
    │   │   ├── [HBox] → "R:" + [SkinRedSlider]
    │   │   ├── [HBox] → "G:" + [SkinGreenSlider]
    │   │   ├── [HBox] → "B:" + [SkinBlueSlider]
    │   │   ├── [TextBlock] "Hair Style:"
    │   │   ├── [HairStyleComboBox] (ComboBoxString)
    │   │   ├── [TextBlock] "Hair Color:"
    │   │   ├── [HBox] → "R:" + [HairColorRedSlider]
    │   │   ├── [HBox] → "G:" + [HairColorGreenSlider]
    │   │   └── [HBox] → "B:" + [HairColorBlueSlider]
    │   ├── [VerticalBox] (CENTER — Preview)
    │   │   ├── [TextBlock] "CHARACTER PREVIEW"
    │   │   └── [Border] (dark placeholder)
    │   │       └── [TextBlock] "3D Preview Coming Soon"
    │   └── [VerticalBox] (RIGHT — Affinities)
    │       ├── [TextBlock] "AFFINITIES"
    │       ├── [TextBlock] "Playstyle (Total: 600)"
    │       ├── [HBox] → "Martial" + [Slider disabled] + "100"
    │       ├── [HBox] → "Ranged" + [Slider disabled] + "100"
    │       ├── [HBox] → "Magic" + [Slider disabled] + "100"
    │       ├── [HBox] → "Crafting" + [Slider disabled] + "100"
    │       ├── [HBox] → "Social" + [Slider disabled] + "100"
    │       ├── [HBox] → "Survival" + [Slider disabled] + "100"
    │       ├── [Spacer]
    │       ├── [TextBlock] "Magical (Total: 800)"
    │       ├── [HBox] → "Fire" + [Slider disabled] + "100"
    │       ├── [HBox] → "Water" + [Slider disabled] + "100"
    │       ├── [HBox] → "Earth" + [Slider disabled] + "100"
    │       ├── [HBox] → "Air" + [Slider disabled] + "100"
    │       ├── [HBox] → "Light" + [Slider disabled] + "100"
    │       ├── [HBox] → "Shadow" + [Slider disabled] + "100"
    │       ├── [HBox] → "Nature" + [Slider disabled] + "100"
    │       └── [HBox] → "Arcane" + [Slider disabled] + "100"
    ├── [Spacer]
    └── [HorizontalBox] (bottom bar)
        ├── [BackButton] → "BACK"
        ├── [ResultText] (TextBlock, fill)
        └── [SubmitButton] → "CREATE CHARACTER"
```

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
