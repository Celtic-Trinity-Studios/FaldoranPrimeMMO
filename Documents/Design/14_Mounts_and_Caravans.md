# 14 Mounts, Caravans & Land Traversal

In a planetary-scale world, travel is a primary gameplay loop. This document outlines the mechanics for mounts, the infrastructure of trade routes, and the "Scale Dilation" system that makes Earth-sized travel possible while maintaining the integrity of the wilderness.

---

## 1. The Core Philosophy of Distance
In Faldoran Prime, **Distance = Content**. 
- **Logistical Friction:** Moving 1,000 miles should feel like a significant undertaking. 
- **Economic Engine:** Regional scarcity is only meaningful if transporting goods is a challenge.
- **The "Highroad" vs. "Wilderness":** Civilized paths offer speed; the trackless wild offers resources and danger.

---

## 2. Mount Taxonomy (Speeds & Capacity)
Mounts are classified into Tiers based on their rarity, training requirements, and utility.

### Ground Mounts
| Tier | Category | Example | Cruising Speed | Max Carry | Special Trait |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **T1** | **Common** | Donkey, Pony | 8 - 12 km/h | 1 Chest | **Hardy:** Low food requirement. |
| **T2** | **Utility** | Camel, Draft Horse | 12 - 18 km/h | 3 Chests | **Environment Spec:** Desert/Arctic. |
| **T3** | **Elite** | Dire Wolf, Raptor | 25 - 40 km/h | Personal | **Agile:** Can jump or climb steep slopes. |
| **T4** | **Behemoth** | Mammoth, Koli Beetle | 6 - 15 km/h | 12+ Chests | **Siege Ready:** Can carry structures. |
| **T5** | **Magitech** | Mana-Strider | 45 - 60 km/h | 2 Chests | **Vibration-Damped:** Does not lose stamina. |

### Aerial Mounts
Aerial travel is limited by **Oxygen/Altitude** and **Stamina**.
- **Tier 3 (Gliders):** Sky-Mantars. Use thermals. Efficient but slow (40 km/h).
- **Tier 4 (Predators):** Gryphons, Wyverns. High speed (90 km/h+). High maintenance.
- **Tier 5 (Ancient):** Dragons. Legendary status. Requires guild-level resources to sustain.

---

## 3. Scale Dilation: The "Highroad" System
To allow for Earth-scale traversal without 10-year real-time journeys, Faldoran Prime uses **Coordinate Dilation**.

### The Three Scales of Land Travel:
1.  **The Wilderness (1:1 Scale):** Every meter is represented. Default for all off-road travel. Used for surveying, hunting, and skirmishes.
2.  **The Frontier Track (1:5 Scale):** Dirt roads or player-cleared paths. Movement speed is effectively multiplied by 5 in the world coordinate system.
3.  **The Imperial Highroad (1:10 Scale):** Paved, guild-maintained stone roads. A mount moving at 20 km/h effectively covers 200 "World Kilometers" per hour.

---

## 4. Caravans & Persistence
Caravans are the lifeblood of the global economy, moving bulk materials too heavy for portals.

### The Caravan Bubble
When 3+ pack animals are tethered, they form a **Caravan Entity**. 
- **Offline Movement:** Using [09 Logic Gems](file:///e:/FaldoranPrimeMMO/Documents/Design/09_Logic_and_Automation.md), a caravan can be programmed to follow a specific road autonomously while the owner is offline.
- **The Campfire Mechanic:** Players can "Rest" inside a large caravan (like a Mammoth Howdah or a Wagon). Logging off inside a moving caravan allows the player to travel across continents while sleeping in real life.
- **Risk:** Offline caravans are vulnerable to "Bandit NPC" spawns or player raids if not protected by "Guard Gems" or hired NPC mercenaries.

---

## 5. Animal Husbandry & Training
Mounts are not static items; they are biological entities that improve with the [13 Animal Husbandry](file:///e:/FaldoranPrimeMMO/Documents/Design/13_Skills_and_Abilities.md) skill.

- **Selective Breeding:** High-tier breeders can produce mounts with +10% Speed or +20% Stamina modifications.
- **Mount Training:** A "Trained" mount can perform "Burst Gallops" or defensive maneuvers in combat.
- **The "Spook" Meter:** Wild predators or magic effects can scare mounts. High **Beast-mastery** reduces the chance of being thrown from your saddle.

### 5b. Legendary Taming (The "Avatar" Moment)
Beyond buying horses, Guilds can organize raids to **Bind Legendary Beasts**.
*   **Procedural Beasts:** The world generates unique "Alpha" predators (e.g., a lightning-infused Wyvern).
*   **The Binding Ritual:** These cannot be tamed by one person. A Guild must weaken the beast, then use **Animist Chains** (Magic) to bind it.
*   **Guild Asset:** A Dragon is not a personal pet; it is a **Guild Vehicle**. It requires immense food upkeep and acts as a bomber/troop transport in war. If it dies, it is gone forever.

---

## 6. Maintenance & Logistics
- **Stabling:** Mounts left in the wild for too long may wander off or be eaten. Cities provide "Stable Master" NPCs for a fee.
- **Feed Quality:** High-protein feed (crafted via [10 Dynamic Crafting](file:///e:/FaldoranPrimeMMO/Documents/Design/10_Dynamic_Crafting_and_Templates.md)) provides temporary speed buffs.
- **Gear:** Saddles, Barding, and Cargo Baskets determine the mount's final carry limit and armor rating.

---

> [!TIP]
> **Proximity Awareness:** Caravans moving through a 1:10 Highroad will automatically "Snap" back to 1:1 scale if an enemy player or hostile monster enters their **Detection Radius**. This prevents "Scale Ganking" where high-speed travelers cannot be caught.
