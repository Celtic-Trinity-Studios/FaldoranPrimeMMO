# Character Creation UI - Step-by-Step UMG Setup

## Prerequisites
- UE5 Editor open with FaldoranPrimeMMO project
- Build completed successfully (C++ code compiled)
- CC5 character exported and imported (or placeholder skeletal mesh for testing)

---

## STEP 1: Create the Widget Blueprint

1. **Open Content Browser** → Navigate to `Content/UI/CharacterCreation/` (create folders if needed)

2. **Right-click** in Content Browser → **User Interface** → **Widget Blueprint**

3. **In the popup**, select parent class:
   - Click **"All Classes"** dropdown
   - Type: `FPMCharacterCreationShellWidget`
   - Select it from the list
   - Click **Select**

4. **Name the widget**: `WBP_CharacterCreationShell`

5. **Double-click** to open the Widget Designer

---

## STEP 2: Set Up Root Layout

1. **In the Hierarchy panel** (left side), you'll see `[Canvas Panel]` as root

2. **Delete the Canvas Panel**:
   - Select `[Canvas Panel]`
   - Press **Delete**

3. **From Palette** (left side), drag **Vertical Box** to the viewport
   - This becomes the root widget

4. **Name it** (optional): Select it → In Details panel → rename to `RootVBox`

---

## STEP 3: Create the 3-Panel Layout

### 3.1 Add Main Content Area

1. **With `RootVBox` selected**, drag a **Horizontal Box** from Palette into it
   - This will be the container for the 3 panels

2. **Select the Horizontal Box** → In **Details panel**:
   - Under **Slot (Vertical Box Slot)** section:
     - Set **Size** → **Fill**
   - Rename to `MainContentHBox`

### 3.2 Create Left Panel Container

1. **With `MainContentHBox` selected**, drag a **Vertical Box** into it
   - This is the LEFT panel

2. **Select this Vertical Box** → In **Details panel**:
   - Under **Slot (Horizontal Box Slot)**:
     - Size: **Fill** 
     - Size Value: `0.30` (30% width)
   - Rename to `LeftPanel`

### 3.3 Create Center Panel Container

1. **With `MainContentHBox` selected**, drag another **Vertical Box** into it
   - This is the CENTER panel (will contain preview)

2. **Select this Vertical Box** → In **Details panel**:
   - Under **Slot (Horizontal Box Slot)**:
     - Size: **Fill**
     - Size Value: `0.40` (40% width)
   - Rename to `CenterPanel`

### 3.4 Create Right Panel Container

1. **With `MainContentHBox` selected**, drag another **Vertical Box** into it
   - This is the RIGHT panel (affinities)

2. **Select this Vertical Box** → In **Details panel**:
   - Under **Slot (Horizontal Box Slot)**:
     - Size: **Fill**
     - Size Value: `0.30` (30% width)
   - Rename to `RightPanel`

### 3.5 Add Footer

1. **With `RootVBox` selected**, drag another **Horizontal Box** into it (below the MainContentHBox)

2. **Select this Horizontal Box** → In **Details panel**:
   - Under **Slot (Vertical Box Slot)**:
     - Size: **Auto** (not Fill)
     - Padding: Top = 8
   - Rename to `FooterHBox`

---

## STEP 4: Build Left Panel (Physical Attributes)

### 4.1 Add Tab Button Bar

1. **Select `LeftPanel`** in Hierarchy

2. **Drag a Horizontal Box** into `LeftPanel`
   - Rename to `TabButtonBar`
   - In Details: Slot → Size: **Auto**

### 4.2 Create Identity Tab Button

1. **With `TabButtonBar` selected**, drag a **Button** into it
   - In Details panel → **rename to exactly**: `TabButtonIdentity`

2. **With `TabButtonIdentity` selected**, drag a **Text Block** into the button
   - In Details panel → **rename to exactly**: `TabTextIdentity`
   - Set **Text** property: `Identity`
   - Set **Font Size**: 14

3. **Style the button** (Details panel):
   - Background Color: `(R:0.05, G:0.05, B:0.05, A:0.8)`
   - Padding: 12 horizontal, 6 vertical

### 4.3 Create Face Tab Button

1. **With `TabButtonBar` selected**, drag another **Button** into it
   - **Rename to exactly**: `TabButtonFace`

