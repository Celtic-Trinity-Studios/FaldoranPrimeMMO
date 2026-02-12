# Landscape Material Build Guide — Procedural Biome Auto-Paint

**Purpose:** Build `M_Landscape_StarterIsland`, a material that auto-paints biomes using world-position noise. No manual layer painting required.

**Prerequisites:**
- Textures imported into `Content/Textures/Landscape/` (5 BaseColor + 5 Normal)
- Landscape created in `L_StarterIsland`

---

## Step 1: Create the Material

1. **Content Browser** → right-click → **Material**
2. Name it **`M_Landscape_StarterIsland`**
3. Save to `Content/Materials/Landscape/`
4. **Double-click** to open the Material Editor

---

## Step 2: Material Settings

Click the **main output node** (big node on the right).

In the **Details Panel** on the left, set:
- **Blend Mode:** `Opaque`
- **Shading Model:** `Default Lit`
- **Two Sided:** ❌ off
- **Fully Rough:** ✅ on *(uncheck later for polish)*

---

## Step 3: Place the World Position Node

1. **Right-click** on empty graph space → type **`Absolute World Position`** → click it
2. The node appears with 3 output pins on its right side:
   - **XYZ** (white circle) — full 3D position
   - **XY** (greenish circle) — horizontal X and Y only
   - **Z** (blue circle) — height only
3. Drag this node to the **far left** of your graph — everything will flow from here

---

## Step 4: Normalize XY to 0–1 Range

This converts world centimeters into the same 0–1 range used by the C++ terrain generator.

1. **Drag a wire** from the **XY pin** → release on empty space → type **`Divide`** → click it
   - The XY pin auto-connects to the **A** input of the Divide node
2. **Right-click** empty space → type **`Constant`** → click it
3. Click the new Constant node → in **Details Panel** set **Value** = **`201600`**
   - *(32 components × 63 quads × 100 scale = 201,600 cm. Adjust if your landscape is different.)*
4. **Drag a wire** from the Constant output → connect to the **B** input of the Divide node

**Result:** The Divide output = `(NormX, NormY)` in 0–1 range.

Label this node mentally as **"NormalizedXY"**.

---

## Step 5: Biome Noise Layer 1 (Large Patches)

1. **Drag a wire** from the **NormalizedXY output** (the Divide from Step 4) → release → type **`Multiply`** → click it
   - NormalizedXY connects to the **A** input
2. **Right-click** → **`Constant`** → set Value = **`3.5`**
3. Connect 3.5 Constant → **B** input of the Multiply
4. **Drag a wire** from this Multiply's output → release → type **`Noise`** → click it
   - The Multiply output connects to the **Position** input of the Noise node
5. Click the **Noise** node → in **Details Panel** set:
   - **Scale:** `1.0`
   - **Quality:** `1`
   - **Noise Function:** `Value`
   - **Levels:** `3`
   - **Output Min:** `0.0`
   - **Output Max:** `1.0`

**Result:** This Noise node outputs the first biome selection value. Call it **"Noise1"**.

---

## Step 6: Biome Noise Layer 2 (Detail Variation)

1. **Drag a wire** from the **NormalizedXY output** (same Divide from Step 4) → release → type **`Multiply`** → click it
   - NormalizedXY connects to **A** input
2. **Right-click** → **`Constant`** → set Value = **`5.0`**
3. Connect 5.0 Constant → **B** input of the Multiply
4. Now we need a seed offset so this noise differs from Layer 1:
   - **Right-click** → **`Constant`** → set Value = **`7777`**
   - **Right-click** → type **`Add`** → click it
   - Connect the Multiply output (×5.0) → **A** input of Add
   - Connect 7777 Constant → **B** input of Add
5. **Drag a wire** from the **Add output** → release → type **`Noise`** → click it
   - Add output connects to **Position** input
6. Click the Noise node → **Details Panel**:
   - **Scale:** `1.0`
   - **Noise Function:** `Value`
   - **Levels:** `2`
   - **Output Min:** `0.0`
   - **Output Max:** `1.0`

**Result:** This outputs the second biome value. Call it **"Noise2"**.

---

## Step 7: Combine Noise into BiomeValue

