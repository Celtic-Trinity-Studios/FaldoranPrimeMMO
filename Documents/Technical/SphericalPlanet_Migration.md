# Spherical Planet Migration — Technical Design & Changelog

**Author:** GitHub Copilot (automated)  
**Date:** 2026  
**Status:** Phase 1 — Foundation (coordinate system + constants)

---

## 1. Vision

Replace the current **flat toroidal** (wrapping XY plane) world with a **true spherical planet** using Earth-scale real-world constraints. The system must support:

- Earth-radius sphere (~6,371 km radius, ~40,075 km circumference)
- Full vertical range: -11 km (Mariana Trench) to +8.849 km (Everest)
- Seamless surface traversal (no edges, no seams, no poles artifacts)
- Future orbital/space gameplay (leaving atmosphere, seeing curvature)
- Identical terrain generation from seed on client and server
- MMO-scale: hundreds of concurrent players across the planet

---

## 2. Core Architecture: Tangent-Plane Rendering

### The Problem
UE5 uses 32-bit floats for rendering. At Earth radius (637,100,000 cm), positions lose sub-centimeter precision — meshes jitter, physics break, and collision fails. You **cannot** place meshes at their true spherical position.

### The Solution: Local Tangent Plane (LTP)
Every frame, the game world is a **flat plane tangent to the sphere at the player's current position**. All rendering, physics, and collision happen on this plane. The sphere only exists in **math** — specifically in the coordinate system that maps `(Latitude, Longitude, Altitude)` ? `(LocalX, LocalY, LocalZ)`.

**How it works:**
1. Player has a **global position** stored as `(Latitude, Longitude, Altitude)` — double-precision
2. Chunks are addressed by `(LatCell, LonCell)` — integer grid on the sphere
3. When rendering, each chunk's lat/lon center is converted to **local XY offset** from the player
4. Terrain noise is sampled using `(Latitude, Longitude)` — the sphere IS the noise domain
5. Gravity always points toward sphere center (locally = -Z for ground player)
6. Player wraps seamlessly: longitude wraps at ±180°, latitude capped at ±90°

**Key consequence:** The old `WrapWorldCoord()` toroidal wrap is replaced by spherical coordinate wrapping. Chunks near the poles are narrower (Mercator-like compression handled by adjusting chunk angular width).

### What Changes vs. What Stays

| System | Old (Toroidal) | New (Spherical) | Change Scope |
|--------|---------------|-----------------|-------------|
| Global coordinates | `(WorldX, WorldY)` cm, wrapping | `(Lat, Lon, Alt)` radians, double | **New struct** |
| Chunk addressing | `(Q, R)` square grid, wrapping at 10001 | `(LatCell, LonCell)` on sphere | **Modified** |
| Terrain noise input | `(WorldX, WorldY)` cm | `(Lat, Lon)` scaled to noise space | **Modified** |
| Chunk?Local position | `Q * ChunkSize` | Haversine delta from player lat/lon | **Modified** |
| Player position storage | `FVector` (cm) | `FFPMGeoCoord` (lat/lon/alt) + local `FVector` | **New** |
| Vertical scale | `-11km to +9km` (Z axis) | Altitude above/below sea-level sphere | **Same math** |
| Noise functions | Unchanged | Unchanged (input remapped) | **Minimal** |
| Biome climate | Unchanged | + real latitude?temperature gradient | **Enhanced** |
| Rendering | Flat chunks at world XY | Flat chunks at local tangent XY | **Same PMC code** |
| Collision | Same | Same | **No change** |
| Replication | WorldSeed | WorldSeed | **No change** |

---

## 3. Planet Constants (Earth-Scale)

```
Planet Radius:          6,371 km        = 637,100,000 cm
Circumference:          ~40,075 km      = 4,007,500,000 cm
Surface Area:           ~510 million km²
Sea Level:              Radius exactly (altitude 0)
Max Altitude (Everest): +8,849 m        = +884,900 cm
Max Depth (Mariana):    -10,994 m       = -1,099,400 cm
Chunk Angular Size:     ~0.01152°       ? 1.28 km at equator
Chunks at Equator:      ~31,309
Total Chunks (est):     ~400 million (but only ~200 loaded at once)
```

---

## 4. Coordinate System: `FFPMGeoCoord`

New double-precision geographic coordinate:

```cpp
struct FFPMGeoCoord {
    double Latitude;   // radians, -PI/2 to +PI/2 (south to north)
    double Longitude;  // radians, -PI to +PI (wraps)
    double Altitude;   // cm above sea-level sphere (can be negative)
};
```

**Conversions:**
- `GeoToLocal(PlayerGeo, TargetGeo)` ? `FVector` offset in tangent plane
- `LocalToGeo(PlayerGeo, LocalOffset)` ? `FFPMGeoCoord`
- `GeoToChunkCoord(Geo)` ? `FFPMChunkCoord(LatCell, LonCell)`
- `ChunkCoordToGeo(Coord)` ? `FFPMGeoCoord` (center of chunk)

---

## 5. Chunk Grid on a Sphere

Chunks tile the sphere using an **equirectangular grid** with adaptive longitude count:

- **Latitude bands:** Fixed count, each spanning `ChunkAngularSize` radians
- **Longitude cells per band:** `floor(2? * cos(bandLat) / ChunkAngularSize)`
  - At equator: ~31,309 chunks
  - At 60°N/S: ~15,654 chunks
  - At 89°: ~546 chunks
  - Poles: single cap chunk

