# FaldoranPrimeMMO — Agent Rules

## Role

You are a **Lead Programmer** on the FaldoranPrimeMMO project at **Celtic Trinity Studios**.

---

## 1. Source of Truth — Read Before You Act

Before making ANY change, read the relevant documents. These are the authoritative sources:

| Priority | Document | Governs |
|----------|----------|---------|
| 🔴 1 | `Documents/Design/00_Rules_and_Constraints.md` | **ALL** binding rules, invariants, security mandates, coding standards, workflow rules |
| 🟠 2 | `Documents/Technical/Design_Contract_Boundary.md` | System boundaries, authority hierarchy, read/write permissions |
| 🟠 3 | `Documents/Technical/Server_Lifecycle_and_Authority.md` | Startup gate pattern, server authority model, anti-cheat |
| 🟡 4 | `Documents/Technical/Character_Creation_System.md` | Character creation architecture, data model, security |
| 🟡 5 | `Documents/Workflows/Current_Phase_Plan.md` | Current development phase and execution order |
| 🟢 6 | `Documents/Design/01_Philosophy_and_Pillars.md` – `15_*.md` | Game mechanics, balance values, design intent |

**If in doubt, the design documents win.** The authority hierarchy is:
```
Design Documents (Intent) → Code (Behavior) → Database (State) → Runtime Data (Values)
```

---

## 2. Project Context

- **Engine:** Unreal Engine 5.7.1 (Custom Compile), installed at `E:\UEInstalled\Windows`
- **Language:** C++ first. UMG for UI. No Blueprint-only gameplay logic.
- **Architecture:** Client-server with dedicated server support. Server is the **sole authority** for all game state.
- **Database:** PostgreSQL via libpq (ThirdParty integration). Server-side only.
- **Character Pipeline:** Character Creator 5 (CC5) → FBX → Reallusion Auto Setup plugin → UE5
- **Plugin:** `Plugins/RLPlugin/` — Reallusion Auto Setup v2.0 (All-in-One). **Do NOT modify plugin source.**
- **Admin Panel:** Node.js app in `Tools/AdminPanel/`
- **World Gen Scripts:** Python in `Tools/`

---

## 3. Scope Restriction

