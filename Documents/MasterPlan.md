# Faldoran Prime MMO — Master Plan

**Vision:** Light No Fire-inspired MMO. Extreme verticality, PCG ecosystems, classless depth.  
**Studio:** Celtic Trinity Studios | **Engine:** UE 5.7.1 Custom | **Stack:** C++ / PostgreSQL / Dedicated Server

> Completed work → [DONE.md](file:///e:/FaldoranPrimeMMO/Documents/DONE.md)

---

## Current Sprint — The Living World

### 🏙️ Nexus System (Starter Cities)
- [x] **CODE:** `AFPMNexusManager` — singleton, loads from `WorldGen.ini [Nexus]`
- [x] **CODE:** New characters always spawn at Nexus; returning players restore saved position
- [x] **CODE:** PCG vegetation suppressed inside Nexus safe radius (512m)
- [x] **CONFIG:** `WorldGen.ini [Nexus]` — Aelvorn Nexus defined at origin
- [x] **DB SCHEMA:** `spawn_x / spawn_y / spawn_z` columns added to `characters`
- [ ] **DB MIGRATE:** Run [Migration_002_AddNexusSpawnPos.sql](file:///e:/FaldoranPrimeMMO/Documents/Technical/Migration_002_AddNexusSpawnPos.sql) in pgAdmin
- [ ] **CODE:** Save player position on logout / disconnect (update `spawn_x/y/z` in DB)
- [ ] **EDITOR:** Place Nexus marker/landmark in the world at (0,0) — placeholder rocks/ruins

### 🌿 PCG Population Polish
- [ ] **EDITOR:** Import tree/rock meshes → assign to `DA_BiomePCGConfig`

### 🌅 Login Level
- [ ] **EDITOR:** Create `Content/Maps/L_LoginLevel`, set as Game Default Map
- [ ] **EDITOR:** Import splash art → assign to `BackgroundTexture` on `WBP_LoginScreen`

---

## Near Horizon — Character Depth

### Species Data Assets
- [ ] **EDITOR:** Create `DA_SpeciesRegistry` + one `DA_Species_*` per species in `Content/Data/Species/`
- [ ] Assign registry to `BP_PlayerCharacter` class defaults

### Animation & Locomotion
- [ ] **EDITOR:** Build `ABP_CC5_Character` for CC5 skeleton
- [ ] State machine: Idle → Walk → Run → Jump
- [ ] IK Retarget across species heights

### Inventory UI
- [ ] `UFPMInventoryWidget`: 8×5 grid, drag-and-drop, rarity borders
- [ ] `ToggleInventory()` shows/hides widget (I key)
- [ ] Save changes to PostgreSQL via `FPMDatabaseSubsystem`

### Character Creation
- [ ] `FFPMCharacterCreationRequest` / `UFPMCharacterCreationSubsystem` code
- [ ] Tabbed UI: Race, Body, Face, Style, Affinities
- [ ] DB persistence: `characters` table, name uniqueness, one-per-account check

---

## Roadmap — Gameplay Loops

### Crafting & Construction
- [ ] `FFPMCraftingRecipe` data-driven system; 3+ starter recipes
- [ ] Ghost placement building system with grid snap + server validation

### Combat & AI
- [ ] Affinity → stats pipeline (6 Playstyle + 8 Magical)
- [ ] Biome-based NPC spawning; safe-zone radius from Nexuses (AFPMNexusManager::IsInNexusSafeZone)

---

## Reference
- [DONE.md](file:///e:/FaldoranPrimeMMO/Documents/DONE.md) — all completed work
- [Agent Prompt Library](file:///e:/FaldoranPrimeMMO/Documents/Workflows/Agent_Prompt_Library.md) — ready-to-paste agent prompts
- [Database Schema](file:///e:/FaldoranPrimeMMO/Documents/Technical/Database_Schema_v1.sql)
- **F key** = toggle flight in PIE

*Copyright Celtic Trinity Studios, 2026.*
