# Flowing Water System — Design Document

**Status:** 🟡 In Progress — Phase 2A (Data + Simulation + Mesh) implemented
**Inspiration:** Enshrouded's flowing water — spawns from source points and flows downhill like real water
**Dependencies:** Voxel terrain system (✅ Complete), Terraforming system (planned)

### Implementation Status
| Phase | Status | Files |
|-------|--------|-------|
| 2A: Water Data Layer | ✅ Done | `FPMWaterChunkData.h` |
| 2B: Water Source Actor | ✅ Done | `FPMWaterSource.h/cpp` |
| 2C: Pipe Model Simulation | ✅ Done | `FPMWaterSimulation.h/cpp` |
| 2D: Water Mesh Builder | ✅ Done | `FPMWaterMeshBuilder.h/cpp` |
| 2E: WorldChunkManager Integration | ✅ Done | `FPMWorldChunkManager.h/cpp` |
| 2F: INI Configuration | ✅ Done | `Config/WorldGen.ini` |
| 3A: Water Material (UE5) | ⬜ Pending | *Needs Blueprint/Material editor* |
| 3B: Terraforming Integration | ⬜ Pending | *Requires terraforming system* |
| 3C: Multiplayer Replication | ⬜ Pending | *Water state sync* |

### Anti-Flood Mechanisms (all implemented)
1. **Evaporation** — `EvaporationRate` in INI (primary control, default 0.001)
2. **Ocean Drain** — Coast/Ocean biomes absorb all water instantly
3. **Flow Range Limit** — Water beyond `MaxFlowHops` decays rapidly
4. **Max Water Depth Cap** — `MaxWaterDepth` prevents unrealistic pooling
5. **Volume Conservation** — Can't outflow more water than available per cell

---

## 1. Overview

The flowing water system replaces the current flat water plane with a physically-motivated water simulation. Water spawns from **source actors** (mountain springs, cave exits, rainfall zones) and flows downhill through the voxel grid, pooling in depressions to form **rivers** and **lakes** naturally.

### Design Goals
- Water flows from source points following gravity and terrain contours
- Rivers form organically based on terrain shape (not pre-carved)
- Lakes form in natural depressions where water pools
- Terraforming affects water flow in real-time (dig a channel → water redirects)
- Visually convincing with surface rendering, foam, and transparency
- Performant enough for MMO with many players in the same area

### Non-Goals (First Pass)
- Full fluid dynamics (Navier-Stokes) — too expensive for MMO
- Erosion simulation — can be added later
- Underground water / aquifers — future feature
- Wave simulation on large bodies of water — separate ocean system

---

## 2. Architecture

### 2.1 Water Source Actors (`AFPMWaterSource`)

Placed in the world (by generation or manually) to emit water:

```
UCLASS()
class AFPMWaterSource : public AActor {
  UPROPERTY() float FlowRate = 1.0f;      // m³/sec of water output
  UPROPERTY() float Temperature = 15.0f;   // For future biome effects
  UPROPERTY() EFPMWaterSourceType Type;     // Spring, Rainfall, Cave, Artesian
  UPROPERTY() bool bInfinite = true;        // Infinite source vs depletable
};
```

**Procedural Placement:** During world generation, water sources are placed:
- At high-elevation points in Mountain/Snow biomes (springs)
- At biome transition zones (groundwater seepage)
- Configurable via `WorldGen.ini` `[WaterSources]` section

### 2.2 Water Volume Grid

Each voxel chunk stores an additional **water level** alongside terrain density:

```
// Per-chunk water data
struct FFPMChunkWaterData {
  TArray<float> WaterLevel;    // Water height at each XY column (2D grid)
  TArray<FVector2D> FlowDir;   // Flow direction per cell (for rendering)
  TArray<float> FlowSpeed;     // Flow velocity magnitude per cell
  float LastSimTime;            // Timestamp of last simulation tick
};
```

**Why 2D columns instead of 3D voxels for water?**
Full 3D water voxels are expensive. For surface water (rivers/lakes), a 2D heightfield per chunk column is sufficient. Each cell stores the water surface height at that XY position. Water flows between adjacent cells based on height differences.

### 2.3 Flow Simulation (Cellular Automaton)

The water simulation uses a **pipe model** — a simplified hydraulic simulation that's fast enough for real-time:

```
For each cell (X, Y):
  TerrainZ = terrain surface height at (X, Y)
  WaterZ = TerrainZ + WaterLevel[X][Y]

  For each neighbor N in {Left, Right, Front, Back}:
    NeighborTerrainZ = terrain surface height at N
    NeighborWaterZ = NeighborTerrainZ + WaterLevel[N]

    // Height difference drives flow
    DeltaH = WaterZ - NeighborWaterZ

    // Update flow pipe
    FlowPipe[X][Y][N] += DeltaH * Gravity * DeltaT
    FlowPipe[X][Y][N] = Max(0, FlowPipe[X][Y][N])  // No negative flow

  // Volume conservation: update water levels
  TotalOutflow = Sum of all outgoing pipes
  If TotalOutflow > WaterLevel[X][Y]:
    Scale all pipes proportionally (can't flow more than you have)

  WaterLevel[X][Y] -= TotalOutflow * DeltaT
  For each neighbor N:
    WaterLevel[N] += FlowPipe[X][Y][N] * DeltaT
```

