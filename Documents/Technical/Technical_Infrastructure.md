# Technical Infrastructure

Implementing an Earth-Sized world requires a hybrid generation and networking architecture.

## 1. Generation: Chunk-Based Deterministic Seeded Procedure
- **Shared Mathematical Seed:** Both client and server generate the same terrain from a shared seed to ensure consistency without storing millions of km² of static data.
- **Chunk-Based Rendering:** The client only generates and renders the immediate 10-20km radius of "Active Chunks" surrounding the player.
- **POI-Overlay System:** 
    1. **Base Layer:** Procedural biomes, terrain, and climate simulation within each chunk.
    2. **Handcrafted Layer:** Fixed POIs (Ruins, Starter Cities) stamped at specific coordinates, overriding chunk data.
    3. **Player Layer:** Guild towns and portals overload the procedural data with persistent database entries.

## 2. Networking: Chunked Tiled Sharding
- **Hexagonal Chunks:** The map is divided into hexagonal server segments (Chunks).
- **Dynamic Sharding:** If player density exceeds a threshold (e.g., in a massive siege), the system spins up a dedicated instance for that specific chunk.
- **Seamless Handoff:** Players moving between chunks experience a seamless transition between server nodes.

## 3. Persistent State
- All player-built structures, guild currencies, and bank balances are stored in a centralized persistence layer that overrides the procedural base.
- **Reputation & Soul Debt:** These are indexed by PlayerUID and FactionID/SkillID. To minimize database overhead, regional reputation is cached at the tile level and synced to the global layer periodically.

## 4. Sharding: The Buffer Zone
- **Overlap Mechanics:** Each hex tile has a ~500m "Mirror Zone" where data from adjacent tiles is visible but read-only.
- **Handoff Logic:** Entity authority (player or projectile) is handed off to the new tile server once the entity crosses the hard hex boundary. 
- **Latency Compensation:** Predictive movement is used within the Mirror Zone to ensure seamless visuals during handoff.

## 5. Server Instance Overlays (Dynamic Dungeons)
To support abandoned mines becoming monster dens, the system uses an **Overlay Instance** architecture.
- **Dormant State:** Normal player-built structures are tracked in the standard persistence layer.
- **Trigger Condition:** When a mine's resource volume = 0 AND player occupancy < 5% for a rolling 24-hour period, the system flags the coordinates for "Infestation."
- **Instance Activation:** Upon player entry into an Infested Mine, the server spins up a lightweight instance that "overlays" the original voxel layout.
- **Persistence:** Monster kills and loot chests in the overlay are tracked independently of the planetary base layer, allowing the dungeon to "reset" while the physical world (the hollow mine) remains static.
