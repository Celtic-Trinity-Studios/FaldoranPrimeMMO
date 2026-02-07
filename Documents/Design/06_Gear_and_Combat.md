# 06 Gear and Combat

Faldoran Prime rejects traditional "armor sets" in favor of a granular, attachment-based system that defines character silhouette and visual identity.

## Socket-Based Attachment System
Instead of simple slots (Head, Chest, Legs), characters have ~20+ skeletal sockets.

### Skeletal Sockets
- **Limbs:** Shoulder (L/R), Forearm (L/R), Thigh (L/R), Shin (L/R).
- **Torso:** Chest-Core, Back-Upper, Back-Lower, Hip (L/R).
- **Head:** Crown, Face, Neck.

### "Scrapper" Design / Modular Assembly
- A "Chest Piece" can be composed of multiple discrete items (e.g., leather vest + steel plate + bandolier).
- **Granular Breaking:** Combat damage is hit-boxed. A hit to the left shoulder only damages the item attached to `Shoulder_L`.

## Item Condition & Maintenance: The Dual-Status System
Items in Faldoran Prime track two distinct health-related values to simulate wear, tear, and eventual obsolescence.

### 1. Damage Status (Current Condition)
- **What it is:** The "HP" of the item. 
- **How it drops:** Taking damage in combat, or the "Durability Hit" upon death.
- **Effect:** As Damage Status drops, the item's effectiveness (Attack, Defense, Speed) scales down. At 0%, the item is "Broken" and provides no stats.
- **Repair:** Restores Damage Status to the item's current max (Durability Status).

### 2. Durability Status (Max Potential)
- **What it is:** The "Max HP" or lifespan of the item.
- **How it drops:** **Every repair** permanently lowers the item's Durability Status.
- **Obsolescence:** Eventually, the Durability Status becomes so low that the item is no longer worth repairing, forcing the player to commission a new one from a crafter.
- **Maintenance Loop:** High-skill crafters (Masters) result in a smaller "Reputation Hit" to Durability Status than low-skill or automated repairs.

## Combat Philosophy
- **Glass Cannons:** High-tier magic naturally results in low HP due to the **Somatic vs. Arcane** split.
- **Hybrid Viability:** "Paladins" or Battlemages are possible but will never achieve Rank 100 in both disciplines.
- **Consequences over Classes:** You are defined by the gear you wear and the skills you've maintained.
