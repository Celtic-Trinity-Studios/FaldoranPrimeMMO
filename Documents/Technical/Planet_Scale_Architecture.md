# Planet-Scale Terrain Architecture for UE 5.7+

> **Status**: Future reference — do NOT implement until the flat island system
> is stable and core gameplay is built.
>
> **Last Updated**: 2026-02-20
>
> **Prerequisites**: Working flat-chunk terrain, origin rebasing, gameplay systems

---

## Overview

This document outlines the architecture needed to scale FaldoranPrimeMMO from a
single starter island (~5km diameter) to full planet-scale terrain. This is a
**v2.0+ roadmap**, not a v1.0 priority.

### When to Implement This

| Milestone | Terrain System |
|-----------|---------------|
| **v0.1 — Now** | Flat square chunks, single island, voxel Marching Cubes |
| **v0.5 — Alpha** | Flat chunks + origin rebasing, async mesh gen, 50km landmass |
| **v1.0 — Beta** | Multiple continents, World Partition for content streaming |
| **v2.0 — Planet** | Cube-sphere quadtree, full planet surface |

---

## 1. Coordinate System: LWC + GeoReferencing

**Use Large World Coordinates (LWC)** for engine-side precision.

Use UE's **GeoReferencing plugin** in **Round Planet mode** so you can work in
lat/long/alt and keep sane "up" orientation over a globe. UE ships helpers like
`AGeoReferencingSystem` and `ARoundPlanetPawn` for this.

**Why this matters**: Earth scale immediately becomes a precision + orientation
problem before it becomes a terrain problem. Double-precision coordinates are
mandatory once you exceed ~10km from origin.

### Migration Path from Current System

The current `FFPMChunkCoord(Q, R)` flat grid maps cleanly to a single face of
a cube-sphere. When migrating:

1. Treat the current island as one face of a 6-face cube
2. Add a `FaceIndex` to the chunk coordinate
3. Replace `ChunkToWorldOrigin` with sphere projection math

---

## 2. Floating Origin / Rebasing

Even with LWC, you still want a **floating origin / rebasing strategy** to keep
local simulation stable (especially physics and tight visuals).

### Practical Approach

1. Maintain a high-precision "geospatial" position (lat/long/alt or ECEF) for
   the player/camera.
2. Maintain a local UE world centered near the player.
3. When the player drifts too far (>10km), **rebase the local world** — move
   all tiles/actors and keep the player near (0,0,0).

### Implementation Notes

- UE supports `AWorldSettings::bEnableWorldBoundsChecks` and manual origin
  shifting via `UWorld::SetNewWorldOrigin`.
- All actors with `bShouldUpdatePhysicsVolume` will be shifted automatically.
- Custom systems (chunk managers, water planes) need manual shift handling.

### Migration Path

This should be implemented at **v0.5 (Alpha)** regardless of whether you go
planet-scale. It's needed for any world larger than ~20km.

---

## 3. Cube-Sphere Quadtree Terrain

For a planet, the standard approach is:

1. Represent the planet as a **cube-sphere** (6 faces)
2. Each face subdivided by a **quadtree** LOD based on camera distance
3. Each leaf node is a small grid mesh patch (33×33 or 65×65 verts) with
   **skirts** to hide cracks between LODs

### Terrain Patch Pipeline

For each visible patch:

1. Generate vertices on the cube face
2. Project to sphere (normalize + scale by radius)
3. Displace by `height = PlanetRadius + Height(lat, long)`
4. Stream/cache the patch's derived data (height, normals, biome masks)

### Comparison to Current System

| Current (v0.1) | Planet (v2.0) |
|----------------|---------------|
| Flat 2D grid of chunks | 6-face cube-sphere quadtree |
| Fixed chunk size (3200cm) | Variable patch size (LOD-dependent) |
| Marching Cubes voxels | Heightfield + displacement |
| Uniform LOD rings | Distance-based quadtree splits |
| `FFPMChunkCoord(Q, R)` | `FPlanetPatchID(Face, QTPath)` |

### Key Design Decisions

- **Patch size**: 33×33 verts is common (32×32 quads per patch)
- **Skirts**: 2-4 verts hanging down at patch edges to hide LOD cracks
- **Max LOD levels**: ~20 levels covers Earth-scale (face = 10,000km) down to
  ~1cm per vertex
- **Cache strategy**: Serialize generated patches to disk, keyed by
  `(seed, face, qtpath, lod)`

---

## 4. World Partition — For Content, Not Terrain

**World Partition** is excellent for actors/content streaming but should NOT be
used for the planet surface mesh itself.

### Why Not for Terrain

- World Partition wants a flat grid over a level; a globe needs a 
  sphere-friendly spatial index (cube-sphere / HTM)
- You want deterministic regeneration from seed, not authoring millions of cells
- Custom LOD is more efficient than WP's distance-based streaming

### Use World Partition For

- Settlements, caves, POIs spawned into cells near the player
- Gameplay actors attached to terrain tiles
- HLOD for authored set pieces
- UE 5.7 Custom HLODs for large streaming setups

---

## 5. Multi-Resolution Terrain Generation

For Earth-scale, you don't generate everything at max detail. Use multi-res:

### Resolution Layers

| Layer | Scale | Generated When | Cached |
|-------|-------|---------------|--------|
| **Global** | Tectonic plates, continents, climate bands, ocean masks | Once per seed | Permanent |
| **Regional** | Mountain ridges, drainage, erosion approximation | Per region on demand | Disk |
| **Local** | Fine noise, biome detail, caves | Per patch, around player | Memory |
| **Micro** | Detail normals, material blend | Material shader, per-pixel | GPU only |

