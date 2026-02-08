# Inspiration from *Ashes of Creation* for Faldoran Prime

*Ashes of Creation* (AoC) is one of the most anticipated MMORPGs currently in development. Its core philosophy revolves around a **Player-Driven World** where the server state is not static but evolves based on player actions.

Below are the key features players love about AoC, along with notes on how they could be adapted for **Faldoran Prime**.

---

## 1. The Node System (Dynamic World)
**The Concept:**
The world is divided into distinct zones called **Nodes**. Every player action (questing, killing mobs, gathering) contributes "experience" to the local Node.
- **Levels:** Nodes progress through stages: Wilderness -> Expedition -> Village -> Town -> City -> Metropolis.
- **Unlocks:** As a Node levels up, it spawns NPC shops, quest givers, crafting stations, and even new dungeons in the area.
- **Politics:** High-level nodes have elections for Mayors who manage taxes and building projects.
- **Conflict:** Rival nodes can declare war or be besieged to lower their level, destroying player housing and infrastructure.

**Adaptation for Faldoran Prime:**
*   **Implementation:** Create a `UNodeManager` class that tracks an "Experience" float for each region.
*   **Gameplay Loop:** When a global threshold is reached, use the **GameMode** to spawn specific Actors (vendors, walls) or enable travel to specific dungeon maps.
*   **Player Value:** Players feel their grinding actually *changes* the world, rather than just increasing their own level.

## 2. The Caravan System (Risk vs. Reward Economy)
**The Concept:**
Regional resources are distinct. You might find *Iron* in the mountains but *Wood* in the forests. To craft high-level gear, you need both.
- **Transportation:** Players cannot carry infinite items. To move bulk resources, they must build and summon a **Caravan** (a wagon).
- **PvP Zone:** The Caravan acts as a moving PvP zone. Other players can attack it to steal a portion of the loot.
- **Defender Loop:** The owner hires other players as guards. If the delivery succeeds, everyone gets paid. If it fails, the attackers get the loot.

**Adaptation for Faldoran Prime:**
*   **Implementation:** A `ACaravan` actor that moves slower than players and has high health. It contains a separate inventory component.
*   **Events:** When a player "starts" a trade run, announce it to nearby players or display it on the map to encourage conflict.
*   **Economy:** This creates an infinite loop of content: Gatherers need Transporters -> Transporters need Guards -> Bandits need targets -> PvP happens naturally.

## 3. The Class Augment System (Horizontal Progression)
**The Concept:**
AoC avoids the "Holy Trinity" stalemate by allowing hybrid classes.
- **Primary Archetype:** Chosen at character creation (e.g., Fighter, Mage, Cleric). Determines your *active skills*.
- **Secondary Archetype:** Chosen later (level 25). Does *not* give new skills, but **Mutates** your primary skills.
- **Examples:**
    - **Fighter (Primary) + Mage (Secondary):** Your "Charge" ability now teleports you and leaves a trail of fire (Spellsword).
    - **Cleric (Primary) + Bard (Secondary):** Your "Heal" spell now also applies a buff that restores Mana over time.

**Adaptation for Faldoran Prime:**
*   **Code Structure:** Use the Strategy Pattern or Components.
    *   `UAbilityBase` has a function `ExecuteAbility()`.
    *   The Secondary Class acts as a modifier, e.g., `UAugmentComponent`.
    *   When `ExecuteAbility` runs, it checks `GetAugment()` to see if it should play a different VFX or apply a different `GameplayEffect` (if using GAS).

## 4. Open World Housing (Freeholds)
**The Concept:**
Housing isn't just cosmetic instance. High-level players can claim a plot of land in the open world called a **Freehold**.
- **Utility:** These are functional bases with farms, lumberyards, and processing stations.
- **Vulnerability:** If the Node creates a siege and loses, the Freeholds can be destroyed or de-leveled.

**Adaptation:**
*   Allow players to place a "Claim Flag" actor in designated zones.
*   This actor spawns other structures (walls, workbenches) relative to itself.
*   Save this data to your `PostgreSQL` database (Location, OwnerID, StructureTier).

## Summary Recommendation
If you only pick **one** feature to prototype, make it the **Node System** or a simplified **Caravan** mechanic. These provide the highest "content per hour" value because they generate infinite player-driven quests (defend the wagon, raid the wagon) without you needing to write thousands of lines of dialogue.
