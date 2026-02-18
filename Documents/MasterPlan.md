# Faldoran Prime MMO — Master Development Plan

**Status:** A consolidation of all prior phase plans into a single execution roadmap.
**Vision:** A "Light No Fire" inspired MMO emphasizing extreme verticality, species variety, and visual density.  
**Core Pillars:**
1.  **Vertical World:** Procedural terrain with massive scale (Alpine peaks, deep valleys).
2.  **Living Ecosystem:** Dense, biome-aware PCG population (flora/fauna).
3.  **Refined Characters:** CC5-based high-fidelity characters with distinct species scales (Giants to Smallfolk).
4.  **Immersive Systems:** Classless affinity progression and deep world interaction.

---

## 🟢 PHASE I: Foundation (COMPLETED)

These systems are implemented and functional.

### 1.1 Remote Database Infrastructure (Pillar 02)
-   **Status:** ✅ COMPLETE
-   **Outcome:** PostgreSQL running on dedicated server (Remote IP).
-   **Schema:** Basic character tables + `character_affinities` table.
-   **Access:** Configured for multi-client testing.

### 1.2 Basic Chunk System (Pillar 03 / New Plan)
-   **Status:** ✅ COMPLETE
-   **Outcome:** 
    -   `FPMChunkData`, `FPMChunkActor`, `FPMWorldChunkManager` classes implemented.
    -   Deterministic procedural generation foundation.
    -   Replaced static Landscape actor with runtime procedural mesh chunks.

### 1.3 CC5 Character Pipeline (Pillar 04A / 04B)
-   **Status:** ✅ COMPLETE
-   **Outcome:**
    -   Reallusion Auto Setup plugin integrated.
    -   Base CC5 Male/Female meshes imported with full morph targets (240+).
    -   `AFPMCharacterPreviewActor` implemented for creation screen with real-time morph/material updates.
    -   Lighting and camera controls for character preview.

---

## 🔵 PHASE II: The Living World (CURRENT FOCUS)

**Objective:** Transform the flat procedural terrain into a dramatic, vertical, and populated world, then showcase it immediately upon login.

### Step 2.1: Terrain 2.0 — "Extreme Verticality"
**Ref:** `Phase_Refactor_Plan.md` & `New_Plan.md`
-   [ ] **Refactor Height Calculation:** Modify `FPMTerrainGenerator::CalculateHeight` to use **Ridge Noise** (absolute value Perlin) for sharp peaks.
-   [ ] **Scale Increase:** Tune `MaxHeightCm` to allow for 1km+ vertical range.
-   [ ] **Biome Logic:** Ensure biomes map correctly to new steep slopes (e.g., rock faces on cliffs, snow only at high altitude).

### Step 2.2: Biome Population (PCG)
**Ref:** `Phase_Refactor_Plan.md`
-   [x] **PCG Spawner Creation:** Created `FPMBiomePCGSpawner` (HISM-based, C++ driven — more deterministic and performant than PCG Graph assets).
-   [x] **Chunk Integration:** Bound spawner to `FPMChunkActor`; generates *after* terrain mesh build at Full LOD only.
-   [x] **Config Data Asset:** Created `UFPMBiomePCGConfig` for artist-editable mesh/density settings.
-   [ ] **Density Pass:** Tune for "movie-quality" forest density (requires tree/rock static mesh assets imported into Content Browser).
-   **NOTE:** Requires tree/rock static meshes. Import meshes, create `DA_BiomePCGConfig` Data Asset, assign meshes, then assign to `AFPMWorldChunkManager`.

### Step 2.3: Visual Integration & Login
**Ref:** `Phase_Refactor_Plan.md`
-   [ ] **Showcase Chunk:** Place a `CameraRig_Rail` in a generated "Showcase Chunk" of the new terrain.
-   [ ] **Login Screen:** Update `FPMLoginWidget` / Level to use this live 3D background instead of static images.
-   [ ] **Lighting:** Ensure Day/Night cycle and Atmosphere look premium in this view.

---

## 🟡 PHASE III: Character Depth

**Objective:** Expand the character system to support diverse species sizes and detailed creation options.

### Step 3.1: The "Species" Refactor
**Ref:** `Phase_Refactor_Plan.md`
-   [ ] **Data Asset:** Create `DA_Species` (BaseHealth, WalkSpeed, Scale, MorphTargetName).
-   [ ] **Dynamic Scaling:** Modify `AFPCharacter` to apply mesh/capsule scale based on Species ID (Giant vs. Gnome).
-   [ ] **Verification:** Ensure animations retarget correctly at different scales.

### Step 3.2: Animation Pipeline (CC5)
**Ref:** `Pillar_04_CC5_Character_Creation.md` (Phase 4C)
-   [ ] **Locomotion:** Implement full Animation Blueprint (Idle -> Walk -> Run -> Sprint -> Jump).
-   [ ] **Retargeting:** Finalize IK Rigs for Mixamo/ActorCore to CC5 skeleton.
-   [ ] **Combat:** Add combat stance and basic attack animations.

### Step 3.3: Full Creation UI & Affinities
**Ref:** `Pillar_01_Character_Creation.md`
-   [ ] **UI Layout:** Build tabbed UI (Race, Body, Face, Hair, Skin, Affinities).
-   [ ] **Affinity Logic:** Connect `FPMCharacterCreationDataContract` to UI sliders for Playstyle/Magical affinities.
-   [ ] **Persist:** Ensure all new data (Species, Affinities, Appearance) saves correctly to the remote DB.

---

## ⚪ PHASE IV: Gameplay Loop (FUTURE)

**Objective:** Implement the core "Survive & Thrive" loop.

### 4.1 Gameplay Systems
-   **Inventory System:** Server-authoritative item management.
-   **Resource Gathering:** Mining, Woodcutting (interacting with PCG elements).
-   **Crafting:** Recipe system and UI.
-   **Building:** Player-placed structures in the open world.

---

## 📂 Reference & Resources

-   **Database Setup:** See `Workflows/Reference/Guide_Database_Setup.md` (formerly Pillar 02).
-   **CC5 Pipeline:** See `Workflows/Reference/Guide_CC5_Pipeline.md` (formerly Pillar 04).
-   **Legacy Plans:** See `Workflows/Archive/Legacy_Plans/` for old detailed breakdowns.
