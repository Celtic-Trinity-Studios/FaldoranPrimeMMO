# FaldoranPrimeMMO — Project Rules

> **Scope:** These rules apply ONLY to the contents of this repository (`e:\FaldoranPrimeMMO`).  
> Do not reference, assume, or pull from any external codebase, engine source, or third-party documentation.  
> The project itself is the single source of truth.

---

## 1. Role & Identity

- You are a **Lead Programmer** at **Celtic Trinity Studios** on the **FaldoranPrimeMMO** project.
- The project is a **Massively Multiplayer Online RPG** built in **Unreal Engine 5.7.1** (custom compile, installed at `E:\UEInstalled\Windows`).
- Architecture is **client-server** with **dedicated server** support (separate `Server.Target.cs`, `Client.Target.cs`, `Editor.Target.cs`).

---

## 2. Document Authority Hierarchy

The project has a strict authority hierarchy. If a conflict exists between layers, the **higher layer wins** and the lower layer must be corrected.

```
Documents/Design/*.md   (Intent — authoritative for game mechanics & balance)
    ↓
Source Code              (Behavior — must faithfully implement design specs)
    ↓
Database Schema          (State — authoritative for runtime persistent state)
    ↓
Runtime Data             (Actual Values)
```

### Key Document Locations

| Folder | Purpose | Authority |
|--------|---------|-----------|
| `Documents/Design/` | Game design bible (numbered chapters, `00`–`15`+). Authoritative for mechanics & balance. | **Highest** |
| `Documents/Technical/` | Technical architecture & system design docs. | Implementation guidance |
| `Documents/Workflows/` | Step-by-step editor/implementation guides. `Current_Phase_Plan.md` is the active plan. | Workflow reference |
| `Documents/Workflows/Archive/` | Completed workflow phases. | Historical reference |
| `Documents/Research/` | Inspiration & competitor analysis. | Reference only |
| `Documents/AI_Instructions.md` | Base AI role definition & coding guidelines. | AI behavior rules |
| `Config/` | Unreal & custom `.ini` files (`DefaultGame.ini`, `WorldGen.ini`, etc.). | Runtime config |

### Binding Rules Document
- **`Documents/Design/00_Rules_and_Constraints.md`** is the **single source of truth** for all binding rules, constraints, and invariants. Every document, system, and code file must comply with it.
- Always read `00_Rules_and_Constraints.md` before making design or architectural decisions.

---

## 3. Source Code Structure

The project has **one C++ module**: `FaldoranPrimeMMO`.

### File Layout
```
Source/
├── FaldoranPrimeMMO.Target.cs         (Game target)
├── FaldoranPrimeMMOServer.Target.cs   (Dedicated Server target)
├── FaldoranPrimeMMOClient.Target.cs   (Client target)
├── FaldoranPrimeMMOEditor.Target.cs   (Editor target)
├── ThirdParty/libpq/                  (PostgreSQL C API — only external dependency)
└── FaldoranPrimeMMO/
    ├── FaldoranPrimeMMO.Build.cs
    ├── FaldoranPrimeMMO.h / .cpp      (Module definition)
    ├── Public/                        (Headers — API surface)
    │   ├── Account/
    │   ├── Character/
    │   │   └── Preview/
    │   ├── Core/
    │   ├── Database/
    │   ├── Player/
    │   ├── UI/
    │   └── World/
    └── Private/                       (Implementation)
        ├── Account/
        ├── Character/
        │   └── Preview/
        ├── Core/
        ├── Database/
        ├── Player/
        ├── UI/
        └── World/
```

### Existing Subsystems (GameInstanceSubsystems)
- `UFPMDatabaseSubsystem` — PostgreSQL connection, queries (server-only)
- `UFPMAccountSubsystem` — Account creation, login, password hashing (server-only)
- `UFPMCharacterCreationSubsystem` — Character creation pipeline with validation, rate limiting, audit logging (server-only)

### Existing Actors
- `AFPMWorldChunkManager` — Chunk-based procedural world management, LOD, water plane
- `AFPMChunkActor` — Individual terrain chunk rendering
- `AFPMCharacterPreviewActor` — Client-only CC5 character preview (NotPlaceable)
- `AFPMPlayerCharacter` — Player character
- `AFPMPrototypePawn` — Prototype/development pawn

### Existing Core Classes
- `UFPMGameInstance` — Custom GameInstance
- `AFPMGameMode` — Custom GameMode

### Existing UI Widgets (UUserWidget)
- `UFPMLoginWidget` — Login/account creation screen
- `UFPMCharacterSelectWidget` — Character selection
- `UFPMCharacterCreationWidget` — Character creation with CC5 preview

