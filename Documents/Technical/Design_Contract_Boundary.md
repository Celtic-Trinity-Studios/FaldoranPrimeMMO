# Design Contract Boundary

**Purpose:** This document formalizes the authority and access patterns for design data across the Faldoran Prime MMO architecture. It establishes which systems are **authoritative** (can write/modify) versus **read-only consumers** of design data.

> **Binding Rules:** Core design contract principles extracted to [00 Rules and Constraints — §3](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md). This document covers the **detailed system boundaries, subsystem contracts, and implementation specifics**.

---

## 1. Core Principles

### 1.1 Single Source of Truth
- **Design Documents** (`Documents/Design/*.md`) are the **authoritative source** for game mechanics, balance values, and system specifications.
- **Database Schema** is the **authoritative source** for runtime persistent state (player data, world state, guild data).
- **Code Implementation** is the **authoritative source** for actual runtime behavior and must faithfully implement design specifications.

### 1.2 Authority Hierarchy
```
Design Documents (Intent)
    ↓
Code Implementation (Behavior)
    ↓
Database Schema (State)
    ↓
Runtime Data (Actual Values)
```

### 1.3 Read vs. Write Authority
- **Write Authority:** The system that can modify the canonical version of data.
- **Read Authority:** The system that can consume data but must not modify the source.

---

## 2. System Boundaries

### 2.1 Design System (Documents/Design/)

**Authority:** Write (Authoritative for game design)

**Responsibilities:**
- Define game mechanics, progression curves, balance values
- Specify system interactions and dependencies
- Document intended player experiences

**Allowed to Read:**
- All systems (for implementation guidance)

**Allowed to Write:**
- Game designers (human authority)
- Design tools (if implemented)

**NOT Allowed to Write:**
- Runtime code
- Database systems
- Client applications

**Contract:**
- Design documents are **version-controlled** and changes require explicit commits
- Breaking changes to design require corresponding code updates
- Design values should be **parameterized** in code (not hardcoded) where feasible

---

### 2.2 Database Subsystem (FPMDatabaseSubsystem)

**Authority:** Write (Authoritative for persistent runtime state)

**Responsibilities:**
- Store and retrieve player persistent data (inventory, skills, reputation)
- Store and retrieve world persistent data (structures, guild territories)
- Maintain transactional integrity for state changes

**Allowed to Read:**
- Design constants (for validation, e.g., max skill level)
- Configuration files (connection strings, table schemas)

**Allowed to Write:**
- Runtime player state
- Runtime world state
- Audit logs and telemetry

**NOT Allowed to Write:**
- Design documents
- Game balance values
- Core mechanic definitions

**Contract:**
- Database schema must **mirror** design intent but is optimized for performance
- The database is **server-authoritative** only (clients never write directly)
- All writes must go through the `FPMDatabaseSubsystem` API
- Schema migrations must be versioned and backward-compatible during rollout

---

### 2.3 Server Runtime (Dedicated Server)

**Authority:** Write (Authoritative for simulation and validation)

**Responsibilities:**
- Execute game logic as defined by design documents
- Validate client inputs against design rules
- Manage world simulation (NPC AI, resource spawning, combat resolution)
- Coordinate database writes for state persistence

**Allowed to Read:**
- Design documents (via config files or data tables)
- Database state (via FPMDatabaseSubsystem)
- Client inputs (for validation)

**Allowed to Write:**
- Database state (via FPMDatabaseSubsystem)
- Server logs and telemetry
- Temporary simulation state (in-memory)

**NOT Allowed to Write:**
- Design documents
- Client-side state directly

**Contract:**
- Server is the **sole authority** for gameplay state
- All design values should be loaded from **Data Tables** or **Config Files** (not hardcoded)
- Server must validate all client requests against design constraints
- Server binds to database subsystem only on dedicated server instances (not in PIE/Editor)

---

### 2.4 Client Runtime (Player Client)

**Authority:** Read-only (No authoritative power)

**Responsibilities:**
- Display game state to player
- Send player inputs to server
- Perform client-side prediction for responsiveness
- Cache read-only design data for UI/tooltips

**Allowed to Read:**
- Design data (via replicated data tables or config files)
- Server-replicated game state
- Local player preferences

**Allowed to Write:**
- Local UI state (non-authoritative)
- Client-side prediction state (non-authoritative)
- Local player settings/preferences