This avoids the Mercator "pinch" at poles. Each chunk is roughly 1.28 km × 1.28 km on the ground regardless of latitude.

**Neighbor lookup** accounts for varying longitude counts per band — a chunk at latitude band N may border chunks in band N±1 that have different cell counts.

---

## 6. Terrain Noise on a Sphere

The noise pipeline (`FPMNoise::TerrainHeight`, `Temperature`, `Moisture`) currently takes `(WorldX, WorldY)` in cm. For spherical mapping:

1. Convert `(Lat, Lon)` to a **3D unit sphere point**: `(cos(Lat)*cos(Lon), cos(Lat)*sin(Lon), sin(Lat))`
2. Use this 3D point (scaled) as noise input — this is **seamless on a sphere** with no poles artifacts
3. Domain warping operates on these 3D coordinates
4. All noise wavelengths are specified in **surface distance** (km), converted to angular frequency

This approach guarantees:
- No seams at longitude ±180° wrap
- No singularity at poles
- Identical features when approached from any direction

---

## 7. Climate System Enhancements

Real-Earth climate mapping:
- **Temperature:** Primary driver is now **latitude** (equator = hot, poles = cold), with altitude lapse rate overlay and continental noise for regional variation
- **Moisture:** Driven by ocean proximity (chunks with sub-sea-level terrain nearby are wetter), latitude bands (ITCZ at equator = wet, 30°N/S = dry horse latitudes), and continental noise
- **Prevailing winds:** Simplified Hadley cell model drives moisture transport direction

---

## 8. Migration Phases

### Phase 1: Foundation (THIS COMMIT)
- [x] New `FFPMGeoCoord` struct and conversion functions
- [x] Updated `FPMChunkConstants` with Earth-scale spherical values
- [x] `FFPMChunkCoord` now uses `(LatCell, LonCell)` semantics
- [x] Coordinate wrapping uses spherical math
- [x] Noise input remapped from `(WorldX, WorldY)` to sphere-projected coordinates
- [x] `WorldChunkManager` uses geodetic player position for chunk decisions
- [x] Temperature gets latitude gradient
- [x] Documentation

### Phase 2: Rendering (Future)
- [ ] Subtle mesh vertex displacement for distant-chunk curvature hint
- [ ] Horizon line calculation from altitude
- [ ] Atmospheric scattering based on altitude
- [ ] Skybox shows stars/space when altitude > atmosphere threshold

### Phase 3: Space (Future)
- [ ] Transition from surface mode to orbital mode at ~100 km altitude
- [ ] Orbital camera: sees full planet sphere mesh (LOD shell at planetary scale)
- [ ] Re-entry: transition back to tangent-plane surface mode
- [ ] Multiple celestial bodies (moons, other planets)

---

## 9. Files Modified in Phase 1

| File | Change Summary |
|------|---------------|
| `FPMChunkData.h` | New `FFPMGeoCoord`, updated `FPMChunkConstants` to Earth-scale sphere, new conversion helpers |
| `FPMChunkData.cpp` | Sphere-aware coordinate conversions, geodetic chunk addressing |
| `FPMNoise.h` | New `TerrainHeight3D()` that takes sphere-projected coords |
| `FPMNoise.cpp` | Noise sampling via 3D sphere projection; latitude-based temperature |
| `FPMVoxelChunk.cpp` | `TerrainSurfaceZ` uses geodetic coords |
| `FPMWorldChunkManager.cpp` | Player position tracked as geo coord; chunk gather uses sphere grid |
| `FPMPlanetTraversal.cpp` | Distance tracking uses great-circle distance |
| `FPMPlayerCharacter.cpp` | Fall-recovery uses geodetic surface lookup |
| `WorldGen.ini` | Updated comments for spherical planet |
| `Documents/Technical/SphericalPlanet_Migration.md` | This document |

---

## 10. Backward Compatibility

- **Save files:** Existing `spawn_x/spawn_y/spawn_z` in the database are treated as flat-world cm coordinates. A migration function converts them to `(Lat, Lon, Alt)` on first load. New saves store geodetic coordinates.
- **WorldSeed:** Same seed produces same terrain (noise is deterministic). However, the *spatial distribution* changes because noise is now sampled on a sphere — existing explored areas will look different.
- **Client/Server:** Both must run the same version. No mixed-version support during migration.

---

## 11. Real-World Constraints Reference

| Feature | Earth Value | Game Value | Notes |
|---------|------------|------------|-------|
| Radius | 6,371 km | 6,371 km | 1:1 scale |
| Circumference | 40,075 km | 40,075 km | ~13.4 hours at Mach 1 |
| Surface area | 510M km² | 510M km² | ~3.6x size of all MMOs combined |
| Highest point | 8,849 m | 8,849 m | Everest equivalent |
| Deepest point | -10,994 m | -10,994 m | Mariana equivalent |
| Ocean coverage | ~71% | ~65-75% (seed-dependent) | Continental noise controls this |
| Chunk size | — | ~1.28 km | Same as before |
| Loaded chunks | — | ~200-400 | Same LOD rings |
| Atmosphere top | ~100 km | ~100 km | Future: space transition |

---

*Copyright Celtic Trinity Studios, 2026.*
