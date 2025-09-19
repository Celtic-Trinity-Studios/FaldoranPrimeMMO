Faldoran Prime — Core Rules & Systems (Draft v0.1) Working draft for the Rules document. Focus: MMO/Survival, no character levels, fully player-driven economy, crafting-centric progression with risk of material destruction on failure. All numbers are placeholders.

Design Pillars Skill-Based, Not Level-Based: Characters progress via skills that improve when used. No global character level.
Player-Run Economy: Nearly every usable item is crafted by players; world drops are mostly inputs, not finished goods. A handful of super-rare items (relics) bypass crafting.

Risk + Reward: Attempts carry meaningful risk (time, materials) and potential for destruction on failure. Higher skill = lower risk.

Survival Is Gameplay: Food, water, temperature, shelter, injury, and disease systems create demand for crafted goods and services.

Specialization With Synergy: Most tasks touch multiple skills (primary + supporting) so groups and trade matter.

Skills & Progression Skill List (initial pass):
Gathering: Foraging, Logging, Mining, Hunting, Fishing.

Processing: Smelting, Tanning, Milling, Weaving, Alchemy (refining tier-0 → tier-1 → tier-2 mats).

Crafting: Smithing (weapons/armor), Carpentry, Fletching, Leatherworking, Tailoring, Cooking, Brewing, Engineering (tools, traps, devices), Architecture (structures), Medicine (consumables/first aid), Artificing (magitech/rare focus).

Survival: Navigation, Campcraft, Agriculture, Animal Handling, Stealth, Athletics, Medicine (field use), Firecraft.

Social/Economic: Trading, Appraisal, Negotiation, Contracts.

Combat Techniques: Blade, Blunt, Ranged, Unarmed; Defensive: Block, Dodge, Parry; Tactics: Positioning, Threat.

Skill Ranks: 0–100 (soft-capped; diminishing returns after 75).

Gaining XP: Any action tied to a skill grants XP. Actions may fan out XP to multiple skills via weights (see §3).

Decay (Optional toggle): Skills above 90 may decay slowly when unused (keeps economy dynamic). (Decide later.)

XP Allocation Model (Per Action) Each action declares Skill Weights totaling 100%. The action grants a base XP bundle X, split by those weights and modified by difficulty. Formula (draft): XP_to_skill = X * weight% * DifficultyMultiplier * AntiGrindFactor
DifficultyMultiplier: 0.5–2.0 based on the action’s relative difficulty vs current skill (see §5: Skill Checks).

AntiGrindFactor: Diminishing returns for repeating identical actions; resets by variety/biome/recipe tier.

Examples: Action Weights Chop Tree (Tier-1) Logging 70%, Athletics 20%, Campcraft 10% Smelt Iron Ingot Smelting 80%, Firecraft 10%, Appraisal 10% Craft Shortbow Carpentry 50%, Fletching 40%, Appraisal 10% Field Dress Deer Hunting 60%, Tanning 30%, Medicine 10%

Itemization & Rarity Inputs First: World primarily yields resources; finished items are crafted.
Quality Tiers: Common, Fine, Superior, Masterwork, Exceptional, Relic (super-rare, mostly non-craftable).

Affixes (Optional): Prefix/Suffix pool unlocked by skill thresholds, stations, or relic catalysts.

Durability: Items degrade through use; repair restores up to current max durability which also decays unless reinforced.

Crafting System Goal: Reward skill investment; preserve meaningful risk; make the economy the content. 5.1 Recipe Difficulty & Skill Check Every recipe has a Recipe Difficulty (RD) based on materials, tier, and required precision. Success Roll (draft): EffectiveSkill = PrimarySkill + Σ(SupportSkill * support_coeff) + ToolQualityBonus + StationBonus + ConsumableBuff Delta = EffectiveSkill - RD SuccessChance = clamp( Base + Δ * Slope, Min, Max )
Base: 40% | Slope: 0.8% per point of Δ | Min/Max: 5%/95% (placeholders)

Critical Bands: If Δ ≥ +20 → Great Success (bonus quality/extra output). If Δ ≤ -20 → Mishap (see below).