1. **Right-click** → **`Multiply`** → place it
2. Connect **Noise1 output** → **A** input
3. **Right-click** → **`Constant`** → Value = **`0.6`** → connect to **B** input

4. **Right-click** → **`Multiply`** → place another one
5. Connect **Noise2 output** → **A** input
6. **Right-click** → **`Constant`** → Value = **`0.4`** → connect to **B** input

7. **Right-click** → **`Add`** → place it
8. Connect the (Noise1 × 0.6) Multiply output → **A** input
9. Connect the (Noise2 × 0.4) Multiply output → **B** input

**Result:** The Add output = **BiomeValue** (0 to 1). This is the master selector.

---

## Step 8: Create Texture Samplers

For each of the 5 biomes, create one texture sampler.
**Repeat these sub-steps 5 times** (once for Grass, Forest, Rock, Sand, Snow):

### 8A: Texture Coordinate (Tiling)

1. **Right-click** → type **`TextureCoordinate`** → click it
2. Click it → **Details Panel**:
   - **UTiling:** `64`
   - **VTiling:** `64`
   - *(This tiles the texture 64x across the landscape so it looks detailed up close)*

### 8B: Texture Sampler

1. **Right-click** → type **`TextureSampleParameter2D`** → click it
2. Click it → **Details Panel**:
   - **Parameter Name:** Set to one of: `T_Grass`, `T_Forest`, `T_Rock`, `T_Sand`, `T_Snow`
   - **Texture:** Click the dropdown → select the matching BaseColor texture you imported
3. **Connect** the TextureCoordinate output → the **UVs** input pin (the very top input on the left side of the TextureSampleParameter2D)

### Pin Layout of TextureSampleParameter2D:
```
Inputs (left side):          Outputs (right side):
  UVs ●                         ● RGB  (top - white - USE THIS)
                                 ● R    (red)
                                 ● G    (green)
                                 ● B    (blue)
                                 ● A    (alpha)
```

**You only need the RGB output pin** (the topmost output, white colored) for base color.

### After creating all 5:

Arrange them vertically on the left side of the graph. You should have:
```
[TextureCoord] → [T_Grass]
[TextureCoord] → [T_Forest]
[TextureCoord] → [T_Rock]
[TextureCoord] → [T_Sand]
[TextureCoord] → [T_Snow]
```

> **Tip:** You can share ONE TextureCoordinate node for all 5 samplers — just drag 5 wires from its output to each sampler's UVs input.

---

## Step 9: Biome Selection with If Nodes

### The If Node — How It Works:
```
Inputs (left side):         Output (right side):
  A ●                           ● Result
  B ●
  A>B ●  ← shown when A > B
  A==B ● ← shown when A equals B
  A<B ●  ← shown when A < B
```

### 9A: Inner If — Forest vs Mountain (threshold 0.75)

1. **Right-click** → type **`If`** → place it
2. Connect **BiomeValue** (the Add output from Step 7) → **A** input
3. **Right-click** → **`Constant`** → Value = **`0.75`** → connect to **B** input
4. Connect **T_Forest** texture **RGB** output (topmost white pin) → **A<B** input
   *(BiomeValue < 0.75 = Forest)*
5. Connect **T_Rock** texture **RGB** output → **A>B** input
   *(BiomeValue > 0.75 = Mountain/Rock)*
6. Connect **T_Rock** texture **RGB** output → **A==B** input as well

Call this node **"If_ForestOrMountain"**. Its output = Forest or Rock texture.

### 9B: Outer If — Meadows vs Everything Else (threshold 0.40)

1. **Right-click** → type **`If`** → place another one
2. Connect **BiomeValue** → **A** input
3. **Right-click** → **`Constant`** → Value = **`0.40`** → connect to **B** input
4. Connect **T_Grass** texture **RGB** output → **A<B** input
   *(BiomeValue < 0.40 = Meadows/Grass)*
5. Connect **If_ForestOrMountain output** → **A>B** input
   *(BiomeValue > 0.40 = use the Forest/Mountain result)*
6. Connect **If_ForestOrMountain output** → **A==B** input

Call this node **"If_BiomeSelect"**. Its output = the biome-selected base color.

---

## Step 10: Slope Blending (Rock on Steep Surfaces)

Steep slopes should show rock regardless of which biome noise says.

1. **Right-click** → type **`VertexNormalWS`** → place it
   - This node has one output: the surface normal vector
