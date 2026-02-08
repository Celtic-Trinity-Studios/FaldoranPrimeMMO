# 00 Rules and Constraints

This is the **single source of truth** for all binding rules, constraints, and invariants that govern Faldoran Prime's design and development. Every document, system, and code file in this project must comply with these rules.

**Last Updated:** 2026-02-07

---

## ⚠️ MMO Security Mandate

> **Faldoran Prime is a Massively Multiplayer Online game. Security is not a feature — it is existential.**
>
> Every system, every RPC, every database query, every piece of state must be designed under the assumption that the client is **hostile, compromised, and actively trying to cheat**. A single exploit in an MMO can destroy the economy, ruin player trust, and kill the game.
>
> **Security is not deferred. It is built into every phase from day one.**

### Core Security Principles

1. **Defense in Depth:** No single layer is trusted. Validation occurs at the network layer, the RPC layer, the subsystem layer, and the database layer.
2. **Least Privilege:** Every system has the minimum access it needs. Clients have zero direct access to any server-side system.
3. **Zero Trust Client Model:** The client is a **thin input sender and state renderer**. It has no authority over any game state. Period.
4. **Assume Breach:** Design every system as if the client binary has been fully reverse-engineered. Obfuscation is not security.
5. **Secure by Default:** New systems must be locked down by default. Access is explicitly granted, never implicitly assumed.

### Network Security Rules

1. **No plaintext credentials in production.** Prototype may use RPC for login; production MUST use encrypted channels (TLS/DTLS).
2. **All RPCs must be rate-limited.** No RPC endpoint may be called without a per-account or per-connection rate limiter.
3. **Packet validation:** All incoming data must be bounds-checked and type-validated before processing.
4. **Session management:** Each authenticated connection gets a server-generated session token. Tokens expire and are non-transferable.
5. **Connection abuse detection:** Track connection attempts per IP. Auto-ban after threshold (e.g., 50 failed logins/hour).
6. **No client-to-client direct communication.** All player interaction routes through the server.

### Credential & Secret Management

1. **Database credentials** are stored in server-side config only (`DefaultGame.ini`). They are **never** compiled into client builds.
2. **Passwords** are hashed with a strong algorithm (bcrypt or Argon2 for production; SHA-256 + salt acceptable for prototype).
3. **API keys, tokens, and secrets** must never appear in source control. Use environment variables or encrypted config for production.
4. **Client builds must be stripped** of any server-only code paths, config sections, or debug commands before distribution.

### Production Security Roadmap (Post-Prototype)

These are **not required for the prototype** but are **mandatory before any public-facing deployment**:

- [ ] TLS/SSL encryption for all client-server communication
- [ ] Upgrade password hashing from SHA-256 to bcrypt/Argon2
- [ ] Implement proper session token system with expiry and refresh
- [ ] Add server-side packet encryption (beyond UE's built-in)
- [ ] Deploy database with SSL connections and certificate auth
- [ ] Implement IP-based connection throttling and auto-ban
- [ ] Add server-side cheat detection (speed hacks, teleportation, impossible actions)
- [ ] Strip all debug/test console commands from shipping builds
- [ ] Penetration testing before public alpha
- [ ] Set up monitoring and alerting for suspicious activity patterns
- [ ] Database connection pooling with read replicas for scalability
- [ ] Implement proper secrets management (e.g., HashiCorp Vault, AWS Secrets Manager)

---

## 1. Development Rules

### 1.1 Engine & Build
- **Engine:** Unreal Engine 5.7.1 (Custom Compile), installed at `E:\UEInstalled\Windows`
- **Priority:** C++ first, UMG on top. No Blueprint-only gameplay logic.
- **Replication:** All gameplay systems must be dedicated-server safe.
- **Engine Code:** Do not modify engine or plugin source code. Ever.
- **File Size:** No source file may exceed 500 lines (NASA standard).
- **Include Order:** All `.generated.h` includes must be at the **bottom** of the include list in header files.
- **Build:** Always compile using Visual Studio; `Build.bat` is non-functional.
- **UProject Sync:** Any change requiring project file regeneration must be explicitly flagged.

### 1.2 Source of Truth
- **No Invention:** No code may be invented. All systems must be designed before implementation.
- **No Assumptions:** No systems may be assumed to exist. Verify before referencing.
- **Patterns:** Use existing architecture and patterns (Bridge Pattern, Data Contracts, Validation).
- **Design Documents** (`Documents/Design/*.md`) are the authoritative source for game mechanics and balance values.
- **Code Implementation** is the authoritative source for actual runtime behavior and must faithfully implement design specifications.
- **Database Schema** is the authoritative source for runtime persistent state.

### 1.3 Workflow
- **Scope:** ONE task per chat session.
- **Micro-steps:** ONE micro-step at a time. No skipping steps.
- **Stability:** Each step must compile successfully.
- **Transparency:** Explain WHY big logical decisions are made.
- **Path Specification:** Always provide full absolute file paths or Content Browser paths.
- **Step Numbering:** List Step Number and total Number of Steps for big logical phases.
- **Step Documentation:** For each major step, create a document in `Documents/Workflows/`.

### 1.4 Version Control
- **Tooling:** GitHub Desktop ONLY. Agent must not use git command line.
- **Branching:** Big logical tasks should occur on a new feature branch.
- **Approvals:** No commits until explicitly approved.
- **Granularity:** Each chat session results in exactly ONE commit.
- **Safety:** All chats must be safely revertible.
- **New Additions:** Ask for approval before adding anything new.

### 1.5 Core Engineering Principles
- **KISS (Keep It Simple, Stupid):** Avoid unnecessary complexity and over-engineering. The simplest solution that fulfills the design intent wins.
- **DRY (Don't Repeat Yourself):** Never duplicate data or logic. Use functions, subsystems, and shared utilities to reuse code. If the same logic exists in two places, extract it.
- **YAGNI (You Aren't Gonna Need It):** Do not add functionality until it is necessary. Build what the current phase requires — nothing more.
- **SOLID Principles:**
  - **Single Responsibility:** Each class/file does one thing. If you can't describe its purpose in one sentence, split it.
  - **Open/Closed:** Systems are open for extension but closed for modification. Use interfaces and inheritance over editing working code.
  - **Liskov Substitution:** Subclasses must be substitutable for their base classes without breaking behavior.
  - **Interface Segregation:** Don't force classes to implement interfaces they don't use. Keep interfaces small and focused.
  - **Dependency Inversion:** Depend on abstractions (interfaces), not concrete implementations. Subsystems communicate through well-defined APIs.
- **Readability is Priority:** Code is read far more often than it is written. Clear structure and descriptive names always beat clever tricks.
- **"If It Works, Don't Touch It"** *(as a caution):* Unnecessary changes to working, complex code often introduce bugs. Refactor with purpose, not for aesthetics.

### 1.6 Coding Style & Standards
- **Follow UE5 Conventions:** Use Unreal's coding standard — `F` prefix for structs, `U` for UObjects, `A` for Actors, `E` for enums, `I` for interfaces, `b` prefix for bools.
- **Use Vanilla Where Possible:** Avoid unnecessary third-party libraries, frameworks, or dependencies. If UE provides a solution, use it. External dependencies (e.g., libpq) require explicit justification.
- **Clear Naming Conventions:**
  - Use descriptive names. `CharacterCreationValidator` over `CCVal`.
  - Function names describe what they do: `ValidateCharacterName()` not `CheckName()`.
  - Avoid single-letter variables except loop counters.
  - No obscure abbreviations — if it's not immediately obvious, spell it out.
- **Document "Why" Over "What":** Comments explain the *reasoning* behind a decision, not what the code is doing. The code itself should be clear enough to explain "what."
  ```cpp
  // BAD: Increment counter
  Counter++;
  
  // GOOD: Rate limit resets every 60s; track attempts to detect brute-force login
  LoginAttemptCount++;
  ```
- **No Magic Numbers:** Use named constants, enums, or data table values. Never embed raw numbers in logic.
  ```cpp
  // BAD
  if (Name.Len() > 20) { ... }
  
  // GOOD
  static constexpr int32 MaxCharacterNameLength = 20;
  if (Name.Len() > MaxCharacterNameLength) { ... }
  ```
- **Header / Source Separation:** Public API in `.h`, implementation in `.cpp`. Headers should be minimal — forward-declare where possible.

---

## 2. Server Authority Rules

These rules are **non-negotiable** and apply to every system in the project.

### 2.1 The Golden Rule
> **The dedicated server is the single source of truth for ALL game state. No exceptions.**

### 2.2 Client-Server Trust Model
1. **Never trust client input** — All client data is considered hostile until validated server-side.
2. **Clients request, server validates** — No client action is trusted without server validation.
3. **Server owns all persistent state** — Database writes only occur server-side.
4. **Replication is one-way** — Server → Client. Clients never replicate to server.
5. **Client prediction is cosmetic only** — Any client-side state is preview/UX, not authoritative.
6. **Client UI code NEVER contains validation logic** — Only UX hints (e.g., graying out a button).

### 2.3 Anti-Cheat Principles (MMO-Critical)
1. **Validate everything** — All client requests validated against design contract rules. Every field, every value, every range.
2. **Rate limit everything** — All client-facing RPC endpoints must have rate limiting. No endpoint is exempt.
3. **Audit everything** — All state changes logged with timestamp, account, IP, and action type. Logs are append-only.
4. **Fail closed** — If validation fails, reject the entire request. No partial application. No "best effort."
5. **Separation of concerns** — Server validation code is separate from client code. Design contracts are the single source of truth for validation rules.
6. **Bounds check all data** — Every numeric value from the client must be checked against min/max bounds. Every string must be length-limited. Every enum must be range-checked.
7. **Idempotency** — Critical operations (account creation, character creation, transactions) must be idempotent to prevent duplication exploits.
8. **Cooldowns are server-side** — All ability cooldowns, crafting timers, and interaction delays are tracked on the server. Client timers are cosmetic only.
9. **Economy is server-authoritative** — Item duplication, gold generation, and trade exploits are existential threats to an MMO. Every item transfer must be transactional and logged.

### 2.4 Authority Boundaries

| System | Authority | Client Role |
|--------|-----------|-------------|
| Character Creation | Server | Submit request, preview only |
| Database | Server | No direct access |
| Combat | Server | Send input, server simulates |
| Inventory | Server | Request action, server validates |
| Economy | Server | Request trade, server commits |
| Building | Server | Request placement, server validates |
| Crafting | Server | Request craft, server validates |
| NPC Interaction | Server | Request action, server validates |

### 2.5 Database Rules
- Database is **server-authoritative only** — Clients never write directly.
- All writes must go through the `FPMDatabaseSubsystem` API.
- Schema migrations must be versioned and backward-compatible during rollout.
- Connection credentials must **never** be exposed to clients.
- Database transactions are atomic and logged.

### 2.6 Server Startup Gate
- The `UFPMServerStartupGateSubsystem` controls the server lifecycle: **Starting → Holding → Ready → Running**.
- All subsystems **MUST** check `IsGateOpen()` before performing server-critical initialization.
- Systems **MUST NOT** poll or race the gate state — bind to the `OnGateOpened` delegate.
- No gameplay systems may process requests until the gate is open.

---

## 3. Design Contract Rules

### 3.1 Authority Hierarchy
```
Design Documents (Intent)
    ↓
Code Implementation (Behavior)
    ↓
Database Schema (State)
    ↓
Runtime Data (Actual Values)
```

If there is a conflict between layers, the **higher layer wins** and the lower layer must be corrected.

### 3.2 Cross-Boundary Communication
- Design → Code: Data Tables, Config files (read-only from code).
- Code → Database: `FPMDatabaseSubsystem` API (server-only writes).
- Server → Client: Replication, RPCs (server-authoritative).
- Client → Server: RPC requests only (always validated).

### 3.3 Enforcement
- Design documents are version-controlled; changes require explicit commits.
- Breaking changes to design require corresponding code updates.
- All database schema changes require review.
- All design document changes require review.
- Code reviews must enforce data table usage (no magic numbers).

---

## 4. Game Mechanics Rules

These are the **binding invariants** that must be respected by all systems.

### 4.1 Weight and Movement Speed
- Every **1 lb** of total character weight (body + inventory) reduces movement speed by **0.1%**.
- This is a **continuous linear scale**, NOT a threshold system.
- **Body Fat** contributes to weight penalty.
- **Muscle Mass** adds weight but provides a speed increase multiplier that offsets the penalty.

### 4.2 Playstyle Affinities (Character Creation)
- **6 affinities:** Martial, Ranged, Magic, Crafting, Social, Survival.
- **Default:** Each starts at **100 points**.
- **Minimum:** 90 points per affinity (can take up to 10).
- **Maximum:** 150 points per affinity (can receive up to 50).
- **Total:** Always **600** (zero-sum invariant).
- These are **permanent** XP/effectiveness multipliers. Cannot be changed after creation.

### 4.3 Magical Affinities (Character Creation)
- **8 affinities:** Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane.
- **Default:** Each starts at **100 points**.
- **Total:** Always **800** (zero-sum invariant).
- These are a **separate system** from playstyle affinities.

### 4.4 Skill Progression — "Rusty Master" Rule
- **Discovery Mechanic:** All skills are hidden from the player's UI until they are discovered through use, training, or observation.
- **Rank Permanence:** You **NEVER** lose a Rank or the recipes/abilities it unlocked.
- **XP Decay:** If you stop using a skill, your effective XP drops to a "Wisdom Floor."
- **Rusty State:** Quality and efficiency drop until you grind back the XP debt.
- **Crafting Impact:** Being "Rusty" in a crafting skill increases baseline Destruction Chance.

### 4.5 Somatic vs. Arcane Trade-Off
- High Arcane Attunement **suppresses** Physical stats (HP/Stamina).
- High Magic rank **caps** Max HP to a percentage of a warrior's potential.
- Casters rely on **Mana Crystals** as external batteries, not internal mana pools.
- **Armor Interference:** Heavy armor dampens crystal resonance, draining/shattering crystals.
- **Hybrid Viability:** "Battlemages" are possible but will **never** achieve Rank 100 in both disciplines.

### 4.6 Item Durability — Dual Status System
- **Damage Status:** Current condition (0-100%). Can be repaired.
- **Durability Status:** Maximum potential (100% → degrades with each repair). Cannot be restored.
- Repairing an item **always degrades** Durability Status, leading to eventual obsolescence.
- **Equipped gear is NEVER dropped on death.** It takes a Durability Hit instead.

### 4.7 Death Consequences
- **Soul Debt:** Death incurs XP Debt, pushing skills toward the "Rusty" state.
- Consequences vary by region Security Tier (Sanctuary, Settled, Wilderness, Dungeons).
- **Inventory items** may drop on death (region-dependent chance).
- **Resurrection** requires infrastructure (Primal Shrines or Guild Town Obelisks).
- Obelisks must be powered by Mana Crystals and maintained.

### 4.8 Economy Invariants
- **Player-driven, decentralized.** No global auction house.
- **Everything breaks.** Items and buildings require constant maintenance, ensuring demand.
- **Finite resources.** Resource nodes are depleted permanently, driving migration and conflict.
- **Economy of Distance.** Regional scarcity is only meaningful if transport is challenging.
- **Guild-created currencies.** No universal currency.

### 4.9 Building Rules
- **Evolution:** Modular/socket-based early game → Voxel tools + structural integrity physics later.
- **Blueprints:** Planning system for group construction.
- **Skill Requirements:** Advanced structures require high Architecture/Engineering ranks.
- **Territory Shields:** Require Mana Crystals for power.

### 4.10 Automation & Logic Rules
- All automated systems **must be powered** by Mana Crystals or Fuel.
- Complexity limits per structure/settlement to control server performance.
- Logic Gems have tick rate limits and power requirements (no infinite automation).

### 4.11 Portal Rules
- Portals are **Guild-built**, not system-provided.
- Construction requires rare "Portal Shards" and massive resource sinks.
- Portals require constant mineral supplies and NPC "Portal Wardens" who must be fed.
- Portal degradation: Instability → Teleport Sickness → Shutdown → Fracture (permanent loss).

### 4.12 Maritime Rules
- **Bulk transport** is sea-only. Portals are for people and high-value artifacts.
- Ships above Tier 1 require a **Crystal Core** for automation and engines.
- **Scale Dilation:** 1:10 coordinate scale in Deep Water for long voyages.
- **Coastal Snap:** Within 10 miles of land, the game snaps to 1:1 scale.
- Voyages require self-sustainability (food, maintenance, crew).
- Shipwrecks create temporary POIs for salvage.

### 4.13 Resource Rules
- Resources are **not** highlighted with HUD markers. Players must "read the land."
- Extraction is **one-way** — finite resource nodes drive economic cycles.
- Deep Mining requires guild infrastructure (Shaft Elevators).
- Abandoned mines become dynamic dungeons ("Infestation" trigger).

### 4.14 NPC Standing Rules
- Standing Tiers range from Arch-Nemesis to Exalted.
- **Global vs. Regional** standing are tracked separately.
- NPC Hirelings require standing thresholds, food, and wages.
- If standing drops, hirelings may "Strike" or sabotage.

### 4.15 Chunk-Based World Architecture
- **Invariant:** The world is Earth-sized (~510 million km²) and partitioned into **Seamless Hexagonal Chunks**.
- **No Global Load:** The server only simulates active chunks (those with players or active NPC tasks).
- **Consistency:** Generation is deterministic; all clients and the server must arrive at the same terrain results for a given chunk coordinate.
- **Persistence:** Any change to a chunk (player-built structure, resource depletion) MUST be persisted to the database and override the procedural base layer.

---

## 5. File & Folder Conventions

| Folder | Purpose |
|--------|---------|
| `Documents/Design/` | Game design bible (numbered chapters). Authoritative for mechanics. |
| `Documents/Technical/` | Technical architecture and system design. |
| `Documents/Workflows/` | Step-by-step editor/implementation guides. |
| `Source/FaldoranPrimeMMO/Public/` | C++ header files (API surface). |
| `Source/FaldoranPrimeMMO/Private/` | C++ implementation files. |
| `Content/` | Unreal assets (Blueprints, meshes, materials, maps). |

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