2. **Drag a Text Block** into `TabButtonFace`
   - **Rename to exactly**: `TabTextFace`
   - Text: `Face`
   - Font Size: 14

### 4.4 Create Body Tab Button

1. **With `TabButtonBar` selected**, drag another **Button** into it
   - **Rename to exactly**: `TabButtonBody`

2. **Drag a Text Block** into `TabButtonBody`
   - **Rename to exactly**: `TabTextBody`
   - Text: `Body`
   - Font Size: 14

### 4.5 Create Hair Tab Button

1. **With `TabButtonBar` selected**, drag another **Button** into it
   - **Rename to exactly**: `TabButtonHair`

2. **Drag a Text Block** into `TabButtonHair`
   - **Rename to exactly**: `TabTextHair`
   - Text: `Hair`
   - Font Size: 14

### 4.6 Add Widget Switcher

1. **Select `LeftPanel`** in Hierarchy

2. **From Palette**, search for **Widget Switcher** → drag into `LeftPanel`
   - **Rename to exactly**: `PhysicalTabSwitcher`
   - In Details: Slot → Size: **Fill**

---

## STEP 5: Create Tab Content Panels

### 5.1 Create Identity Panel (Tab Index 0)

1. **Select `PhysicalTabSwitcher`** in Hierarchy

2. **Drag a Vertical Box** into `PhysicalTabSwitcher`
   - Rename to `IdentityPanel`
   - Padding: 12 all sides

3. **With `IdentityPanel` selected**:

   a. **Add Name Label**:
      - Drag **Text Block** → Text: `Character Name` → Font: Bold 14
   
   b. **Add Name Input**:
      - Drag **Editable Text Box** → **Rename to exactly**: `NameInput`
      - In Details: Hint Text: `Enter name...`
   
   c. **Add Spacer**:
      - Drag **Spacer** → Size: 16
   
   d. **Add Voice Label**:
      - Drag **Text Block** → Text: `Voice Type`
   
   e. **Add Voice Dropdown**:
      - Drag **Combo Box (String)** → **Rename to exactly**: `VoiceTypeComboBox`

### 5.2 Create Face Panel (Tab Index 1)

1. **Select `PhysicalTabSwitcher`** in Hierarchy

2. **Drag a Scroll Box** into `PhysicalTabSwitcher`
   - Rename to `FacePanelScroll`

3. **Drag a Vertical Box** into `FacePanelScroll`
   - Rename to `FacePanel`
   - Padding: 12 all sides

4. **For EACH face morph slider, repeat this pattern**:

   **Face Width:**
   - Drag **Text Block** → Text: `Face Width`
   - Drag **Slider** → **Rename to exactly**: `FaceWidthSlider`
   - Slider Details: Min Value: 0, Max Value: 1, Value: 0.5

   **Jaw Width:**
   - Drag **Text Block** → Text: `Jaw Width`
   - Drag **Slider** → **Rename to exactly**: `JawWidthSlider`
   
   **Cheekbones:**
   - Drag **Text Block** → Text: `Cheekbones`
   - Drag **Slider** → **Rename to exactly**: `CheekbonesSlider`

   **Nose Width:**
   - Drag **Text Block** → Text: `Nose Width`
   - Drag **Slider** → **Rename to exactly**: `NoseWidthSlider`

   **Nose Length:**
   - Drag **Text Block** → Text: `Nose Length`
   - Drag **Slider** → **Rename to exactly**: `NoseLengthSlider`

   **Lip Thickness:**
   - Drag **Text Block** → Text: `Lip Thickness`
   - Drag **Slider** → **Rename to exactly**: `LipThicknessSlider`

   **Eye Size:**
   - Drag **Text Block** → Text: `Eye Size`
   - Drag **Slider** → **Rename to exactly**: `EyeSizeSlider`

5. **Add Eye Color Section**:
   - Drag **Spacer** → Size: 16
   - Drag **Text Block** → Text: `Eye Color` → Font: Bold

   **Red:**
   - Drag **Text Block** → Text: `Red`
   - Drag **Slider** → **Rename to exactly**: `EyeColorRedSlider`

   **Green:**
   - Drag **Text Block** → Text: `Green`
   - Drag **Slider** → **Rename to exactly**: `EyeColorGreenSlider`

   **Blue:**
   - Drag **Text Block** → Text: `Blue`
   - Drag **Slider** → **Rename to exactly**: `EyeColorBlueSlider`