2. **Drag a wire** from its output → release → type **`ComponentMask`** → click it
3. Click the ComponentMask → in **Details Panel**:
   - **R:** ❌ unchecked
   - **G:** ❌ unchecked
   - **B:** ✅ checked *(B = Z axis = how "flat" the surface is)*
4. **Drag a wire** from ComponentMask output → release → type **`Abs`** → click it
   *(Abs = absolute value, ensures we get a positive number)*

**How this value works:**
- Flat ground → Z normal ≈ 1.0
- Steep cliff → Z normal ≈ 0.0

5. **Right-click** → type **`LinearInterpolate`** (or just type **`Lerp`**) → place it
6. Connect wires:
   - **T_Rock** texture RGB output → **A** input of the Lerp
   - **If_BiomeSelect** output (from Step 9B) → **B** input of the Lerp
   - **Abs** output (slope value) → **Alpha** input of the Lerp

**How this blends:**
- Alpha = 0 (steep cliff) → shows A (Rock)
- Alpha = 1 (flat ground) → shows B (biome texture)

Call this output **"SlopeBlended"**.

---

## Step 11: Coast Blending (Sand at Island Edges)

1. **Drag a wire** from the **NormalizedXY output** (Divide from Step 4) → release → type **`Distance`** → click it
   - NormalizedXY connects to the **A** input
2. **Right-click** → type **`Constant2Vector`** → place it
   - Click it → **Details Panel:** set **R** = `0.5`, **G** = `0.5`
   - *(This is the island center in normalized coordinates)*
3. Connect Constant2Vector output → **B** input of the Distance node

4. **Drag a wire** from Distance output → release → type **`Divide`** → click it
   - Distance connects to **A** input
5. **Right-click** → **`Constant`** → Value = **`0.40`** → connect to **B** input
   *(0.40 = island radius, matches C++ IslandMask radius)*

6. **Drag a wire** from this Divide output → release → type **`SmoothStep`** → click it
   - Divide output connects to the **Value** input (bottom pin)
7. **Right-click** → **`Constant`** → Value = **`0.75`** → connect to **Min** input (top pin)
8. **Right-click** → **`Constant`** → Value = **`1.0`** → connect to **Max** input (middle pin)

**SmoothStep pin layout:**
```
Inputs (left):              Output (right):
  Min ●                         ● Result
  Max ●
  Value ●
```

9. **Right-click** → type **`Lerp`** → place it
10. Connect:
    - **SlopeBlended** output (Step 10) → **A** input
    - **T_Sand** texture RGB output → **B** input
    - **SmoothStep** output → **Alpha** input

**How this blends:**
- Alpha = 0 (center of island) → shows A (biome/slope texture)
- Alpha = 1 (edge of island) → shows B (Sand)

Call this output **"CoastBlended"**.

---

## Step 12: Snow at High Altitude

1. **Drag a wire** from the **Z pin** (blue) on the Absolute World Position node → release → type **`Divide`** → click it
   - Z connects to **A** input
2. **Right-click** → **`Constant`** → Value = **`25000`** → connect to **B** input
   *(25000 cm = 250m. This is the snow line height. Adjust after seeing the terrain.)*

3. **Drag a wire** from Divide output → type **`SmoothStep`** → click it
4. **Right-click** → **`Constant`** → Value = **`0.8`** → connect to **Min** input
5. **Right-click** → **`Constant`** → Value = **`1.0`** → connect to **Max** input
6. Connect the Divide output → **Value** input

7. **Right-click** → type **`Lerp`** → place it
8. Connect:
   - **CoastBlended** output (Step 11) → **A** input
   - **T_Snow** texture RGB output → **B** input
   - **SmoothStep** output → **Alpha** input

Call this output **"FinalColor"**.

---

## Step 13: Connect to Material Output

The main material output node (far right) has these input pins:
```
  Base Color ●
  Metallic ●
  Specular ●
  Roughness ●
  Normal ●
  ...etc
```

1. Connect **FinalColor** (the last Lerp output from Step 12) → **Base Color** input
2. **Right-click** → **`Constant`** → Value = **`0.5`** → connect to **Roughness** input
3. **Right-click** → **`Constant3Vector`** → set to **(0, 0, 1)** → connect to **Normal** input
   *(Flat normal for now — we can add per-biome normals later)*