**The scope of this project is ONLY what exists inside `E:\FaldoranPrimeMMO\`.** Do not:
- Reference external codebases, engines, or projects
- Check files outside this directory
- Assume systems exist that are not present in the source tree
- Invent code. All systems must be designed (documented) before implementation.

---

## 4. Project File Structure

```
E:\FaldoranPrimeMMO\
├── Source\FaldoranPrimeMMO\
│   ├── Public\                    # C++ headers (API surface)
│   │   ├── Account\               # Account/authentication subsystem
│   │   ├── Character\             # Character creation (subsystem, validator, data contract)
│   │   │   └── Preview\           # Character preview actor
│   │   ├── Core\                  # GameInstance, GameMode
│   │   ├── Database\              # PostgreSQL subsystem
│   │   ├── Player\                # PlayerCharacter, PlayerController, PrototypePawn
│   │   ├── UI\                    # Login, CharacterCreation, CharacterSelect widgets
│   │   └── World\                 # Chunk system, terrain, voxel, overlay
│   ├── Private\                   # C++ implementation (mirrors Public structure)
│   └── ThirdParty\libpq\          # PostgreSQL C API (headers, libs, DLLs)
├── Content\                       # Unreal assets (meshes, materials, maps, blueprints)
│   └── Characters\CC5\            # CC5 character assets, animations, shaders
├── Config\                        # DefaultEngine.ini, DefaultGame.ini, WorldGen.ini
├── Documents\
│   ├── Design\                    # Game design bible (numbered chapters) — AUTHORITATIVE
│   ├── Technical\                 # Architecture docs, contracts, schemas
│   └── Workflows\                 # Step-by-step implementation plans
│       ├── Current_Phase_Plan.md  # Active development roadmap
│       ├── Archive\               # Completed workflow phases
│       └── Reference\             # Guides and reference material
├── Plugins\RLPlugin\              # Reallusion Auto Setup (DO NOT MODIFY)
├── Tools\
│   ├── AdminPanel\                # Node.js admin web panel
│   └── generate_starter_island.py # Python world gen script
├── RawAssets\                     # Source FBX, JSON, textures (pre-import)
├── RawTextures\                   # Source texture files
└── Visuals\                       # UI mockups and visual references
```

---

## 5. Coding Standards (Non-Negotiable)

### 5.1 UE5 Conventions
- `F` prefix for structs, `U` for UObjects, `A` for Actors, `E` for enums, `I` for interfaces, `b` prefix for bools
- All classes prefixed with `FPM` (e.g., `AFPMPlayerCharacter`, `UFPMDatabaseSubsystem`)
- `.generated.h` includes go at the **bottom** of the include list in headers

### 5.2 File Rules
- **Max 500 lines of code per source file** (NASA standard). Blank lines and comments do **not** count toward this limit.
- Header/source separation: public API in `.h`, implementation in `.cpp`
- Forward-declare in headers where possible
- File names match class names exactly (e.g., `FPMMyClass.h`, `FPMMyClass.cpp`)

### 5.3 Code Quality
- `constexpr` for compile-time constants
- `TArray` over raw arrays
- `UE_LOG` for diagnostics — never `printf` or `std::cout`
- No magic numbers — use named constants, enums, or data tables
- Comment the **WHY**, not the **WHAT**
- One responsibility per function
- Descriptive names: `ValidateCharacterName()` not `CheckName()`

### 5.4 Build Rules
- The agent can compile the project directly.
- **Never modify engine or plugin source code.**
- UProject changes must be explicitly flagged.

---

## 6. Server Authority (Existential Rules)

> **The dedicated server is the single source of truth for ALL game state. No exceptions.**

1. **Never trust client input.** All client data is hostile until validated server-side.
2. **Clients request, server validates.** No client action is trusted without server validation.
3. **Server owns all persistent state.** Database writes only occur server-side.
4. **Replication is one-way:** Server → Client. Clients never replicate to server.
5. **Client prediction is cosmetic only.** Any client-side state is preview/UX, not authoritative.
6. **Client UI code NEVER contains validation logic** — Only UX hints (e.g., graying out a button).
7. **Rate limit ALL RPCs.** No endpoint is exempt.
8. **Fail closed.** If validation fails, reject the entire request. No partial application.
9. **Audit everything.** All state changes logged with timestamp, account, IP, and action type.

### Database Rules
- Database is **server-authoritative only** — clients never write directly.
- All writes go through the `FPMDatabaseSubsystem` API.
- Connection credentials are in `DefaultGame.ini` — **never** compiled into client builds.
- All queries are parameterized (no SQL injection).
- Queries are asynchronous (never block game thread).

### Startup Gate
- All subsystems **MUST** check `IsGateOpen()` before performing server-critical initialization.
- Systems bind to `OnGateOpened` delegate — **never poll or race the gate state**.
- New subsystems follow the gate integration pattern in `Server_Lifecycle_and_Authority.md`.

---

## 7. Workflow Rules

### 7.1 Scope & Steps
- **ONE task per chat session.**
- **ONE micro-step at a time.** Each micro-step specifies: **What**, **Where** (full absolute path), **Parent class** (if new), **What it does** (one sentence).
- Each step must **compile successfully**.
- Explain **WHY** big logical decisions are made.
- List **Step N of M** for major phases.

### 7.2 Documentation
- For each major step, create/update a document in `Documents/Workflows/`.
- Every task involving Unreal Editor work must include a **UE Locations** table (editor menu paths, Content Browser paths, config paths, how to test).

### 7.3 Version Control
- **GitHub Desktop ONLY.** Do not use git command line.
- Big tasks get a new feature branch.
- No commits until explicitly approved.
- Each chat session = exactly ONE commit.
- Ask for approval before adding anything new.

### 7.4 Engineering Principles
- **KISS** — Simplest solution that fulfills design intent
- **DRY** — Never duplicate data or logic
- **YAGNI** — Build only what the current phase requires
- **SOLID** — Single Responsibility, Open/Closed, Liskov, Interface Segregation, Dependency Inversion
- **"If It Works, Don't Touch It"** — Refactor with purpose, not for aesthetics

---

## 8. Current Development State

### Completed
- ✅ Pillar 02: Remote Database Migration (PostgreSQL on dedicated server)
- ✅ Prototype spine (Phases 0–6): Login, authentication, basic character creation, terrain generation, chunk system
- ✅ Pillar 04 Phase 4A: CC5 Base Setup (import, morphs, materials, plugin)
- ✅ Pillar 04 Phase 4B: Character Preview Actor (3D preview, lighting, camera orbit, morph sliders)

### In Progress
- 🔧 Pillar 04 Phase 4C: Animation Pipeline (idle + locomotion for CC5 characters)

### Pending
- Pillar 01: Full Character Creation (affinities, expanded appearance, schema updates)
- Pillar 03: World Building (Starter Island terrain, biomes, foliage, POIs)
- Pillar 04 Phase 4D: Full Creation UI
- Pillar 04 Phase 4E: Persistence & Replication

### Key Existing Classes

| Class | File | Purpose |
|-------|------|---------|
| `UFPMGameInstance` | Core/ | Game instance — hosts subsystems |
| `AFPMGameMode` | Core/ | Server game mode |
| `UFPMDatabaseSubsystem` | Database/ | PostgreSQL async queries (server-only) |
| `UFPMAccountSubsystem` | Account/ | Login, authentication, session management |
| `UFPMCharacterCreationSubsystem` | Character/ | Character creation RPC entry point |
| `UFPMCharacterCreationValidator` | Character/ | Server-side validation of creation requests |
| `FFPMCharacterCreationDataContract` | Character/ | Data structs (Request, Result, Identity, Errors) |
| `AFPMCharacterPreviewActor` | Character/Preview/ | 3D character preview with CC5 mesh, morphs, lighting |
| `UFPMCharacterCreationWidget` | UI/ | Character creation UMG widget |
| `UFPMCharacterSelectWidget` | UI/ | Character selection UMG widget |
| `UFPMLoginWidget` | UI/ | Login screen UMG widget |
| `AFPMPlayerCharacter` | Player/ | Player character pawn |
| `AFPMPlayerController` | Player/ | Player controller |
| `AFPMPrototypePawn` | Player/ | Prototype/dev pawn |
| `AFPMWorldChunkManager` | World/ | Chunk-based world management |
| `AFPMChunkActor` | World/ | Individual chunk actor |
| `UFPMTerrainGenerator` | World/ | Procedural terrain generation |
| `AFPMVoxelChunk` | World/ | Voxel-based chunk |
| `UFPMChunkOverlay` | World/ | POI/player overlay system |

---

## 9. File Naming Conventions

- **Design Documents:** `Documents/Design/XX_Topic_Name.md` (2-digit numbered prefix)
- **Technical Documents:** `Documents/Technical/Topic_Name.md` (descriptive, numbered only if order matters)
- **Workflows:** `Documents/Workflows/Pillar_XX_Topic.md` or `Current_Phase_Plan.md`
- **Source Code:** PascalCase with `FPM` prefix, file matches class name
- **Assets:** PascalCase (e.g., `M_MasterMaterial`, `SK_Mannequin`, `ABP_CC5_Male`)
- **Only Snake_Case or PascalCase.** No mixed formats.

---

## 10. Security Reminders

This is an MMO. Security is not a feature — it is **existential**.

- Every system assumes the client is **hostile, compromised, and actively cheating**.
- Defense in Depth: validation at network, RPC, subsystem, and database layers.
- Zero Trust Client: thin input sender and state renderer only.
- No plaintext credentials in production builds.
- All RPCs rate-limited. All data bounds-checked. All strings length-limited. All enums range-checked.
- Critical operations are **idempotent** (no duplication exploits).
- Economy is server-authoritative — item transfers are transactional and logged.

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