### 5.3 Create Body Panel (Tab Index 2)

1. **Select `PhysicalTabSwitcher`** in Hierarchy

2. **Drag a Vertical Box** into `PhysicalTabSwitcher`
   - Rename to `BodyPanel`
   - Padding: 12 all sides

3. **Add Body Type Slider**:
   - Drag **Text Block** → Text: `Body Type`
   - Drag **Slider** → **Rename to exactly**: `BodyTypeSlider`

4. **Add Skin Tone Section**:
   - Drag **Spacer** → Size: 16
   - Drag **Text Block** → Text: `Skin Tone` → Font: Bold

   **Red:**
   - Drag **Text Block** → Text: `Red`
   - Drag **Slider** → **Rename to exactly**: `SkinRedSlider`

   **Green:**
   - Drag **Text Block** → Text: `Green`
   - Drag **Slider** → **Rename to exactly**: `SkinGreenSlider`

   **Blue:**
   - Drag **Text Block** → Text: `Blue`
   - Drag **Slider** → **Rename to exactly**: `SkinBlueSlider`

### 5.4 Create Hair Panel (Tab Index 3)

1. **Select `PhysicalTabSwitcher`** in Hierarchy

2. **Drag a Vertical Box** into `PhysicalTabSwitcher`
   - Rename to `HairPanel`
   - Padding: 12 all sides

3. **Add Hair Style Dropdown**:
   - Drag **Text Block** → Text: `Hair Style`
   - Drag **Combo Box (String)** → **Rename to exactly**: `HairStyleComboBox`

4. **Add Hair Color Section**:
   - Drag **Spacer** → Size: 16
   - Drag **Text Block** → Text: `Hair Color` → Font: Bold

   **Red:**
   - Drag **Text Block** → Text: `Red`
   - Drag **Slider** → **Rename to exactly**: `HairColorRedSlider`

   **Green:**
   - Drag **Text Block** → Text: `Green`
   - Drag **Slider** → **Rename to exactly**: `HairColorGreenSlider`

   **Blue:**
   - Drag **Text Block** → Text: `Blue`
   - Drag **Slider** → **Rename to exactly**: `HairColorBlueSlider`

---

## STEP 6: Build Center Panel (Preview)

1. **Select `CenterPanel`** in Hierarchy

2. **Drag an Image** widget into `CenterPanel`
   - **Rename to exactly**: `CharacterPreviewImage`
   - In Details:
     - Slot → Size: **Fill**
     - Slot → Horizontal Alignment: **Fill**
     - Slot → Vertical Alignment: **Fill**

---

## STEP 7: Build Right Panel (Affinities)

### 7.1 Add Playstyle Affinities Section

1. **Select `RightPanel`** in Hierarchy

2. **Drag a Vertical Box** → Rename to `PlaystyleSection`
   - Slot → Size: Fill
   - Padding: 12

3. **Inside `PlaystyleSection`**:

   a. **Section Header**:
      - Drag **Text Block** → Text: `Playstyle Affinities` → Font: Bold 14
   
   b. **Pool Text**:
      - Drag **Text Block** → **Rename to exactly**: `AffinityPoolText`
      - Text: `Available Points: 0`
   
   c. **List Container**:
      - Drag **Vertical Box** → **Rename to exactly**: `AffinityListContainer`
      - Slot → Size: Fill
   
   d. **Plus/Minus Buttons**:
      - Drag **Horizontal Box** → Rename to `PlaystyleButtonBar`
      
      - Drag **Button** into `PlaystyleButtonBar`
        - **Rename to exactly**: `AffinityMinusButton`
        - Add **Text Block** inside → Text: `-`
      
      - Drag **Spacer** into `PlaystyleButtonBar` → Size: 8
      
      - Drag **Button** into `PlaystyleButtonBar`
        - **Rename to exactly**: `AffinityPlusButton`
        - Add **Text Block** inside → Text: `+`

### 7.2 Add Spacer Between Sections

1. **Select `RightPanel`**
2. Drag **Spacer** → Size: 16

### 7.3 Add Magical Affinities Section

1. **Select `RightPanel`** in Hierarchy

2. **Drag a Vertical Box** → Rename to `MagicalSection`
   - Slot → Size: Fill
   - Padding: 12

