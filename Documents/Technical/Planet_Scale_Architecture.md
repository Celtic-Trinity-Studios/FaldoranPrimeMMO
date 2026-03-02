# Planet-Scale Architecture

> **Status:** Future reference — do NOT implement until flat island system and core gameplay are stable.  
> **Prerequisite Milestones:** v0.5 Alpha (working flat chunks, origin rebasing, core gameplay)

---

## When to Implement

| Version | Terrain System |
|---------|---------------|
| v0.1 Now | Flat square chunks, voxel Marching Cubes, single island |
| v0.5 Alpha | Flat chunks + origin rebasing, async mesh gen, 50km landmass |
| v1.0 Beta | Multiple continents, World Partition for content streaming |
| v2.0 Planet | Cube-sphere quadtree, full planet surface |

---

## Architecture Summary

1. **GeoReferencing (Round Planet) + LWC** — lat/long/alt working coordinates
2. **Floating Origin Rebasing** at v0.5 (required for any world > 20km)
   - Player drifts >10km → rebase all actors, reset player near (0,0,0)
   - `UWorld::SetNewWorldOrigin`; custom systems need manual shift handling
3. **Cube-Sphere Quadtree** — 6 cube faces, each subdivided by distance-based quadtree
   - Leaf node = 33×33 vert mesh patch with skirts (hide LOD cracks)
   - Current `FFPMChunkCoord(Q,R)` maps to one cube face; migration adds `FaceIndex`
4. **Multi-Resolution Generation**
   - Global (tectonic/climate) → Regional (ridges/erosion) → Local (biome detail) → Micro (GPU shader)
   - All pure functions; deterministic by `(seed, tileID, LOD)`
5. **World Partition** — for POIs/settlements/actors ON terrain, not for the terrain mesh itself
6. **PCG** — biome-driven scattering around player; current `FPMBiomePCGSpawner` ports directly
7. **Nanite** — pre-authored meshes (rocks, structures); terrain patches use classic vertex buffers

## Current System Compatibility
- Vertex-color biome splatting (`M_ChunkTerrain`) transfers directly to planet-scale
- `FPMBiomePCGSpawner` HISM system compatible; needs density tuning and billboard LOD
- `FPMVoxelGenerator::GenerateAndMesh` is mostly pure — main changes: spherical coords + regional cache layer

## Rendering Split
| System | Approach |
|--------|---------|
| Terrain patches | Classic vertex/index, custom LOD |
| Rocks, cliffs, structures | Nanite |
| Trees, props | Nanite or HISM |
| Grass, ground cover | GPU instancing |

---

## Roadmap Checklist

### Phase 1 — Stabilize (Now → v0.3)
- [x] Square chunk grid + voxel collision
- [x] Biome texturing + PCG spawning
- [ ] Async mesh generation (background threads)
- [ ] Origin rebasing (>10km world)

### Phase 2 — Scale Up (v0.3 → v0.5)
- [ ] 50km island diameter
- [ ] Disk caching for generated chunks
- [ ] Distance fog / atmospheric scattering
- [ ] Basic LOD mesh simplification

### Phase 3 — Multi-Region (v0.5 → v1.0)
- [ ] Multiple islands / continents
- [ ] World Partition for content streaming
- [ ] Ocean system + regional terrain layer

### Phase 4 — Planet Scale (v1.0 → v2.0)
- [ ] Cube-sphere quadtree conversion
- [ ] GeoReferencing Round Planet mode
- [ ] Planetary atmosphere + spherical ocean shell

---

## Reference
- [GeoReferencing Plugin](https://dev.epicgames.com/documentation/en-us/unreal-engine/georeferencing-plugin-for-unreal-engine)
- [Large World Coordinates](https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-unreal-engine-5)
- [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