**NOT Allowed to Write:**
- Database state
- Server game state
- Design documents

**Contract:**
- Client is **never authoritative** for gameplay state
- Client predictions must be validated by server
- Client can cache design data but must defer to server for authoritative values
- Client must gracefully handle server corrections (rollback prediction)

---

### 2.5 Data Tables / Config System

**Authority:** Read-only at runtime (Write authority during development)

**Responsibilities:**
- Provide structured access to design values (skill costs, item stats, etc.)
- Bridge design documents to runtime code
- Enable hot-reloading of balance values without code changes

**Allowed to Read:**
- Design documents (during data table authoring)

**Allowed to Write (Development Only):**
- Data table assets (.uasset files)
- Config files (.ini files)

**Allowed to Write (Runtime):**
- Nothing (read-only at runtime)

**Contract:**
- Data tables are **derived** from design documents
- Data tables are version-controlled alongside code
- Changes to data tables should reference corresponding design document changes
- Server and client load identical data tables (server validates, client displays)

---

## 3. Specific Subsystem Contracts

### 3.1 FPMDatabaseSubsystem

**Read Authority:**
- Configuration files (`DefaultEngine.ini`, `DefaultGame.ini`)
- Design constants (for validation, e.g., max inventory slots)

**Write Authority:**
- PostgreSQL database (player state, world state)
- Server logs (connection status, query performance)

**Boundary Rules:**
- Only instantiated on **dedicated servers** (not PIE, not client)
- Auto-connect gated by `FPMStartupGateSubsystem` to ensure proper initialization order
- All database queries are **asynchronous** to prevent blocking game thread
- Connection credentials must never be exposed to clients

**API Contract:**
```cpp
// Read Operations (any server thread can call)
void QueryPlayerData(FGuid PlayerUID, FOnQueryComplete Callback);
void QueryGuildTerritory(FVector Location, FOnQueryComplete Callback);

// Write Operations (validated by caller, subsystem ensures transactional integrity)
void SavePlayerData(FGuid PlayerUID, const FPlayerStateData& Data);
void UpdateGuildTerritory(FGuid GuildUID, const FTerritoryData& Data);

// Subsystem does NOT expose raw SQL or connection objects
// All queries are parameterized to prevent injection
```

---

### 3.2 Procedural Generation System

**Read Authority:**
- Shared mathematical seed (from server config)
- Design documents (biome definitions, resource distribution)
- POI database (handcrafted locations)

**Write Authority:**
- In-memory terrain cache (ephemeral)
- Client-side visual representation (non-authoritative)

**Boundary Rules:**
- Procedural generation is **deterministic** (same seed = same output)
- Server and client generate identical terrain for validation
- Player-built structures **override** procedural data (stored in database)
- Procedural system never writes to database (only reads POI overlays)

---

### 3.3 Reputation & Soul Debt System

**Read Authority:**
- Design documents (reputation thresholds, soul debt formulas)
- Database (current player reputation values)

**Write Authority:**
- Database (via FPMDatabaseSubsystem)
- Server logs (reputation change events)

**Boundary Rules:**
- Reputation calculations are **server-authoritative**
- Design documents define the formulas (e.g., "killing NPC = -10 reputation")
- Code implements the formulas
- Database stores the current values
- Regional reputation is **cached at tile level** and synced to global DB periodically
- Clients receive reputation values via replication (read-only)

---

### 3.4 Crafting & Template System

**Read Authority:**
- Design documents (crafting recipes, material properties)
- Data tables (recipe definitions, item templates)
- Database (player-discovered recipes, custom templates)

**Write Authority:**
- Database (player-created templates, recipe unlocks)

**Boundary Rules:**
- Base recipes are defined in **data tables** (derived from design docs)
- Player-created templates are **validated** against design constraints (e.g., max stat budget)
- Server validates all crafting operations before committing to database
- Clients display recipes but cannot modify them

---

## 4. Cross-Boundary Communication

### 4.1 Design → Code
- **Mechanism:** Data tables, config files, design document references in code comments
- **Validation:** Code reviews ensure implementation matches design intent
- **Versioning:** Design changes are tracked in git; breaking changes require code updates