### Key Rules

1. **Deterministic**: `(seed, tileID, LOD)` → same outputs always
2. **Cache aggressively**: Disk + memory LRU
3. **Async generation**: Split into jobs so streaming doesn't hitch
4. **Pure functions**: No side effects, thread-safe by design

### Migration Path

The current `FPMVoxelGenerator::GenerateAndMesh` is already mostly pure and
deterministic. The main changes for planet-scale:

1. Replace flat noise coordinates with spherical coordinates
2. Add a regional cache layer between global seed and local generation
3. Move generation to `FRunnable` background threads (planned for v0.5)

---

## 6. Materials & Micro-Detail

UE has **Runtime Virtual Textures (RVT)** and **Virtual Heightfield Mesh (VHM)**
for displacement-style workflows, but VHM is primarily a flat/heightfield
paradigm.

### For a Spherical Planet

- Use **patch meshes** for true geometry silhouette
- Use **materials** for micro-detail:
  - Triplanar mapping (avoids UV distortion on steep surfaces)
  - Detail normals blended by distance
  - Macro/micro color blending
  - Biome-driven material splatting (vertex colors, like current system)
- Optionally use **RVT-like concepts** for:
  - Local decals and footprints
  - Blending between authored and procedural terrain

### Current System Compatibility

The current vertex-color biome splatting material (`M_ChunkTerrain`) transfers
directly. The main addition for planet-scale would be triplanar mapping for
cliff faces and distance-based detail reduction.

---

## 7. Foliage & Scattering: PCG

Once terrain tiles exist, use PCG to populate:

- Trees, rocks, ground cover driven by biome masks, slope, height, moisture
- Runtime spawning around the player, despawn outside radius
- UE 5.7's procedural tooling and vegetation workflows are evolving quickly

### Current System Compatibility

The current `FPMBiomePCGSpawner` and `PopulateBiome` HISM-based system works
well. For planet-scale, the main changes:

1. Increase scatter density for richer visuals
2. Add more biome-specific meshes (flowers, grass, bushes, etc.)
3. Add distance-based billboard/impostor LOD for far vegetation
4. Consider GPU-driven instancing for millions of instances

---

## 8. Rendering: Nanite Split

**Nanite** is fantastic for streaming high-detail static meshes, but
runtime-generated terrain meshes typically can't use Nanite (runtime Nanite
generation isn't supported in shipping builds).

### Practical Split

| System | Rendering Approach |
|--------|--------------------|
| Terrain patch meshes | Classic vertex/index buffers, custom LOD |
| Rocks, cliffs, structures | Nanite (pre-authored meshes) |
| Trees, props | Nanite or HISM depending on count |
| Grass, ground cover | GPU instancing or Nanite foliage |

---

## 9. Atmosphere & Ocean: Separate Systems

### Atmosphere

- `SkyAtmosphere` component tuned for ground-to-space transitions
- Volumetric clouds with planetary curvature
- Day/night cycle with proper scattering

### Ocean

- Separate sphere/ocean shell at sea level radius
- Local wave detail near camera (FFT ocean or Gerstner waves)
- Don't simulate waves globally — only near the camera
- Current water plane system scales directly to a spherical shell

---

## Recommended Architecture Summary

For the most robust, least-regret design in UE 5.7+:

1. ✅ **GeoReferencing (Round Planet) + LWC** for coordinates/orientation
2. ✅ **Floating origin** to keep local simulation tight
3. ✅ **Cube-sphere quadtree** terrain tiles around camera
4. ✅ **Deterministic multi-res generation** with disk caching
5. ✅ **World Partition + HLOD** for content ON the planet, not the surface
6. ✅ **PCG** for biome-driven scattering near the player
7. ✅ **Nanite** for authored meshes, classic rendering for terrain patches

---

## Migration Roadmap from Current System

### Phase 1: Stabilize (Now → v0.3)
- [x] Square chunk grid working
- [x] Voxel terrain with collision
- [x] Biome texturing and tree spawning
- [ ] Fix remaining chunk loading/spawn issues
- [ ] Implement async mesh generation (background threads)
- [ ] Add origin rebasing for >10km worlds

### Phase 2: Scale Up (v0.3 → v0.5)
- [ ] Expand island to 50km diameter
- [ ] Add more biomes and terrain features
- [ ] Implement disk caching for generated chunks
- [ ] Add distance fog / atmospheric scattering to hide loading edge
- [ ] Implement basic LOD mesh simplification

### Phase 3: Multi-Region (v0.5 → v1.0)
- [ ] Add multiple islands / continents
- [ ] Implement World Partition for content streaming
- [ ] Add ocean system between landmasses
- [ ] Implement regional terrain generation layer

### Phase 4: Planet Scale (v1.0 → v2.0)
- [ ] Convert to cube-sphere quadtree
- [ ] Add GeoReferencing in Round Planet mode
- [ ] Implement planetary atmosphere
- [ ] Convert ocean to spherical shell
- [ ] Add space-to-ground transition

---

## Reference Links

- [UE5 GeoReferencing Plugin Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/georeferencing-plugin-for-unreal-engine)
- [UE5 Large World Coordinates](https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-unreal-engine-5)
- [UE5 World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
- [Cube-Sphere Terrain (GPU Gems)](https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows)