**Simulation Rate:** 10-20 Hz (not every frame). Water spreads gradually.
**Chunk Boundaries:** Flow across chunk edges requires inter-chunk communication via the WorldChunkManager.

### 2.4 Water Surface Mesh

A separate rendering pass generates water surface meshes:

```
For each chunk with water:
  For each cell (X, Y) where WaterLevel > MinRenderThreshold:
    Generate a quad at height TerrainZ + WaterLevel
    Set vertex color based on depth (deeper = more opaque)
    Set UV flow direction from FlowDir for animated water texture
```

This produces a separate `ProceduralMeshComponent` per chunk for water, rendered with a translucent water material.

### 2.5 Water Material

```
Material Features:
- Translucent with depth-based opacity (shallow = clear, deep = opaque)
- Animated UV scrolling based on flow direction vertex attribute
- Fresnel effect for realistic water surface
- Foam at shorelines (depth < threshold)
- Refraction for underwater visibility
- Color tint based on biome (clear mountain water vs murky swamp)
```

---

## 3. Integration Points

### 3.1 Terraforming Interaction
When a player terraforms (adds/removes voxels):
1. Terrain density changes
2. Water simulation re-evaluates affected cells
3. If a dam is broken → water flows through the new gap
4. If a channel is dug → water follows the new path
5. If terrain is raised → water pools behind the new obstacle

### 3.2 Gameplay Effects
- Swimming: Player enters swim mode when water depth > capsule height
- Drowning: Stamina drain while swimming, damage when stamina = 0
- Fishing: Requires water cells of sufficient depth
- Agriculture: Crops near water grow faster
- Combat: Some spells interact with water (freeze creates ice bridge, lightning chains through water)

### 3.3 INI Configuration (`Config/WorldGen.ini`)

```ini
[WaterSources]
; Number of water sources per mountain biome chunk
SourcesPerMountainChunk=2
; Base flow rate (m³/sec) for springs
SpringFlowRate=0.5
; Minimum elevation for natural springs (normalized height)
MinSpringElevation=0.25

[WaterSimulation]
; Simulation tick rate (Hz)
SimulationRate=15
; Gravity acceleration for flow
FlowGravity=9.8
; Minimum water level to render (cm)
MinRenderDepth=2.0
; Evaporation rate (water lost per second per cell)
EvaporationRate=0.001
; Maximum water depth per cell (cm)
MaxWaterDepth=500.0
```

---

## 4. Implementation Phases

### Phase 1: Static Water Fill (Current ✅)
- Flat water plane at configurable Z height
- Rivers pre-carved in terrain via noise
- Lakes from terrain depressions

### Phase 2: Water Source Actors + Basic Flow
- Create `AFPMWaterSource` actor
- Implement 2D column water heightfield per chunk
- Basic pipe-model flow simulation (single chunk)
- Simple flat water surface mesh per chunk
- Water source placement during world generation

### Phase 3: Cross-Chunk Flow + Rendering
- Inter-chunk water flow communication
- Translucent water material with depth-based opacity
- Flow direction for UV animation
- Foam at shorelines

### Phase 4: Terraforming Integration
- Water re-simulation on voxel changes
- Dam building / channel digging affects flow
- Performance optimization (only re-sim affected chunks)

### Phase 5: Gameplay Integration
- Swimming mechanics
- Water-based gameplay (fishing, agriculture, combat spells)
- Sound effects (flowing water, babbling brook, waterfall)
- Particle effects (splashes, mist near waterfalls)

---

## 5. Performance Considerations

| Concern | Mitigation |
|---------|------------|
| Sim cost per chunk | Only simulate chunks with water sources or active flow |
| Cross-chunk sync | Batch edge-cell updates each sim tick via WorldChunkManager |
| Mesh generation | Only regenerate water mesh when water level changes > threshold |
| Memory per chunk | 2D grid (32×32 = 1024 floats = 4KB per chunk) — negligible |
| Distant chunks | Skip simulation for chunks far from any player |
| Server authority | Server runs simulation, replicates water levels to clients |

---

## 6. References

- **Enshrouded water system** — Flowing water from spawn points, pools in terrain depressions
- **Pipe model paper:** "Fast Hydraulic Erosion Simulation and Visualization on GPU" (Mei, Decaudin, Hu, 2007)
- **Minecraft water** — Simplified cellular automaton, source blocks, flow range limit
- **Valheim water** — Static water level with wave rendering, no flow simulation

---

## 7. File Structure (Planned)

```
Source/FaldoranPrimeMMO/
  Public/World/
    FPMWaterSource.h          — Water source actor
    FPMWaterSimulation.h      — Flow simulation manager
    FPMWaterChunkData.h       — Per-chunk water data structures
  Private/World/
    FPMWaterSource.cpp         — Source actor implementation
    FPMWaterSimulation.cpp     — Pipe-model flow simulation
    FPMWaterMeshBuilder.cpp    — Water surface mesh generation
    FPMWaterMaterial.cpp       — Dynamic water material setup
```
