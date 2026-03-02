# 00 Rules and Constraints
**Single source of truth for all binding rules. Every file in this project must comply.**

---

## SECTION 1 — Development Rules

### Engine & Build
- **Engine:** UE 5.7.1 Custom at `E:\UEInstalled\Windows`
- C++ first; no Blueprint-only gameplay logic
- All systems must be dedicated-server safe
- Never modify engine or plugin source
- Max 500 lines per source file
- `.generated.h` includes go at the bottom of header include lists
- Build via Visual Studio only (`Build.bat` non-functional)

### Source of Truth
- `Documents/Design/*.md` = authoritative for game mechanics & balance
- Code = authoritative for runtime behavior
- Database schema = authoritative for persistent state
- No code may be invented; all systems designed before implementation

### Workflow
- ONE task per chat session; ONE micro-step at a time
- Each step must compile successfully
- Every new class must specify: **what**, **where** (full path), **parent class**, **one-sentence purpose**
- All major steps produce a doc in `Documents/Workflows/`
- Agent prompts must state: context docs to read, completed prerequisites, exact deliverables

### Version Control
- GitHub Desktop only — no git CLI
- Big tasks on feature branches
- ONE commit per chat session; no commits without approval

### Engineering Principles
- **KISS / DRY / YAGNI / SOLID** — simplest solution, no duplication, no premature features
- **Single Responsibility:** one sentence = one class
- **No magic numbers:** named constants or data table values only
- **Comment WHY, not WHAT** — code explains what; comments explain reasoning
- `F` structs · `U` UObjects · `A` Actors · `E` enums · `I` interfaces · `b` bools

---

## SECTION 2 — MMO Security Mandate

> Security is existential. Every system is designed assuming the client is hostile.

### Non-Negotiable Rules
- **Zero Trust Client:** client is a thin input sender + renderer. It has zero authority.
- **Server is ALWAYS the single source of truth for all game state.**
- All RPCs must be rate-limited; no exceptions
- All data from the client is validated server-side before use
- No credentials, tokens, or secrets compiled into client builds
- All state changes logged (timestamp, account, IP, action)
- Fail closed — reject the entire request on any validation failure
- All ability cooldowns, crafting timers, economy actions are server-side only
- Cooldowns / timers on client = cosmetic only

### Authority Table
| System | Authority | Client Role |
|--------|-----------|-------------|
| Character Creation | Server | Submit request |
| Database | Server | No access |
| Combat | Server | Send input |
| Inventory | Server | Request action |
| Economy | Server | Request trade |
| Building | Server | Request placement |

### Source of Truth Hierarchy
```
Design Documents → Code Implementation → Database Schema → Runtime Data
```
Conflict = higher layer wins, lower layer corrected.

### Startup Gate
- `UFPMServerStartupGateSubsystem` controls lifecycle: **Starting → Holding → Ready → Running**
- All subsystems MUST check `IsGateOpen()` before server-critical init
- Bind to `OnGateOpened` delegate — never poll the gate state

### Production Security (pre-public release)
- [ ] TLS/SSL for all client-server communication
- [ ] Upgrade to bcrypt/Argon2 password hashing
- [ ] Session token system with expiry
- [ ] IP-based throttling and auto-ban
- [ ] Server-side cheat detection
- [ ] Penetration testing before public alpha

---

## SECTION 3 — Game Mechanics Invariants

### Weight & Speed
- Every **1 lb** of character weight (body + inventory) → **−0.1% movement speed** (continuous linear)
- Body Fat adds penalty; Muscle Mass adds weight but also a speed multiplier offset

### Playstyle Affinities
- 6 types: Martial, Ranged, Magic, Crafting, Social, Survival
- Default 100 each · Min 90 · Max 150 · **Total always 600** (zero-sum)
- Permanent; cannot be changed after character creation

### Magical Affinities
- 8 types: Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane
- Default 100 each · **Total always 800** (zero-sum)

### Skill — "Rusty Master" Rule
- Skills hidden until discovered through use/training/observation
- Ranks are **permanent** — you never lose a rank
- XP decays to a "Wisdom Floor" when unused → Rusty state (quality/efficiency drops)
- Rusty crafting = higher Destruction Chance

### Somatic vs Arcane
- High Arcane suppresses HP/Stamina; Mana Crystal batteries, not internal mana
- Heavy armor drains/shatters crystals
- Battlemages viable but can never max both disciplines

### Item Durability
- Damage Status (0–100%, repairable) vs Durability Status (degrades each repair, irreversible)
- Equipped gear **never drops on death** — takes Durability Hit instead

### Death
- Soul Debt: XP Debt pushes skills toward Rusty state
- Inventory drop chance is region-dependent (Sanctuary / Settled / Wilderness / Dungeons)
- Resurrection requires Primal Shrines or Guild Town Obelisks (Crystal-powered)

### Economy
- Player-driven, no global auction house; no universal currency (guild currencies)
- Everything breaks; resources are finite (non-renewable metals/minerals)
- Distance = difficulty; regional scarcity only meaningful if transport is hard

### Building
- Modular/socket-based early → Voxel structural integrity later
- Blueprints for group construction; Territory Shields require Crystal power

### Automation
- All automated systems powered by Mana Crystals or Fuel
- Complexity limits per structure to cap server load

### Portals
- Guild-built only; require Portal Shards + massive resource sink
- Degradation: Starved → Depleted → Wild (random exit) → Fracture (permanent loss)

### Maritime
- Bulk transport sea-only; Portals for people/artifacts
- Ships above Tier 1 require Crystal Core
- Scale Dilation: 1:10 in Deep Water; 1:1 within 10 miles of land

### Resources
- No HUD markers; players read the land
- Deep Mining requires Shaft Elevators (guild infrastructure)
- Abandoned mines → Infestation → dynamic dungeon overlay

### NPC Standing
- Global vs Regional standing tracked separately; tiers from Arch-Nemesis to Exalted
- Hirelings: require standing threshold + food + wages; can Strike or sabotage if standing drops

### Chunk Architecture
- World: Earth-sized (~510M km²), seamless hexagonal chunks
- Only active chunks (player presence or active NPC task) simulated
- Generation deterministic: same seed → same terrain everywhere
- Player changes (structures, depletion) persist to DB and override procedural base

### Monster Spawning
- No fixed spawn points; biome-constrained dynamic spawning
- Spawning prohibited within any Town Anchor radius

---

## SECTION 4 — File & Folder Conventions

| Folder | Purpose |
|--------|---------|
| `Documents/Design/` | Game design bible (numbered chapters) |
| `Documents/Technical/` | Architecture & system design |
| `Documents/Workflows/` | Step-by-step implementation guides |
| `Source/.../Public/` | C++ headers (API surface) |
| `Source/.../Private/` | C++ implementations |
| `Content/` | UE assets (BP, meshes, materials, maps) |

### Naming
- Design docs: `Documents/Design/XX_Topic_Name.md`
- Technical docs: `Documents/Technical/Topic_Name.md`
- Classes: PascalCase with UE prefix (`FPMMyClass`)
- Files match class name exactly
- Assets: PascalCase (`M_MasterMaterial`, `SK_Mannequin`)

---
*Copyright Celtic Trinity Studios, 2026.*
