# Pillar 01 — Full Character Creation

**Goal:** A player should be able to create a character that looks distinct, has a name, and has starting affinities — with a 3D preview during creation.

**Status:** Planned / In Progress

---

## Phase 1A: Affinity System Integration

The game uses a **classless** system with **two fully adjustable affinity pools** — no primary/secondary distinction. All affinities are freely distributable at character creation:

**Playstyle Affinities** (6 categories):
- Martial, Ranged, Magic, Crafting, Social, Survival

**Magical Affinities** (8 elements):
- Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane

**Implementation:**
- Add both affinity pools to `FFPMCharacterCreationRequest`
- Validator enforces: each pool's total = its point budget, each value ≥ 0, no individual value exceeds max
- Persist to a `character_affinities` table (type + points per character)
- UI: Zero-sum point allocation sliders for each pool
- Per [03_Progression_and_Attributes.md](file:///e:/FaldoranPrimeMMO/Documents/Design/03_Progression_and_Attributes.md)

---

## Phase 1B: 3D Character Preview

- Create `AFPMCharacterPreviewActor` — a client-only actor for the creation screen
- Spawned in a dedicated "preview" scene or sublevel
- Updates appearance in real-time as sliders change (skin tone material, hair mesh swap)
- Camera orbits the preview (mouse drag to rotate)
- Consider [Mutable/CC5 integration](file:///e:/FaldoranPrimeMMO/Documents/Workflows/Reference/CC5_Mutable_Integration_Guide.md) or use material parameter changes on the mannequin as a stepping stone

---

## Phase 1C: Expanded Appearance Options

- Facial features (morph targets or mesh swaps, up to 8 slots per `Character_Creation_System.md`)
- Voice type selection (index for now, audio preview later)
- Better UI layout — possibly the tabbed design from [Character_Creation_Tabbed_UI_Guide.md](file:///e:/FaldoranPrimeMMO/Documents/Workflows/Reference/Character_Creation_Tabbed_UI_Guide.md)

---

## Phase 1D: Database Schema Updates

```sql
-- Affinities table (covers both Playstyle and Magical pools)
-- affinity_pool: 0 = Playstyle, 1 = Magical
-- affinity_type: index within the pool (e.g., 0=Martial, 1=Ranged, ...)
CREATE TABLE character_affinities (
    character_id  UUID REFERENCES characters(character_id) ON DELETE CASCADE,
    affinity_pool SMALLINT NOT NULL,
    affinity_type SMALLINT NOT NULL,
    points        SMALLINT NOT NULL DEFAULT 0,
    PRIMARY KEY (character_id, affinity_pool, affinity_type)
);

-- Expanded appearance columns on characters
ALTER TABLE characters ADD COLUMN facial_features SMALLINT[] DEFAULT '{}';
ALTER TABLE characters ADD COLUMN voice_type SMALLINT NOT NULL DEFAULT 0;
```

> [!NOTE]
> A separate affinities table is the right call since affinities change through gameplay progression (equipment, skill use, quest rewards). The `affinity_pool` column cleanly separates the two pools without needing separate tables.

---

## Agent Prompts — Pillar 1

### Phase 1A --- 01. Affinity Backend
```
CONVERSATION TITLE: Pillar 01, Phase 1A --- 01. Affinity Backend

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Design/03_Progression_and_Attributes.md for the affinity system design.
Read the file Documents/Workflows/Archive/00_Prototype_Plan.md for prototype context.
Read the file Documents/Workflows/Pillar_01_Character_Creation.md for the current plan.

TASK: Add both affinity pools (Playstyle + Magical) to the character creation backend.

The system is classless — NO primary/secondary distinction. ALL affinities are fully adjustable.

Playstyle Affinities (6): Martial, Ranged, Magic, Crafting, Social, Survival
Magical Affinities (8): Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane

Prerequisites: Phases 0-6 from prototype are complete. UFPMCharacterCreationSubsystem, FPMCharacterCreationDataContract, and FPMCharacterCreationValidator exist.

Do this in micro-steps, one at a time, each must compile:
1. Update FPMCharacterCreationDataContract.h:
   - Add EFPMPlaystyleAffinity enum (Martial, Ranged, Magic, Crafting, Social, Survival)
   - Add EFPMMagicalAffinity enum (Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane)
   - Add TMap<EFPMPlaystyleAffinity, int32> PlaystyleAffinities to FFPMCharacterCreationRequest
   - Add TMap<EFPMMagicalAffinity, int32> MagicalAffinities to FFPMCharacterCreationRequest
2. Update FPMCharacterCreationValidator:
   - ValidatePlaystyleAffinities(): total = point budget, each value >= 0, no value exceeds max
   - ValidateMagicalAffinities(): same rules for the magical pool
   - Add both to ValidateRequest()
3. Create the character_affinities table (provide SQL for pgAdmin):
   - character_id UUID FK, affinity_pool SMALLINT, affinity_type SMALLINT, points SMALLINT
   - PRIMARY KEY (character_id, affinity_pool, affinity_type)
4. Update UFPMCharacterCreationSubsystem::SubmitCharacterCreation() to persist affinities
5. Add console test: FPM.TestCreateCharacterWithAffinities
6. Compile and test

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Server authority. Never trust client.
```

### Phase 1B --- 02. Character Preview
```
CONVERSATION TITLE: Pillar 01, Phase 1B --- 02. Character Preview

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Technical/Character_Creation_System.md for architecture.
Read the file Documents/Workflows/Pillar_01_Character_Creation.md for the current plan.

TASK: Create a 3D character preview actor for the character creation screen.

Prerequisites: Pillar 01, Phase 1A complete. Affinity system integrated.

Do this in micro-steps, one at a time, each must compile:
1. Create AFPMCharacterPreviewActor (inherits AActor) in Source/FaldoranPrimeMMO/Public/Character/Preview/ and Private/Character/Preview/
   - Client-only actor (never replicated)
   - USkeletalMeshComponent with UE5 mannequin mesh
   - UMaterialInstanceDynamic for skin tone changes
   - Functions: SetSkinTone(FLinearColor), SetHairStyle(uint8), SetHairColor(FLinearColor), SetBodyType(uint8)
   - Each setter updates the visual immediately
2. Create a preview scene setup:
   - USceneCaptureComponent2D or a dedicated sublevel camera
   - Lighting for the preview (point light + fill light)
3. Add camera orbit controls:
   - Mouse drag rotates the preview actor around Y axis
   - Mouse wheel zooms
4. Wire up to UFPMCharacterCreationWidget:
   - When slider values change, call corresponding setter on the preview actor
   - Preview updates in real-time
5. Compile and test: open character creation → changing sliders updates the 3D preview

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. This is client-only code — no server authority needed.
```

### Phase 1C --- 03. Expanded Appearance
```
CONVERSATION TITLE: Pillar 01, Phase 1C --- 03. Expanded Appearance

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Technical/Character_Creation_System.md for appearance spec.
Read the file Documents/Workflows/Pillar_01_Character_Creation.md for the current plan.

TASK: Expand the character appearance system with facial features and voice type, plus improve the creation UI layout.

Prerequisites: Pillar 01, Phases 1A-1B complete. Preview actor works.

Do this in micro-steps, one at a time, each must compile:
1. Update FFPMCharacterCreationRequest:
   - Add FacialFeatures (TArray<uint8>, max 8 entries)
   - Add VoiceType (uint8)
2. Update FPMCharacterCreationValidator:
   - ValidateFacialFeatures(): max 8 entries, each valid index
   - ValidateVoiceType(): valid index range
3. Update database: ALTER TABLE characters ADD COLUMN facial_features SMALLINT[] DEFAULT '{}'; ALTER TABLE characters ADD COLUMN voice_type SMALLINT NOT NULL DEFAULT 0;
4. Update UFPMCharacterCreationSubsystem to persist new fields
5. Update AFPMCharacterPreviewActor to apply facial features (morph targets) and display voice type
6. Update UFPMCharacterCreationWidget with new controls:
   - Facial feature sliders/selectors
   - Voice type dropdown
   - Consider reorganizing into tabbed layout
7. Update AFPMPlayerCharacter replicated properties to include facial_features and voice_type
8. Compile and test end-to-end: create character with full appearance → verify in DB → spawn → verify replicated

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Server authority on validation.
```
