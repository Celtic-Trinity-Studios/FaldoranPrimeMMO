# Character Creation Screen — Widget Layout

## Three-Column Layout Wireframe

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                              ═══ CREATE YOUR CHARACTER ═══                                      │
├──────────────────────────┬────────────────────────────┬─────────────────────────────────────────┤
│   PHYSICAL ATTRIBUTES    │    CHARACTER PREVIEW        │          AFFINITIES                     │
│                          │                            │                                         │
│  Character Name:         │  ┌──────────────────────┐  │  ── Playstyle (Total: 600) ──           │
│  ┌────────────────────┐  │  │                      │  │                                         │
│  │ [NameInput]        │  │  │                      │  │  Martial:  ──●────────── [100]           │
│  └────────────────────┘  │  │                      │  │  Ranged:   ──●────────── [100]           │
│                          │  │     (Mannequin)       │  │  Magic:    ──●────────── [100]           │
│  Body Type:              │  │                      │  │  Crafting:  ──●────────── [100]           │
│  ──────●──────── [0-3]   │  │     3D Preview       │  │  Social:   ──●────────── [100]           │
│  [BodyTypeSlider]        │  │     Placeholder      │  │  Survival: ──●────────── [100]           │
│                          │  │                      │  │                                         │
│  Skin Color:             │  │                      │  │  ── Magical (Total: 800) ──              │
│  R: ────●──────── [0.8]  │  │                      │  │                                         │
│  G: ────●──────── [0.6]  │  │                      │  │  Fire:     ──●────────── [100]           │
│  B: ────●──────── [0.5]  │  └──────────────────────┘  │  Water:    ──●────────── [100]           │
│  [SkinRedSlider]         │                            │  Earth:    ──●────────── [100]           │
│  [SkinGreenSlider]       │                            │  Air:      ──●────────── [100]           │
│  [SkinBlueSlider]        │                            │  Light:    ──●────────── [100]           │
│                          │                            │  Shadow:   ──●────────── [100]           │
│  Hair Style:             │                            │  Nature:   ──●────────── [100]           │
│  ┌──────────────────┐    │                            │  Arcane:   ──●────────── [100]           │
│  │ Bald          ▼  │    │                            │                                         │
│  └──────────────────┘    │                            │  Points are zero-sum:                    │
│  [HairStyleComboBox]     │                            │  Raising one lowers others.              │
│                          │                            │  Min: 90  Max: 150 (playstyle)           │
│  Hair Color:             │                            │  (Magical: separate pool)                │
│  R: ────●──────── [0.2]  │                            │                                         │
│  G: ────●──────── [0.15] │                            │                                         │
│  B: ────●──────── [0.1]  │                            │                                         │
│  [HairColorRedSlider]    │                            │                                         │
│  [HairColorGreenSlider]  │                            │                                         │
│  [HairColorBlueSlider]   │                            │                                         │
├──────────────────────────┴────────────────────────────┴─────────────────────────────────────────┤
│                                                                                                 │
│   [BackButton]                    [ResultText: "Ready"]                    [SubmitButton]        │
│                                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Widget Names for BindWidget (C++ ↔ Blueprint)

### Left Column — Physical Attributes
| Widget Type          | Exact Name            | Purpose                          |
|---------------------|-----------------------|----------------------------------|
| EditableTextBox     | `NameInput`           | Character name entry             |
| Slider              | `BodyTypeSlider`      | Body type 0-3                    |
| Slider              | `SkinRedSlider`       | Skin color R (0.0-1.0)          |
| Slider              | `SkinGreenSlider`     | Skin color G (0.0-1.0)          |
| Slider              | `SkinBlueSlider`      | Skin color B (0.0-1.0)          |
| ComboBoxString      | `HairStyleComboBox`   | Hair style dropdown              |
| Slider              | `HairColorRedSlider`  | Hair color R (0.0-1.0)          |
| Slider              | `HairColorGreenSlider`| Hair color G (0.0-1.0)          |
| Slider              | `HairColorBlueSlider` | Hair color B (0.0-1.0)          |

### Bottom Row — Actions
| Widget Type | Exact Name      | Purpose                                |
|------------|-----------------|----------------------------------------|
| Button     | `SubmitButton`  | Submit creation request to server      |
| Button     | `BackButton`    | Return to login screen                 |
| TextBlock  | `ResultText`    | Status/error message display           |

### Right Column — Affinities (FUTURE — Not in prototype C++)
These are shown in the mockup for vision, but the PROTOTYPE uses defaults.
Post-prototype, these will be added as BindWidgets:

| Widget Type | Name (planned)           | Purpose |
|------------|--------------------------|---------|
| Slider     | `MartialAffinitySlider`  | Playstyle affinity 90-150 |
| Slider     | `RangedAffinitySlider`   | Playstyle affinity 90-150 |
| Slider     | `MagicAffinitySlider`    | Playstyle affinity 90-150 |
| Slider     | `CraftingAffinitySlider` | Playstyle affinity 90-150 |
| Slider     | `SocialAffinitySlider`   | Playstyle affinity 90-150 |
| Slider     | `SurvivalAffinitySlider` | Playstyle affinity 90-150 |
| Slider     | `FireAffinitySlider`     | Magical affinity |
| Slider     | `WaterAffinitySlider`    | Magical affinity |
| Slider     | `EarthAffinitySlider`    | Magical affinity |
| Slider     | `AirAffinitySlider`      | Magical affinity |
| Slider     | `LightAffinitySlider`    | Magical affinity |
| Slider     | `ShadowAffinitySlider`   | Magical affinity |
| Slider     | `NatureAffinitySlider`   | Magical affinity |
| Slider     | `ArcaneAffinitySlider`   | Magical affinity |

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