5.2 Outcomes Great Success: +1 quality tier or +10–25% output (config per recipe).

Success: Item crafted at rolled quality (see 5.3) and consumes required materials.

Near Miss (Optional): -50% output or lower quality; keeps some materials. (Toggle for new-player friendliness.)

Failure: No item; material loss chance per component.

Mishap: Tool durability hit; small AoE hazard (e.g., furnace flare) at high-tier stations.

5.3 Rolling Quality QualityScore = Δ + RNG(0, q_spread) + rare_modifiers Map QualityScore to tier thresholds per recipe.

Tools, stations, and rare catalysts raise the quality floor.

5.4 Material Destruction Rules Each component has a Destruction Probability on Failure P_destruction by rarity: Common 30%, Fine 40%, Superior 55%, Masterwork 70%, Relic 90%.

Salvage Roll: Appraisal/Engineering can recover fragments; increases with skill/station.

5.5 Crafting Stations Tiered stations gate recipe tiers and add StationBonus; require upkeep (fuel, parts) → resource sinks.

Permissions: settlement-owned stations can set tax/access rights to fund towns.

Economy (Player-Driven) CURRENCY: Single soft currency (name TBD) earned via NPC buy orders, contracts, events. Barter stays viable.
MARKET STRUCTURE:

Local markets with distance friction (transport risk). Optional regional auction house with listing + sale taxes.

Contracts: Buy orders, sell orders, work orders (e.g., "craft 10 masterwork picks").

SINKS (Inflation Control): Repair decay, station upkeep, property taxes, travel fees, listing taxes, blueprint research, cosmetic vanity, consumable demand.

ANTI-EXPLOIT: Server-wide anti-duplication, trade logging, suspicious-profit alerts, cooldowns on new accounts selling high-value items.

DISCOVERY: Blueprints drop as fragments; research to unlock permanent recipes; temporary recipe scrolls encourage trade.

Survival Loops Needs: Hunger, thirst, temperature, disease, injury. Crafting creates solutions: food, clothing, shelters, medicine.
Environment: Biomes shift resource tables; weather affects stations (windmills, drying racks) and travel risk.

Death Penalties (draft): Drop backpack (recoverable), gear durability hit, short debuff. (Tune for fairness.)

PvE, PvP, and Risk PvE: Dungeons/rifts as material fountains not gear fountains; outputs are rare reagents, plans, catalysts.
PvP Rules (draft):

Flagging/consent rules by region tier. High-tier zones enable full loot backpack only.

Bounty system to curb griefing; social skills (Contracts/Negotiation) enable mercenary enforcement.

Examples (Concrete Task → Skills → Risk) Craft Iron Pickaxe (T2): Primary: Smithing; Support: Carpentry (handle), Appraisal. RD = 60. Uses T2 ingots + hardwood. Failure destroys some ingots; mishap can crack the anvil (station damage chance).
Brew Antiseptic: Primary: Brewing; Support: Foraging, Medicine. Needs herbs + alcohol base. Great Success yields extra doses.

Build Field Shelter: Primary: Architecture; Support: Carpentry, Campcraft. Requires tools, rope, planks. Near Miss produces a lower-durability shelter.

Tuning Dials (for balancing) Skill gain curve (0–75–100 pacing; decay on/off).
SuccessChance base/slope/min/max; Near Miss toggle.

P_destruction per rarity; salvage recovery curve.

Station bonuses and upkeep costs.

Tax rates and currency drop rates.

AntiGrind variety thresholds and cooldowns.

Open Questions (for next pass) Confirm final skill list and rename/merge where needed.
Decide on Near Miss behavior (on/off; recipe-specific?).

Choose currency name and whether regional currencies exist.

Determine blueprint acquisition rates and permanence.

Finalize death penalties by region tier.

Accessibility levers for new players (tutorial station, starter recipes, protected zone).

Glossary RD (Recipe Difficulty): Target number for the skill check.
Δ (Delta): EffectiveSkill − RD.

Relic: Super-rare item typically found, not crafted, sometimes used as a catalyst in top-tier crafting.

Status: Draft v0.1 — ready for your edits. Mark TODOs inline and we’ll lock mechanics next.