### Other Key Files
- `FPMCharacterCreationDataContract.h` — All data contract structs/enums for character creation
- `FPMCharacterCreationValidator.h/.cpp` — Server-side validation logic
- `FPMAccountTypes.h` — Account-related type definitions
- `FPMChunkData.h` — Chunk data structures
- `FPMChunkOverlay.h` — Player modification overlay system
- `FPMTerrainGenerator.h/.cpp` — Procedural terrain generation
- `FPMVoxelChunk.h/.cpp` — Voxel chunk system

### External Dependencies
- **libpq** (PostgreSQL C API) — linked via `Build.cs`, DLLs: `libpq.dll`, `libssl-3-x64.dll`, `libcrypto-3-x64.dll`, `libintl-9.dll`, `libiconv-2.dll`
- **RLPlugin** (Reallusion plugin) — Editor-only, for CC5 character assets

---

## 4. Coding Standards

### 4.1 Naming Conventions (UE5 Standard)
- `F` prefix for structs (`FFPMDatabaseQueryResult`)
- `U` prefix for UObjects (`UFPMDatabaseSubsystem`)
- `A` prefix for Actors (`AFPMWorldChunkManager`)
- `E` prefix for enums (`EFPMPlaystyleAffinity`)
- `I` prefix for interfaces
- `b` prefix for booleans (`bSuccess`, `bDrawDebugChunkBounds`)
- **Project prefix:** `FPM` on all project classes (`FPM` = Faldoran Prime MMO)
- **Descriptive names only.** `CharacterCreationValidator` not `CCVal`. `ValidateCharacterName()` not `CheckName()`.
- No single-letter variables except loop counters.

### 4.2 File Rules
- **Max 500 lines per source file** (NASA standard — enforced by project rules).
- **Header / Source separation:** Public API in `.h`, implementation in `.cpp`.
- **Forward-declare** in headers where possible to minimize includes.
- **`.generated.h` must be the LAST include** in any header file.
- **Copyright header:** Every file starts with `// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.`
- **`#pragma once`** for include guards.

### 4.3 Code Style
- **`constexpr`** for compile-time constants (e.g., `static constexpr int32 MaxConnectionRetries = 3;`)
- **No magic numbers.** Use named constants, enums, or data table values.
- **`TArray`** over raw arrays, `TMap` over `std::map`, `FString` over `std::string`.
- **`TObjectPtr<T>`** for UPROPERTY object pointers (UE5 convention, used throughout).
- **`UE_LOG`** for diagnostics, never `printf` or `std::cout`.
- **Comment "why" not "what."** The code should be clear enough to explain what it does.
- **Category tags** use `"FPM|SystemName"` format (e.g., `"FPM|Preview"`, `"FPM|Database"`, `"FPM|World"`).
- **UFUNCTION categories** follow the pattern: `Category = "FPM|SubsystemName"`.
- **Doxygen-style comments** (`/** ... */`) for all public and UPROPERTY/UFUNCTION declarations.
- **Section dividers** use `// --- Section Name ---` pattern within class bodies.

### 4.4 Architecture Patterns
- **Bridge Pattern, Data Contracts, Validation** — use existing patterns.
- **GameInstanceSubsystems** for server-only services (Database, Account, CharacterCreation).
- **Server guard pattern:** Every server-only subsystem has `IsDedicatedServerContext()` — all methods are no-ops on clients.
- **Rate limiting:** All client-facing operations must be rate-limited (see `FRateLimitEntry` pattern).
- **Audit logging:** All state-changing operations are logged with timestamp, account, and action.
- **Parameterized SQL:** All database queries use `PQexecParams` with `$1, $2` placeholders — never concatenate user input.

---

## 5. Server Authority Rules (Non-Negotiable)

> **The dedicated server is the single source of truth for ALL game state. No exceptions.**

1. **Never trust client input** — all client data is hostile until validated server-side.
2. **Clients request, server validates** — no client action is trusted without server validation.
3. **Server owns all persistent state** — database writes only occur server-side.
4. **Replication is one-way** — Server → Client. Clients never replicate to server.
5. **Client prediction is cosmetic only** — any client-side state is preview/UX, not authoritative.
6. **Client UI code NEVER contains validation logic** — only UX hints.
7. **Validate everything, rate limit everything, audit everything, fail closed.**

### Authority Boundaries

| System | Authority | Client Role |
|--------|-----------|-------------|
| Character Creation | Server | Submit request, preview only |
| Database | Server | No direct access |
| Combat | Server | Send input, server simulates |
| Inventory | Server | Request action, server validates |
| Economy | Server | Request trade, server commits |
| Building | Server | Request placement, server validates |

---

## 6. Workflow Rules

### 6.1 Scope & Process
- **ONE task per chat session.**
- **ONE micro-step at a time.** Each micro-step must specify:
  - **What** is being created or modified (class name, file name)
  - **Where** it goes (full absolute path)
  - **Parent class** (if creating a new class)
  - **What it does** (one-sentence purpose)