3. **Inside `MagicalSection`**:

   a. **Section Header**:
      - Drag **Text Block** → Text: `Magical Affinities` → Font: Bold 14
   
   b. **Pool Text**:
      - Drag **Text Block** → **Rename to exactly**: `MagicalAffinityPoolText`
      - Text: `Available Points: 0`
   
   c. **List Container**:
      - Drag **Vertical Box** → **Rename to exactly**: `MagicalAffinityListContainer`
      - Slot → Size: Fill
   
   d. **Plus/Minus Buttons**:
      - Drag **Horizontal Box** → Rename to `MagicalButtonBar`
      
      - Drag **Button** into `MagicalButtonBar`
        - **Rename to exactly**: `MagicalAffinityMinusButton`
        - Add **Text Block** inside → Text: `-`
      
      - Drag **Spacer** → Size: 8
      
      - Drag **Button** into `MagicalButtonBar`
        - **Rename to exactly**: `MagicalAffinityPlusButton`
        - Add **Text Block** inside → Text: `+`

---

## STEP 8: Build Footer

1. **Select `FooterHBox`** in Hierarchy

2. **Add Mock Data Button** (optional):
   - Drag **Button** → **Rename to exactly**: `PopulateMockDataButton`
   - Add **Text Block** inside → Text: `Populate Mock Data`

3. **Add Spacer**:
   - Drag **Spacer** → Slot → Size: Fill (pushes submit to right)

4. **Add Submit Button**:
   - Drag **Button** → **Rename to exactly**: `SubmitButton`
   - Add **Text Block** inside → Text: `Submit Character`
   - Style: Make it prominent (green background recommended)

5. **Add Result Log**:
   - Drag **Spacer** → Size: 16
   - Drag **Text Block** → **Rename to exactly**: `ResultLogText`
   - Text: (leave empty - will be set by code)
   - Size: Slot → Size: Fill

---

## STEP 9: Configure Widget Defaults

1. **Click the root widget** (or click empty space in Designer)

2. **In Details panel** → scroll to **FPM | Character Preview** section:

   a. **Preview Render Target**:
      - Click dropdown → **Create New Asset** → **Texture Render Target 2D**
      - Name it: `RT_CharacterPreview`
      - **CRITICAL**: The creator dropdown doesn't set the size. Double-click the `RT_CharacterPreview` asset in the Content Browser.
      - In the RT asset editor, set **Size X**: `1024` and **Size Y**: `1024` (or 512).
      - **Save** the RT asset and return to the widget.

   b. **Preview Actor Spawn Location**:
      - Default `(10000, 10000, 0)` is usually fine.
      - This places the preview actor far from the game world.

   c. **Preview Mesh**:
      - Usually leave as **None** (the code will use the Mutable Customizable Object).

   d. **Customizable Object Asset**:
      - Click dropdown → Select your Customizable Object (e.g., `CO_TestBody`).

   e. **Preview Rotation/Zoom/Pan Sensitivity**:
      - Set Rotation to `0.5`, Zoom to `10.0`, Pan to `0.5`.


---

## STEP 10: Save and Compile

1. **Click Compile** (top left of Widget Designer)

2. **Click Save**

3. **Close Widget Designer**

---

## STEP 11: Test the Widget

### Option A: Test in PIE

1. Create a test level or use your character creation level
2. Add a Player Controller with logic to show this widget
3. Play in Editor

### Option B: Quick Test via Level Blueprint

1. Open your test level's Level Blueprint
2. Add nodes:
   ```
   Event BeginPlay →
   Create Widget (Class: WBP_CharacterCreationShell) →
   Add to Viewport
   ```
3. Play in Editor

---

## Verification Checklist

After testing, verify:

- [ ] Widget displays without crash
- [ ] All 4 tabs are visible (Identity, Face, Body, Hair)
- [ ] Clicking tabs switches content
- [ ] Selected tab text turns green
- [ ] Character preview appears in center
- [ ] Left-drag on preview rotates character
- [ ] Right-drag on preview pans camera
- [ ] Mouse scroll zooms preview
- [ ] Affinity sections visible in right panel
- [ ] +/- buttons are clickable
- [ ] Submit button is clickable

---

## Troubleshooting

### "Widget X is not bound" Error
- Open widget, find the widget mentioned
- Check the name matches EXACTLY (case-sensitive)
- Common issues:
  - Extra space in name
  - Wrong capitalization
  - Typo

