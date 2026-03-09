# Faldoran Prime MMO — Master Plan

**Vision:** Light No Fire-inspired MMO. Earth-scale spherical planet, PCG ecosystems, classless depth.  
**Studio:** Celtic Trinity Studios | **Engine:** UE 5.7.1 Custom | **Stack:** C++ / PostgreSQL / Dedicated Server

> Completed work → [DONE.md](file:///e:/FaldoranPrimeMMO/Documents/DONE.md)

---

## 🌍 Spherical Planet Architecture
- [x] **CODE:** `FFPMGeoCoord` — double-precision geodetic coordinate struct (Lat/Lon/Alt)
- [x] **CODE:** `FPMChunkConstants` — Earth-scale (R=6,371km, C≈40,075km), adaptive lat-band chunk grid
- [x] **CODE:** Geodetic ↔ local tangent-plane conversion functions
- [x] **CODE:** Latitude-based temperature gradient (equator hot, poles cold)
- [x] **CODE:** Haversine great-circle distance for `FFPMGeoCoord`
- [x] **CODE:** 3D sphere-projected noise coordinates (`GeoToNoiseCoord`)
- [x] **CODE:** Legacy flat-world → geodetic migration helper (`FlatWorldToGeo`)
- [x] **DOCS:** [SphericalPlanet_Migration.md](file:///e:/FaldoranPrimeMMO/Documents/Technical/SphericalPlanet_Migration.md)
- [ ] **CODE (Phase 2):** Noise sampling via 3D sphere projection end-to-end (bypass flat XY)
- [ ] **CODE (Phase 2):** WorldChunkManager tracks player as `FFPMGeoCoord`
- [ ] **CODE (Phase 2):** Chunk gather uses adaptive lon-cells per latitude band
- [ ] **CODE (Phase 3):** Orbital camera / space transition at 100km altitude
- [ ] **CODE (Phase 3):** Planet-scale LOD shell visible from orbit
- [ ] **DB MIGRATE:** Add `spawn_lat / spawn_lon / spawn_alt` columns (Phase 2)

---

## Current Sprint — The Living World

### 🏙️ Nexus System (Starter Cities)
- [x] **CODE:** `AFPMNexusManager` — singleton, loads from `WorldGen.ini [Nexus]`
- [x] **CODE:** New characters always spawn at Nexus; returning players restore saved position
- [x] **CODE:** PCG vegetation suppressed inside Nexus safe radius (512m)
- [x] **CONFIG:** `WorldGen.ini [Nexus]` — Aelvorn Nexus defined at origin
- [x] **DB SCHEMA:** `spawn_x / spawn_y / spawn_z` columns added to `characters`
- [x] **DB MIGRATE:** Run [Migration_002_AddNexusSpawnPos.sql](file:///e:/FaldoranPrimeMMO/Documents/Technical/Migration_002_AddNexusSpawnPos.sql) in HeidiSQL
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

### Inventory UI — Tetris-style Grid
- [x] **CODE:** `UFPMInventoryComponent` — 2D grid backend (8×5), AABB placement, stacking, Server RPC move
- [x] **CODE:** `FFPMInventoryItem` — positioned item struct with `GridX/Y`, `SizeX/Y` footprint
- [x] **CODE:** `FFPMItemDefinition` — `SizeX/Y` grid footprint fields added
- [x] **DB MIGRATE:** [Migration_003_CreateInventoryTable.sql](file:///e:/FaldoranPrimeMMO/Documents/Technical/Migration_003_CreateInventoryTable.sql) ← **run this in HeidiSQL**
- [ ] **CODE:** `LoadInventoryFromDB` / `SaveInventoryToDB` — persist on login/logout via `FPMDatabaseSubsystem`
- [ ] **EDITOR:** `WBP_InventoryGrid` — UMG widget: renders grid, drag-and-drop, rarity borders
- [ ] **EDITOR:** `WBP_InventoryItem` — draggable item tile (icon + stack count)
- [ ] **CODE:** `ToggleInventory()` — show/hide widget on **I** key

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
- [Spherical Planet Design](file:///e:/FaldoranPrimeMMO/Documents/Technical/SphericalPlanet_Migration.md) — architecture, phases, real-world constraints
- [Agent Prompt Library](file:///e:/FaldoranPrimeMMO/Documents/Workflows/Agent_Prompt_Library.md) — ready-to-paste agent prompts
- [Database Schema](file:///e:/FaldoranPrimeMMO/Documents/Technical/Database_Schema_v1.sql)
- **G key** = toggle flight, **H key** = cycle speed tier

*Copyright Celtic Trinity Studios, 2026.*