---

## Step 14: Apply and Save

1. Click **Apply** in the Material Editor toolbar (top left area)
2. Click **Save** (or `Ctrl+S`)
3. Close the Material Editor

---

## Step 15: Assign Material to Landscape

1. In the **Outliner** panel (usually top right), click on your **Landscape** actor
2. In the **Details Panel** (right side), scroll down to find **Landscape Material**
3. Click the dropdown → search for **`M_Landscape_StarterIsland`** → select it
4. The landscape should immediately repaint itself based on world-position noise

---

## Complete Node Graph Summary

```
[Absolute World Position]
   ├── XY pin ─→ [Divide ÷ 201600] = NormalizedXY
   │                ├─→ [× 3.5] → [Noise (Val,3lvl)] = Noise1 ─→ [× 0.6] ──┐
   │                └─→ [× 5.0] → [+ 7777] → [Noise (Val,2lvl)] = Noise2 ─→ [× 0.4] ──┤
   │                                                                                      ↓
   │                                                                                   [Add] = BiomeValue
   │                                                                                      │
   │   [T_Grass RGB] ──────────────────────────────────────────→ If(BV<0.40) ─┐
   │   [T_Forest RGB] ──→ If(BV<0.75) ─┐                                      │
   │   [T_Rock RGB] ───→ If(BV>0.75) ──┤→ If_ForestOrMtn ──→ If(BV>0.40) ────┤
   │                                                                           ↓
   │                                                                    If_BiomeSelect
   │                                                                           │
   │   [VertexNormalWS] → [Mask B] → [Abs] ──────────────→ Lerp Alpha         │
   │   [T_Rock RGB] ──────────────────────────────────────→ Lerp A             │
   │                                                        Lerp B ←───────────┘
   │                                                           │
   │                                                    SlopeBlended
   │                                                           │
   │   NormalizedXY → [Distance to (0.5,0.5)] → [÷ 0.4] → [SmoothStep 0.75-1.0]
   │   [T_Sand RGB] ──────────────────────────────────────→ Lerp B             │
   │                                                        Lerp A ←───────────┘
   │                                                        Lerp Alpha ← SmoothStep
   │                                                           │
   │                                                    CoastBlended
   │                                                           │
   └── Z pin ──→ [Divide ÷ 25000] → [SmoothStep 0.8-1.0] → Lerp Alpha        │
       [T_Snow RGB] ──────────────────────────────────────→ Lerp B             │
                                                            Lerp A ←───────────┘
                                                               │
                                                          FinalColor
                                                               │
                                                     ┌─────────┘
                                                     ↓
                                              [Base Color] on Material Output
                                              [Constant 0.5] → Roughness
                                              [Constant3 (0,0,1)] → Normal
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Entire landscape is one solid color | The 201600 constant is wrong. Select Landscape → Details → check its actual world bounds. Use `MaxX - MinX` in cm. |
| Textures are blurry up close | Increase UTiling/VTiling on TextureCoordinate (try 128 or 256) |
| Biome patches are too huge | Increase the BiomeScale constants: change 3.5 → 6.0, change 5.0 → 8.0 |
| Biome patches are too tiny/noisy | Decrease the BiomeScale constants: change 3.5 → 2.0, change 5.0 → 3.0 |
| Snow covers too much | Increase the snow Divide constant (25000 → 35000) |
| Snow covers too little | Decrease it (25000 → 15000) |
| Coast sand ring too wide | Increase SmoothStep Min (0.75 → 0.85) |
| Coast sand ring too narrow | Decrease SmoothStep Min (0.75 → 0.65) |
| Rock doesn't show on cliffs | Check VertexNormalWS → ComponentMask has ONLY B checked, and the Lerp A/B aren't swapped |
| Material is black/wrong | Hit Apply, verify all connections. Ensure textures have valid assets assigned |

---

## Future Improvements (Not Now)

- Wire up per-biome Normal maps for surface detail
- Add roughness variation per biome
- Add macro-variation noise to break up tiling
- Add wetness darkening near rivers
- Add Swamp biome blending using a third noise layer

---

*Created: 2026-02-10*
*Engine: UE 5.7.1 (Source Build)*