### Tab Content Not Switching
- Verify `PhysicalTabSwitcher` is a **Widget Switcher** (not Switcher)
- Verify child order in Hierarchy matches: IdentityPanel, FacePanel, BodyPanel, HairPanel

### Preview Not Displaying
- Check `Preview Render Target` is assigned in Details
- Check `Customizable Object Asset` is assigned (or `Preview Render Target`)
- Check Output Log for errors

### Widget Crashes on Open
- Compile C++ code first
- Check for missing required widgets (marked with ✅ in earlier tables)

---

## Final Hierarchy Reference

Your Hierarchy panel should look like this:

```
WBP_CharacterCreationShell
└── RootVBox [Vertical Box]
    ├── MainContentHBox [Horizontal Box]
    │   ├── LeftPanel [Vertical Box]
    │   │   ├── TabButtonBar [Horizontal Box]
    │   │   │   ├── TabButtonIdentity [Button]
    │   │   │   │   └── TabTextIdentity [Text]
    │   │   │   ├── TabButtonFace [Button]
    │   │   │   │   └── TabTextFace [Text]
    │   │   │   ├── TabButtonBody [Button]
    │   │   │   │   └── TabTextBody [Text]
    │   │   │   └── TabButtonHair [Button]
    │   │   │       └── TabTextHair [Text]
    │   │   └── PhysicalTabSwitcher [Widget Switcher]
    │   │       ├── IdentityPanel [Vertical Box]
    │   │       │   ├── [Text] "Character Name"
    │   │       │   ├── NameInput [Editable Text Box]
    │   │       │   ├── [Spacer]
    │   │       │   ├── [Text] "Voice Type"
    │   │       │   └── VoiceTypeComboBox [Combo Box String]
    │   │       ├── FacePanelScroll [Scroll Box]
    │   │       │   └── FacePanel [Vertical Box]
    │   │       │       ├── [Text] "Face Width"
    │   │       │       ├── FaceWidthSlider [Slider]
    │   │       │       ├── ... (more face sliders)
    │   │       │       ├── [Text] "Eye Color"
    │   │       │       ├── EyeColorRedSlider [Slider]
    │   │       │       ├── EyeColorGreenSlider [Slider]
    │   │       │       └── EyeColorBlueSlider [Slider]
    │   │       ├── BodyPanel [Vertical Box]
    │   │       │   ├── [Text] "Body Type"
    │   │       │   ├── BodyTypeSlider [Slider]
    │   │       │   ├── [Text] "Skin Tone"
    │   │       │   ├── SkinRedSlider [Slider]
    │   │       │   ├── SkinGreenSlider [Slider]
    │   │       │   └── SkinBlueSlider [Slider]
    │   │       └── HairPanel [Vertical Box]
    │   │           ├── [Text] "Hair Style"
    │   │           ├── HairStyleComboBox [Combo Box String]
    │   │           ├── [Text] "Hair Color"
    │   │           ├── HairColorRedSlider [Slider]
    │   │           ├── HairColorGreenSlider [Slider]
    │   │           └── HairColorBlueSlider [Slider]
    │   ├── CenterPanel [Vertical Box]
    │   │   └── CharacterPreviewImage [Image]
    │   └── RightPanel [Vertical Box]
    │       ├── PlaystyleSection [Vertical Box]
    │       │   ├── [Text] "Playstyle Affinities"
    │       │   ├── AffinityPoolText [Text]
    │       │   ├── AffinityListContainer [Vertical Box]
    │       │   └── PlaystyleButtonBar [Horizontal Box]
    │       │       ├── AffinityMinusButton [Button]
    │       │       └── AffinityPlusButton [Button]
    │       ├── [Spacer]
    │       └── MagicalSection [Vertical Box]
    │           ├── [Text] "Magical Affinities"
    │           ├── MagicalAffinityPoolText [Text]
    │           ├── MagicalAffinityListContainer [Vertical Box]
    │           └── MagicalButtonBar [Horizontal Box]
    │               ├── MagicalAffinityMinusButton [Button]
    │               └── MagicalAffinityPlusButton [Button]
    └── FooterHBox [Horizontal Box]
        ├── PopulateMockDataButton [Button] (optional)
        ├── [Spacer]
        ├── SubmitButton [Button]
        └── ResultLogText [Text]
```