### 4.2 Code → Database
- **Mechanism:** FPMDatabaseSubsystem API (async queries, parameterized statements)
- **Validation:** Schema migrations are versioned; code must handle schema evolution
- **Security:** Connection credentials stored server-side only; clients never access DB directly

### 4.3 Server → Client
- **Mechanism:** Unreal replication system (replicated properties, RPCs)
- **Validation:** Server validates all client inputs before applying state changes
- **Security:** Client predictions are non-authoritative; server can override at any time

### 4.4 Database → Design
- **Mechanism:** Analytics and telemetry (one-way, read-only)
- **Validation:** Designers review player behavior data to inform balance changes
- **Security:** Telemetry is anonymized and aggregated; no PII in design documents

---

## 5. Enforcement Mechanisms

### 5.1 Compile-Time Enforcement
- **Private Members:** Database connection objects are private; only subsystem can access
- **Interface Contracts:** Public APIs use strongly-typed structs (no raw SQL exposure)
- **Conditional Compilation:** Database subsystem only compiles on server builds

### 5.2 Runtime Enforcement
- **Startup Gates:** FPMDatabaseSubsystem only initializes on dedicated servers
- **Validation Layers:** Server validates all client inputs against design constraints
- **Audit Logging:** All database writes are logged with timestamp and initiating actor

### 5.3 Development Enforcement
- **Code Reviews:** All database schema changes require review
- **Design Reviews:** All design document changes require review
- **Testing:** Integration tests validate that code behavior matches design intent

---

## 6. Violation Scenarios & Resolutions

### 6.1 Client Attempts Direct Database Write
- **Prevention:** Database credentials never sent to client
- **Detection:** Network traffic monitoring (no client should connect to DB port)
- **Resolution:** Immediate disconnect; flag account for review

### 6.2 Code Hardcodes Design Values
- **Prevention:** Code review guidelines require data table usage
- **Detection:** Grep for magic numbers in gameplay code
- **Resolution:** Refactor to use data tables; update design document reference

### 6.3 Database Schema Diverges from Design
- **Prevention:** Schema migrations reference design document changes
- **Detection:** Integration tests compare DB state to design constraints
- **Resolution:** Update schema or design document to restore consistency

### 6.4 Server Trusts Client Input Without Validation
- **Prevention:** Code review guidelines require server-side validation
- **Detection:** Security audits and penetration testing
- **Resolution:** Add validation layer; log suspicious activity

---

## 7. Future Considerations

### 7.1 Design Data Versioning
- As the game evolves, design documents may need versioning (e.g., "Season 1 vs. Season 2 balance")
- Consider a `DesignVersion` field in data tables to support A/B testing or rollback

### 7.2 Hot-Reloading Design Data
- For live-ops, consider a system to reload data tables without server restart
- Requires careful handling of in-flight transactions and player state

### 7.3 Cross-Region Database Sharding
- If the game scales globally, database sharding by region may be necessary
- Design contract must specify how cross-region queries are handled (eventual consistency)

### 7.4 Modding Support
- If modding is supported, establish a "mod boundary" where custom content can override design data
- Mods should never have write access to the authoritative database

---

## 8. Summary Table

| System                     | Design Docs | Database | Server State | Client State |
|----------------------------|-------------|----------|--------------|--------------|
| **Design Documents**       | Write       | Read     | Read         | Read         |
| **FPMDatabaseSubsystem**   | Read        | Write    | Read         | None         |
| **Dedicated Server**       | Read        | Write*   | Write        | Read         |
| **Player Client**          | Read        | None     | Read         | Write (local)|
| **Data Tables**            | Read        | None     | Read         | Read         |
| **Procedural Generation**  | Read        | Read     | Write (temp) | Write (temp) |

*Via FPMDatabaseSubsystem only

---

## 9. Conclusion

This design contract establishes clear boundaries for data authority in Faldoran Prime MMO:

1. **Design documents** own the intent and specifications
2. **Database** owns the persistent runtime state
3. **Server** owns the authoritative simulation
4. **Client** owns only local presentation and prediction

By respecting these boundaries, we ensure:
- **Consistency:** All systems agree on the source of truth
- **Security:** Clients cannot tamper with authoritative state
- **Maintainability:** Changes to one system have predictable impacts on others
- **Scalability:** Clear boundaries enable distributed architecture (tiled sharding)

All developers must adhere to these contracts. Violations should be treated as critical bugs and addressed immediately.
