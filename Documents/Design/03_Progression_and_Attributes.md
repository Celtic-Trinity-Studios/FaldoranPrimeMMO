# 03 Progression and Attributes

Faldoran Prime uses a classless, skill-based progression system where identity is defined by actions and consequences. However, **Biological Identity** (Species/Race) provides the foundational canvas for these skills.

## distinct Species & Biological Morph Targets
Inspired by the variety in *Light No Fire*, Faldoran Prime offers distinct **Species** that drastically alter gameplay, utilizing Character Creator 5's morph capabilities.

### Species Traits
*   **Humans:** The baseline. Balanced stats, high diplomacy/versatility.
*   **Giants/Ogres:** (High Strength/Con, Low Agility). Can carry heavy loads without mounts but cannot ride standard horses.
*   **Smallfolk (Gnomes/Tylwyth):** (High Agility/Stealth, Low Strength). Harder to hit, can access small cave passages, but lower carry weight.
*   **Beast-Kin (Wolf/Badger/Fox):** (High Senses/Speed). Natural resistance to cold/heat, potential for "Scent" mechanics to track players/resources.

> **Design Note:** Species choice is *permanent* (mostly). It defines your interaction with the physics of the world (Carry Weight, Hitbox, Interaction Height).

## The Affinity System

### Playstyle Affinities (Character Genesis)
At character creation, players distribute points into **Playstyle Affinity Multipliers**:
- **Martial** - Melee combat, physical damage
- **Ranged** - Bows, thrown weapons, ranged attacks
- **Magic** - Spellcasting, magical damage (general proficiency)
- **Crafting** - Item creation, gathering efficiency
- **Social** - NPC interactions, trading, diplomacy
- **Survival** - Gathering, exploration, environmental resistance

> **Rules:** See [00 Rules and Constraints — §4.2 Playstyle Affinities](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md) for point distribution rules (defaults, min/max, zero-sum invariant).

### Magical Affinities (Element Specialization)
Separate from playstyle affinities, **Magical Affinities** represent bonuses to specific damage types:
- Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane

> **Rules:** See [00 Rules and Constraints — §4.3 Magical Affinities](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md) for point distribution rules.

**Gameplay Progression:**
In addition to creation choices, affinities are further modified through:
- Equipment bonuses (e.g., Fire Staff grants +Fire Affinity)
- Skill/talent choices
- Quest rewards
- Specialization paths

**Design Rationale:** A character with high Magic (playstyle) can specialize in Fire OR Arcane OR any magical element through gameplay progression.

## Weight and Movement Speed
Character weight is a continuous factor for movement performance.

> **Rules:** See [00 Rules and Constraints — §4.1 Weight and Movement Speed](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md) for the exact formula (0.1% per lb, body composition modifiers).

## Skill Progression & The "Rusty Master" Rule
There is no hard level cap, but progression is balanced through an **Opposing Force Decay Formula**.

### Rank Permanence
- You **NEVER** lose a Rank or the recipes/abilities unlocked by it.
- Decay only attacks your **current XP debt**.

### The "Rusty" State (XP Debt)
If you stop using a skill, your *effective* XP drops (down to a "Wisdom Floor").
- **Effect:** You are "Rusty." Quality and efficiency drop until you grind back the debt.
- **Example:** A Rank 100 Chef returning after a year can still cook high-tier meals but may burn them until they "remember" their flow.
- **Crafting Volatility:** Being "Rusty" in a crafting skill increases the baseline **Destruction Chance** when adding resources to a template (see [10 Dynamic Crafting](file:///e:/FaldoranPrimeMMO/Documents/Design/10_Dynamic_Crafting_and_Templates.md)).

## Attribute Trade-Offs: Somatic vs. Arcane
The body adapts to the energy it channels, creating a natural "See-Saw" between Physical and Magical power.
- **Physical Conditioning (HP/Stamina):** Gained via exertion. High Arcane Attunement suppresses these stats.
- **Arcane Attunement (Resonance/Will):** Gained via spellcasting/meditation. High Magic rank caps Max HP to a percentage of a warrior's potential. Instead of an internal mana pool, casters rely on **Mana Crystals** as external batteries.
- **Armor Interference:** Heavy armor dampens the sympathetic resonance required to draw from Mana Crystals. Mages in plate suffer from poor **Efficiency**, causing crystals to drain rapidly or even shatter upon use.

## Utility/Downtime Skills
Minor magic (e.g., "Minor Healing") is designed to be non-viable in combat (long cast times, armor debuffs) to ensure self-sufficiency without invalidating dedicated roles.
