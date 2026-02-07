# 11 Resource Surveying and Geology

Resources in Faldoran Prime are integrated into the environment through a "Geological Simulation" layer. Finding resources requires professional surveying skills and observation of the terrain.

## 1. Visual Cues (The "Tell")
Resources are not highlighted with HUD markers. Players must learn to "read the land."

| Resource | Telltale Signs / Geological Markers |
| :--- | :--- |
| **Iron** | Red-tinted soil, rust-colored streaks on rock faces, localized magnetic interference (compass jitter). |
| **Copper** | Greenish (Oxidized) patina on cliff edges, specific blue-flowering flora that thrives in copper-rich soil. |
| **Coal** | Dark, crumbling shale layers near riverbeds or exposed ravines. |
| **Gold** | Quartz veins in metamorphic rock, heavy silt deposits at the inner curves of rivers. |
| **Mana Crystals** | Faint bioluminescence at night, "Dead Zones" where normal plants won't grow but strange fungi thrive. |

## 2. Surveying Mechanics
While visual cues provide the "Where," specialized tools provide the "How Much."

- **The Prospector's Hammer:** Used to sample rock. Provides a rough estimate of ore density.
- **Seismic Rods:** (High Tier) Planted in the ground to map underground veins, revealing the total volume (the "Finite Cap") of the deposit.
- **Dowsing:** A Magic-based alternative for finding Mana Crystal or Water sources.

## 3. Extraction & Depletion
Once found, extraction is a one-way street for minerals.

- **Vein Volume:** Every resource node tracks a `TotalUnits` variable determined at world seed generation.
- **Variable Yield:** High-skill miners extract more units per swing, but the total volume remains the same. A master miner doesn't "make more ore," they "waste less."
- **Deep Mining:** Once surface deposits are gone, guilds must build **Shaft Elevators** (Voxel-based digging) to reach deeper, more dangerous veins.

## 4. Geopolitical Impact: The "Migration Engine"
Finite resources transform the map over real-time months.
1.  **Phase: The Rush:** A new iron-rich valley is discovered. Guilds race to build outposts.
2.  **Phase: Prosperity:** Trade towns flourish. Portals are established to export ore.
3.  **Phase: The Squeeze:** Surface iron is gone. Prices rise. Conflict for remaining deep-veins intensifies.
4.  **Phase: Exodus:** The valley is hollow. Guilds abandon portals and towns, moving to the next "Frontier." 
5.  **Phase: The Infestation:** Abandoned mine shafts do not vanish. Once player activity drops below a threshold, the "Instance Overlay" triggers. The mine becomes a dynamic dungeon where monsters (spiders, goblins, or undead) spawn in the dark, hollowed-out chambers.

> [!TIP]
> **Salvaging the Dark:** Infested mines can be re-entered by "Scrapper" classes to find leftover structural materials or rare monster drops, turning a "dead" resource node into a repeatable combat challenge.
> **Trade Viability:** Because iron in the "Starter High-Sec Zones" will eventually run out, the entire global economy relies on the caravans and portals bringing metal from the dangerous, ever-shifting Frontier.
