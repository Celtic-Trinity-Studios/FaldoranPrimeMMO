# Technical Infrastructure

---

## 1. Generation — Deterministic Seeded Procedural
- **Shared Seed:** Client and server generate identical terrain from same seed
- **Active Chunks:** Only 10–20km radius around each player loaded and simulated
- **Layer Stack:**
  1. Procedural base (biomes, terrain, climate)
  2. Handcrafted POI overlay (Ruins, Starter Cities at fixed coords)
  3. Player layer (Guild towns, portals — DB-persisted, override procedural)

## 2. Networking — Hexagonal Chunked Sharding
- Map divided into hexagonal server segments
- Dynamic sharding: if player density exceeds threshold, dedicated instance spins up for that chunk
- Seamless handoff when players cross chunk boundaries

## 3. Persistent State
- All player-built structures, guild currencies, bank balances → centralized DB, override procedural base
- Reputation & Soul Debt indexed by `PlayerUID` + `FactionID/SkillID`
- Regional reputation cached at tile level; synced to global DB periodically

## 4. Chunk Handoff — Buffer Zone
- Each hex has a ~500m Mirror Zone: adjacent tile data visible but read-only
- Entity authority handed off when crossing hard hex boundary
- Predictive movement used in Mirror Zone for seamless visuals

## 5. Dynamic Dungeons — Overlay Instances
- Trigger: mine resource volume = 0 AND player occupancy < 5% over 24h
- On entry: server spins up lightweight overlay instance on top of original voxel layout
- Dungeon kills/loot tracked independently; physical world (hollow mine) remains static