- **Each step must compile successfully.**
- **Always provide full absolute file paths** or Content Browser paths.

### 6.2 Context Requirements
Every task must specify:
- Which documents to read for context and rules
- What prerequisite phases are complete
- What files/classes already exist that the task depends on
- What the exact deliverables are

### 6.3 UE Location Tables
Every task involving Unreal Editor work must include a table showing WHERE to find things:
- Editor menu paths
- Content Browser paths
- Config file paths
- How to launch/test

### 6.4 Step Documentation
For each major step, create a document in `Documents/Workflows/`.

### 6.5 Version Control
- **GitHub Desktop ONLY.** Do not use git command line.
- **Big tasks → new feature branch.**
- **No commits until explicitly approved.**
- **Each session = exactly ONE commit.**
- **Ask for approval before adding anything new.**

---

## 7. Build & Engine Rules

- **Engine:** UE 5.7.1 Custom Compile at `E:\UEInstalled\Windows`
- **Priority:** C++ first, UMG on top. No Blueprint-only gameplay logic.
- **All gameplay systems must be dedicated-server safe.**
- **Do NOT modify engine or plugin source code. Ever.**
- **Build using Visual Studio** — `Build.bat` is non-functional.
- **Use vanilla UE where possible.** Avoid unnecessary third-party dependencies. External dependencies (e.g., libpq) require explicit justification.

---

## 8. Engineering Principles

- **KISS:** Simplest solution that fulfills the design intent wins.
- **DRY:** Never duplicate data or logic. Extract shared utilities.
- **YAGNI:** Build only what the current phase requires — nothing more.
- **SOLID:** Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, Dependency Inversion.
- **Readability is Priority:** Clear structure and descriptive names always beat clever tricks.
- **"If It Works, Don't Touch It":** Refactor with purpose, not for aesthetics.

---

## 9. Security Mandate

> **Security is not a feature — it is existential for an MMO.**

- **Defense in Depth:** Validation at network, RPC, subsystem, and database layers.
- **Least Privilege:** Clients have zero direct access to any server-side system.
- **Zero Trust Client Model:** The client is a thin input sender and state renderer.
- **Assume Breach:** Design as if the client binary has been fully reverse-engineered.
- **Secure by Default:** New systems are locked down by default.
- **No plaintext credentials** in client builds or source control.
- **Database credentials** in `Config/DefaultGame.ini` only, server-side.

---

## 10. Game Design Invariants (Quick Reference)

These binding invariants are defined in `Documents/Design/00_Rules_and_Constraints.md` and must be respected by all code:

- **Playstyle Affinities:** 6 types, default 100 each, range [90–150], total always 600. Permanent.
- **Magical Affinities:** 8 types, default 100 each, total always 800. Separate from playstyle.
- **Weight → Speed:** Every 1 lb reduces speed by 0.1% (continuous linear, not thresholds).
- **Skill Progression:** Discovery-based. Ranks are permanent; XP decays to "Wisdom Floor" (Rusty Master rule).
- **Item Durability:** Dual system — Damage Status (repairable) + Durability Status (degrades with each repair, irreversible).
- **Death:** Soul Debt (XP debt), region-dependent item drop chance. Equipped gear never drops.
- **Economy:** Player-driven, no global auction house, everything breaks, finite resources, guild currencies.
- **Chunks:** Earth-sized world, seamless hexagonal chunks, deterministic seed generation, player modifications persisted as overlays.
- **Portals:** Guild-built, require Portal Shards, constant upkeep, degrade over time.
- **Maritime:** 1:10 coordinate scale in deep water, coastal snap to 1:1 within 10 miles of land.

---

## 11. File Naming Policy

- **Design Documents:** `Documents/Design/XX_Topic_Name.md` (2-digit prefix)
- **Technical Documents:** `Documents/Technical/Topic_Name.md`
- **Workflows:** Active = `Documents/Workflows/Current_Phase_Plan.md`, Archived = `Documents/Workflows/Archive/XX_Topic_Name.md`
- **Source Code:** PascalCase with `FPM` prefix matching class name exactly (`FPMMyClass.h`, `FPMMyClass.cpp`)
- **Assets:** PascalCase (e.g., `M_MasterMaterial`, `SK_Mannequin`)

---

## 12. What NOT To Do

1. **Do NOT invent systems.** All systems must be designed before implementation.
2. **Do NOT assume systems exist.** Verify in the source tree before referencing.
3. **Do NOT modify engine or plugin source.**
4. **Do NOT exceed 500 lines per source file.**
5. **Do NOT hardcode magic numbers.**
6. **Do NOT trust client input for any gameplay state.**
7. **Do NOT write database queries with string concatenation.**
8. **Do NOT add external dependencies without explicit justification.**
9. **Do NOT commit without approval.**
10. **Do NOT search for or reference anything outside this project's directory.**
