# 05 Building and Settlements

The building system in Faldoran Prime balances creative freedom with structural realism and economic impact.

## Core Philosophies

### 1. The "Evolutionary" System
- **Early Game:** Modular/Socket-based building (snapping pieces like *Rust/Ark*) for quick survival.
- **Advanced Gamne:** Unlocking Voxel tools for fine-tuning and **Structural Integrity** physics (inspired by *Valheim*), allowing for complex, engineering-focused builds.

### 2. The Blueprint System (Ghost Placement)
- Players place "ghost" versions of structures to plan layouts before committing resources.
- **Interdependence:** Multiple players can contribute resources to a single "Town Blueprint."

## The "Master Builder" Economy
- **Skill Requirements:** Advanced structures (Fortified Gates, Guild Vaults) require high-rank Architecture or Engineering skills.
- **Consumable Blueprints:** Master builders can "package" their designs into single-use physics Blueprint Items to sell to other players or guilds.

## Territory & Meaningful Building
- **Land Leasing:** Guilds can lease segments of their territory to individual players, creating vassalage and social hierarchy.
- **Territory Shields:** Powered by "Mana Crystals," shields provide 100% protection during peace time but drop to 20% (vulnerability windows) during wars.

## Terraforming (Player-Owned Areas)

Players and guilds with **territory ownership** can modify the terrain within their claimed land. This is a core feature — not cosmetic.

### What Terraforming Allows
- **Raise/Lower Terrain:** Push terrain height up or down within bounds (max ±50m from original procedural height).
- **Flatten:** Level an area to a target height for building foundations, roads, or farm plots.
- **Carve/Dig:** Create trenches, moats, basements, mine entrances. Depth limited by bedrock layer.
- **Sculpt:** Free-form terrain shaping for defensive earthworks, berms, hills.
- **Paint/Rezone Biome:** Change the surface material within owned land (e.g., convert forest floor to farmland, lay cobblestone paths). Does NOT change the underlying biome type for spawn/resource purposes — it's cosmetic painting only.

### Rules & Constraints
1. **Territory Lock:** Terraforming tools are ONLY available within player/guild-owned territory boundaries. You cannot modify terrain in the wild.
2. **Server Authority:** All terrain modifications are validated server-side. The client sends "sculpt requests" and the server applies them. See `00_Rules_and_Constraints.md` §2.
3. **Persistence:** Terrain modifications are stored as **delta overlays** in the database, overriding the procedural base terrain. When the chunk loads, the server applies: `Final Height = Procedural Base + Stored Delta`. See `Technical_Infrastructure.md` §3.
4. **Undo on Abandonment:** If territory ownership lapses (guild disbands, lease expires, conquered in war), terrain modifications **gradually revert** to the procedural base over a configurable decay period (default: 7 real days). This prevents permanent scarring of the world.
5. **Volume Limits:** Each territory claim has a maximum terraforming budget (cubic meters of displaced earth). Larger claims = more budget. Prevents hollowing out entire mountains.
6. **Structural Dependency:** Buildings placed on terraformed ground maintain a reference to the terrain height. If the terrain reverts (ownership lost), buildings lose structural support and collapse.

### Terraforming Tools (Progression)
| Tool | Unlock | Capability |
| :--- | :--- | :--- |
| **Wooden Shovel** | Default | Raise/Lower ±2m, 3m radius, slow |
| **Iron Spade** | Crafting Rank 5 | Raise/Lower ±10m, 5m radius, medium |
| **Mason's Level** | Architecture Rank 10 | Flatten to exact height, 10m radius |
| **Guild Earthworks** | Guild Tech Tree | Raise/Lower ±50m, 20m radius, carve/dig |
| **Biome Painter** | Farming or Architecture skill | Repaint surface texture within territory |

### Technical Integration
- **Landscape System:** Terraforming modifies the UE5 Landscape heightmap at runtime via `FLandscapeEditDataInterface::SetHeightData()`. The same API used by `FPMTerrainGenerator` during world gen.
- **Material Interaction:** The procedural landscape material (`M_Landscape_StarterIsland`) reads world position for biome painting. Terraformed areas that change height will naturally shift which biome texture appears (e.g., raising terrain above the snow line will paint it as snow). Biome Painter tool overrides this with a per-territory material layer.
- **Networking:** Terrain deltas are replicated to all clients in the chunk. Large terraforming operations are queued and applied over multiple frames to prevent hitching.

## Summary of Mechanics
| Feature | Reference | Priority |
| :--- | :--- | :--- |
| **Material Tiers** | Wood -> Stone -> Metal | High |
| **Integrity Physics** | Structural Support | Medium |
| **Terraforming** | Player-owned territory terrain modification | High |
| **Biome Painting** | Cosmetic surface texture override in territory | Medium |
| **Group Blueprints** | Shared Construction | High |
