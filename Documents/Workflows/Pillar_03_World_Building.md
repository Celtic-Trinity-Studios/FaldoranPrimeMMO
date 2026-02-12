# Pillar 03 — Building the World

**Goal:** Replace `L_PrototypeWorld` (flat empty map) with an actual explorable environment that demonstrates the game's biome and terrain system.

**Status:** Planned

This is the biggest pillar and the most creative. Based on [02_World_and_Scale.md](file:///e:/FaldoranPrimeMMO/Documents/Design/02_World_and_Scale.md) and [Technical_Infrastructure.md](file:///e:/FaldoranPrimeMMO/Documents/Technical/Technical_Infrastructure.md), the full vision is an Earth-sized procedurally generated world with hexagonal chunk sharding. **That's production.** For this phase, we need a **playable slice** that proves the world systems work.

## Strategy: The "Starter Island" Approach

Build a single, contained region (~2–5 km²) that showcases:
- Multiple biomes: **Meadows** (starter/spawn), Forest, Mountain, Coast
- Terrain variation (hills, valleys, water)
- Resource nodes (visual cues from [11_Resource_Surveying_and_Geology.md](file:///e:/FaldoranPrimeMMO/Documents/Design/11_Resource_Surveying_and_Geology.md))
- Day/night cycle
- Basic weather
- Spawn points (Meadows biome only)
- At least one POI (a ruin, a mine entrance, or a settlement footprint)

> [!IMPORTANT]
> **Spawn Rule:** Players can **ONLY** spawn in biomes tagged as **Meadows**. The exact spawn location within Meadows zones is **randomized**. This means the Starter Island must have at least one Meadows biome with multiple valid spawn points scattered across it.

---

## Phase 3A: Terrain & Landscape

- Create a new map `L_StarterIsland` using UE5 Landscape tools
- Design heightmap with varied elevation (mountains, valleys, flatlands, coast)
- Set up landscape materials with auto-painting (slope-based: grass → rock → snow)
- Water body (ocean around island, at least one river or lake)
- Multiple biome zones using landscape layers

---

## Phase 3B: Environment & Atmosphere

- Directional light with day/night cycle (rotating sun)
- Sky atmosphere + volumetric clouds
- Post-process volume for mood (warm day, cool night)
- Fog for distance and atmosphere
- Basic weather system (rain particle effect, wind audio)

---

## Phase 3C: Foliage & Natural Objects

- Trees (multiple types per biome: deciduous, coniferous, tropical)
- Grass and ground cover (procedural foliage spawning)
- Rocks and boulders (variety pack)
- Flowers and bushes for biome identity

---

## Phase 3D: Resource Nodes (Visual Prototypes)

- Place static mesh actors representing mineable rocks (iron-tinted, copper-patina, etc.)
- These are **visual only** for now — no interaction system yet
- Follow the "tell" system from [11_Resource_Surveying_and_Geology.md](file:///e:/FaldoranPrimeMMO/Documents/Design/11_Resource_Surveying_and_Geology.md)
- At minimum: iron, copper, coal, and wood (trees) as distinct visual types

---

## Phase 3E: Points of Interest

- At least one handcrafted POI:
  - An ancient ruin (static meshes arranged architecturally)
  - OR a mine entrance (demonstrates future dungeon system)
  - OR a settlement footprint (flat area with placeholder market stalls)
- This proves the "Handcrafted Layer" concept from `Technical_Infrastructure.md`

---

## Phase 3F: Player Spawn & Navigation

- Create a `Meadows` biome zone with multiple `PlayerStart` actors scattered throughout
- Spawn logic: server picks a random `PlayerStart` that is within a Meadows-tagged volume
- Other biomes (Forest, Mountain, Coast) have **no** spawn points — players must walk to them
- Basic nav mesh for future NPC pathfinding
- Collision on all terrain and objects
- Kill volume under the world (fall-off protection)

---

## Phase 3G: World Chunk Foundation (Optional Advanced)

- If time allows, set up the hexagonal chunk concept using World Partition or Level Streaming
- This is forward-looking prep for the procedural generation system
- Not required for the playable slice but sets up the architecture

> [!TIP]
> **Asset Strategy:** UE5 Marketplace has free packs (Quixel Megascans via Fab) that provide photorealistic rocks, trees, and foliage. Using these for the prototype world saves weeks of art time. The custom art pipeline can replace them later.

---

## Agent Prompts — Pillar 3

### Phase 3A --- 06. Terrain and Landscape
```
CONVERSATION TITLE: Pillar 03, Phase 3A --- 06. Terrain and Landscape

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Design/02_World_and_Scale.md for world design.
Read the file Documents/Workflows/Pillar_03_World_Building.md for the current plan.

TASK: Walk me through creating the Starter Island terrain in UE5.

This is primarily an editor task. I need step-by-step instructions to:
1. Create a new map L_StarterIsland in Content/Maps/
2. Create a Landscape (~2-5 km²) using UE5 Landscape tools
   - Heightmap with varied elevation: mountains in the center/north, valleys, flatlands (Meadows), coastal edges
   - At least 4 distinct biome zones: Meadows, Forest, Mountain, Coast
3. Set up landscape material with auto-painting:
   - Grass layer for Meadows (flat, gentle terrain)
   - Forest floor layer (under tree canopy)
   - Rock layer (steep slopes, mountains)
   - Sand/dirt layer (coast, beaches)
   - Auto slope-based blending
4. Add water:
   - Ocean around the island using UE5 Water system
   - At least one river or lake inland
5. Mark biome boundaries (volume actors or tags) so the spawn system knows which zones are Meadows

Provide exact UE Editor steps: which menus, which settings, which values.
The Meadows biome should be the largest flat area — this is where players spawn.
```

### Phase 3B --- 07. Atmosphere and Lighting
```
CONVERSATION TITLE: Pillar 03, Phase 3B --- 07. Atmosphere and Lighting

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Pillar_03_World_Building.md for the current plan.

TASK: Set up the day/night cycle, sky, and weather for L_StarterIsland.

Prerequisites: Phase 3A complete. L_StarterIsland terrain exists.

Walk me through (editor steps + any C++ needed):
1. Day/Night cycle:
   - Directional light that rotates over time (sun)
   - Blueprint or C++ actor that controls rotation speed (1 in-game day = configurable real minutes)
   - Moon light for nighttime
2. Sky and atmosphere:
   - SkyAtmosphere component
   - Volumetric clouds
   - Sky colors change with sun angle (dawn, noon, dusk, night)
3. Post-processing:
   - PostProcessVolume for the map
   - Warm tones during day, cool blue tones at night
   - Auto-exposure for indoor/outdoor transitions
4. Fog:
   - ExponentialHeightFog for atmosphere and distance haze
   - Denser fog near water/coast
5. Basic weather:
   - Rain particle system (Niagara) that can be toggled
   - Wind audio (ambient sound)
   - Weather does NOT need gameplay effects yet — just visual/audio

Provide exact steps. For any C++ classes, follow all rules in 00_Rules_and_Constraints.md.
```

### Phase 3C --- 08. Foliage and Environment
```
CONVERSATION TITLE: Pillar 03, Phase 3C --- 08. Foliage and Environment

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Pillar_03_World_Building.md for the current plan.

TASK: Populate L_StarterIsland with foliage, trees, rocks, and natural objects per biome.

Prerequisites: Phases 3A-3B complete. Terrain and atmosphere exist.

Walk me through:
1. Foliage setup:
   - Set up UE5 Procedural Foliage system
   - Biome-appropriate foliage types:
     * Meadows: grass, wildflowers, scattered bushes
     * Forest: dense trees (deciduous + coniferous mix), undergrowth, ferns
     * Mountain: sparse shrubs, alpine grass, exposed rock
     * Coast: beach grass, driftwood, palm-like trees
2. Tree placement:
   - Use foliage painting tool or procedural foliage spawner
   - Multiple tree meshes per biome for variety
   - Proper LOD settings for performance
3. Rocks and boulders:
   - Scatter placement in Mountain and Forest zones
   - Variety of sizes
4. Performance:
   - Foliage culling distances
   - HLOD setup if needed
   - Target: stable 60fps with foliage visible

Use free assets from Fab/Quixel Megascans where possible. Provide exact asset names or download instructions.
```

### Phase 3DE --- 09. Resources and POIs
```
CONVERSATION TITLE: Pillar 03, Phase 3DE --- 09. Resources and POIs

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Design/11_Resource_Surveying_and_Geology.md for resource visual cues.
Read the file Documents/Workflows/Pillar_03_World_Building.md for the current plan.

TASK: Place visual resource nodes and at least one POI on L_StarterIsland.

Prerequisites: Phases 3A-3C complete. Terrain, atmosphere, and foliage exist.

Do this in micro-steps:
1. Create visual resource node actors (visual only, no interaction yet):
   - Iron nodes: red-tinted rock meshes, placed in Mountain biome
   - Copper nodes: greenish-patina rock meshes, placed near Forest/Mountain border
   - Coal nodes: dark crumbling shale, placed near rivers/ravines
   - Wood: trees already placed in Forest (no special actor needed)
   - Follow the "tell" system from 11_Resource_Surveying_and_Geology.md
2. Create at least one POI:
   - Option A: Ancient ruin (static meshes arranged as crumbling walls, columns, an altar)
   - Option B: Mine entrance (cave opening in mountainside)
   - Option C: Settlement footprint (flat cleared area with placeholder market stalls)
   - Choose whichever is most achievable with available assets
3. Place the POI on the map in a discoverable but not obvious location
4. Add basic collision to all placed objects

For C++ resource actors: follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines.
These are VISUAL ONLY — no interaction, no inventory, no gathering. Just proof of the world concept.
```

### Phase 3F --- 10. Spawn System and Polish
```
CONVERSATION TITLE: Pillar 03, Phase 3F --- 10. Spawn System and Polish

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Pillar_03_World_Building.md for the current plan.

TASK: Implement Meadows-only randomized spawning and final world polish.

Prerequisites: Phases 3A-3E complete. Full Starter Island exists with biomes, atmosphere, foliage, resources, and POIs.

IMPORTANT SPAWN RULE: Players can ONLY spawn in Meadows biome zones. Spawn location is RANDOMIZED among valid Meadows spawn points.

Do this in micro-steps, one at a time, each must compile:
1. Create biome volume actors:
   - AFPMBiomeVolume (inherits AVolume) with a BiomeType enum (Meadows, Forest, Mountain, Coast)
   - Place volumes over each biome zone in L_StarterIsland
2. Place multiple PlayerStart actors within the Meadows volume(s)
   - Scatter them across the Meadows area (at least 8-10 spawn points)
   - Ensure they are on solid ground, not overlapping objects
3. Update AFPMGameMode:
   - Override ChoosePlayerStart() or FindPlayerStart()
   - Filter PlayerStarts to only those inside a Meadows-tagged biome volume
   - Randomly select from valid Meadows spawns
4. Add kill volume under the world (fall-off protection)
5. Add basic nav mesh for future NPC pathfinding
6. Update DefaultEngine.ini to set L_StarterIsland as the new default map (replacing L_PrototypeWorld)
7. Compile and test:
   - Launch PIE with 2+ players
   - Verify all players spawn in Meadows area (different random locations)
   - Verify players can walk to Forest, Mountain, and Coast biomes
   - Verify players can see each other
   - THIS IS THE PILLAR 3 MILESTONE 🎉

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Server authority — server picks spawn point.
```
