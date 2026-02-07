# Character Creation System — Technical Reference

**Last Updated:** 2026-02-07  
**Status:** Foundation Complete — No code created yet. Ready for implementation.  
**Location:** `UFPMCharacterCreationSubsystem` (GameInstance subsystem)

---

## 1. Scope Boundary

### In Scope
- **Menu-based UI only** — Character creation happens in a dedicated UMG menu system
- **Client-side preview/cosmetics** — Visual representation of choices before submission
- **Server-side validation** — Authority validation of all creation requests
- **Request/response RPC flow** — No replication during character creation
- **Logging and audit trail** — Security logging of all creation attempts

### Explicitly Out of Scope
- ❌ In-world UI, gameplay systems, database persistence (future)
- ❌ Character spawning, replication logic, offline/local creation
- ❌ Client-side authority over final character state

---

## 2. Architecture

### Why GameInstance Subsystem
- Available on both client and server (RPC target)
- Lives for entire game session
- Separate from UI (UI calls subsystem, doesn't contain logic)
- Separate from gameplay (character creation ≠ character spawning)

### RPC Pattern (Not Replication)
Character creation uses **request/response RPC** because:
- Character doesn't exist in world during creation (nothing to replicate to)
- Preview state is client-only (no need to replicate)
- Replication happens AFTER character spawns (separate system)
- Prevents "UI shows X, server corrects to Y" chaos

### Lifecycle Integration
- Character creation subsystem respects startup gate (`UFPMServerStartupGateSubsystem`)
- Won't accept requests until server is ready (gate open)
- Binds to `OnGateOpened` delegate

### Architecture Diagram
```
CLIENT SIDE:
┌─────────────────────┐
│  UMG Character      │  ← User customizes appearance
│  Creation UI        │  ← Preview mesh updates (client-only)
└──────────┬──────────┘
           │ User clicks "Create Character"
           ▼
┌─────────────────────┐
│ FFPMCharacter       │  ← Builds request from UI state
│ CreationRequest     │  ← UNTRUSTED client input
└──────────┬──────────┘
           │ RPC Call
           ▼
SERVER SIDE:
┌─────────────────────────────────────────┐
│ UFPMCharacterCreationSubsystem          │
│ SubmitCharacterCreation(Request)        │
└──────────┬──────────────────────────────┘
           │ 1. Check startup gate
           │ 2. Get authenticated account ID
           │ 3. Check rate limiting
           ▼
┌─────────────────────────────────────────┐
│ UFPMCharacterCreationValidator          │
│ ValidateCreationRequest(...)            │
└──────────┬──────────────────────────────┘
           │ Validates:
           │ - Name (length, chars, profanity)
           │ - Appearance (indices, colors)
           │ - Affinities (distribution, totals)
           ▼
┌─────────────────────────────────────────┐
│ FFPMCharacterIdentity (Server-Owned)    │
│ + Audit Logging (all attempts)          │
└──────────┬──────────────────────────────┘
           │ TODO: Persist to database
           ▼
┌─────────────────────────────────────────┐
│ FFPMCharacterCreationResult             │
│ (Success + CharacterId OR Error)        │
└──────────┬──────────────────────────────┘
           │ RPC Response
           ▼
CLIENT SIDE:
┌─────────────────────┐
│  Displays success   │
│  screen or error    │
└─────────────────────┘
```

---

## 3. Data Model

### Character Identity Fields

#### Core Identity (Server-Assigned — NEVER from client)
| Field | Type | Notes |
|-------|------|-------|
| `AccountId` | `FGuid` | Derived from authenticated session |
| `CharacterId` | `FGuid` | Generated via `FGuid::NewGuid()` on server |
| `CreationTimestamp` | `FDateTime` | Server system time at creation |

#### Appearance (Client-Requested, Server-Validated)
| Field | Type | Validation |
|-------|------|------------|
| `CharacterName` | `FString` | 3-20 chars, alphanumeric + space/hyphen/apostrophe, profanity filter |
| `BodyType` | `uint8` | Valid index from design data (0-3) |
| `SkinTone` | `FLinearColor` | RGB values 0.0-1.0 |
| `HairStyle` | `uint8` | Valid index from design data |
| `HairColor` | `FLinearColor` | RGB values 0.0-1.0 |
| `FacialFeatures` | `TArray<uint8>` | Max 8 features, each valid index |
| `VoiceType` | `uint8` | Valid index from design data |

#### Starting Affinities (Client-Requested, Server-Validated)
| Field | Type | Validation |
|-------|------|------------|
| `PrimaryAffinity` | `EFPMAffinity` | Valid enum value |
| `SecondaryAffinity` | `EFPMAffinity` | Must differ from primary |
| `StartingAffinityPoints` | `TMap<EFPMAffinity, int32>` | Total = 10, primary 3-5, secondary 2-4, others 0-3 |

#### Affinity Enum
```cpp
UENUM(BlueprintType)
enum class EFPMAffinity : uint8
{
    None = 0,
    Fire, Water, Earth, Air,
    Light, Shadow, Nature, Arcane
};
```

### Data Structs (Planned)
- `FFPMCharacterCreationRequest` — Client request payload (UNTRUSTED)
- `FFPMCharacterCreationResult` — Server response (success or detailed errors)
- `EFPMCharacterCreationError` — Comprehensive error codes
- `FFPMCharacterIdentity` — Server-authoritative character identity

---

## 4. Replication Boundaries

Character creation has **three distinct data domains**:

| Domain | Data | Lifetime | Security |
|--------|------|----------|----------|
| **Client-Only (Preview)** | 3D mesh, camera orbit, UI state, temp appearance | Created/destroyed when UI opens/closes. NEVER sent to server. | Cosmetic only; cannot become "real" without server validation |
| **Server-Owned (Authority)** | `FFPMCharacterIdentity` — all validated fields | Created on server after validation. Persisted to DB (future). | Server is ONLY authority. Client receives `CharacterId` on success. |
| **Replicated (Post-Spawn)** | Character appearance after spawning in world | Begins when character spawns (separate system). | Server authority for replicated values. Clients read-only. |

### Field-Level Boundary Table

| Field | Client Preview | Server Authority | Replicated Post-Spawn |
|-------|:-:|:-:|:-:|
| CharacterName | ✅ Editable | ✅ Validated | ✅ Display name |
| BodyType | ✅ Preview mesh | ✅ Validated | ✅ Mesh selection |
| SkinTone | ✅ Preview material | ✅ Validated | ✅ Material param |
| HairStyle | ✅ Preview mesh | ✅ Validated | ✅ Mesh selection |
| HairColor | ✅ Preview material | ✅ Validated | ✅ Material param |
| FacialFeatures | ✅ Preview morphs | ✅ Validated | ✅ Morph targets |
| VoiceType | ✅ Preview audio | ✅ Validated | ✅ Audio selection |
| Affinities | ✅ Preview UI | ✅ Validated | ✅ VFX/stats |
| AccountId | ❌ Never | ✅ Server-only | ❌ Private |
| CharacterId | ❌ Not during creation | ✅ Server-assigned | ✅ Identifier |

---

## 5. Security Model

### Core Principles
1. **Never trust client input** — All client data validated server-side
2. **Fail closed** — If validation fails, reject entire request (no partial application)
3. **Audit everything** — All creation attempts logged (success and failure)
4. **Separation of concerns** — Preview state is separate from server authority

### Validation Rules

**Name:**
- Length: 3-20 characters
- Characters: A-Z, a-z, 0-9, space, hyphen, apostrophe
- No leading/trailing/consecutive spaces
- Profanity filter (server-side list)
- Uniqueness check (database query — future)

**Appearance:**
- All indices validated against design data table bounds
- Color values in range [0.0, 1.0]
- Facial features array max length: 8

**Affinities:**
- Primary and secondary must differ
- Total points must equal 10
- Primary: 3-5 points, Secondary: 2-4 points, Others: 0-3 points each
- No negative values

### Rate Limiting
- Max 5 requests per minute per account
- Rate limit violations logged for analysis

### Attack Vector Defenses
| Attack | Defense |
|--------|---------|
| Name injection | Strict character whitelist |
| Invalid indices | Bounds checking against design data |
| Affinity cheating | Server validates point distribution |
| Replay attacks | Rate limiting |
| Account spoofing | Server derives AccountId from session |
| Client manipulation | Preview state separate from authority |

---

## 6. Planned Code File Manifest

> **⚠️ NOTE:** None of these files exist yet. This is the planned file structure.

### Headers (Public/Character/)
- `FPMCharacterCreationDataContract.h` — Data structures (Request, Result, Identity, Error codes)
- `FPMCharacterCreationValidator.h` — Server-side validation logic
- `FPMCharacterCreationSubsystem.h` — RPC entry point and lifecycle management
- `FPMCharacterCreationTestCommands.h` — Console commands for testing

### Implementation (Private/Character/)
- `FPMCharacterCreationValidator.cpp` — Validation implementation
- `FPMCharacterCreationSubsystem.cpp` — Subsystem implementation
- `FPMCharacterCreationTestCommands.cpp` — Test command implementation

### Preview System
- `Public/Character/Preview/FPMCharacterPreviewActor.h`
- `Private/Character/Preview/FPMCharacterPreviewActor.cpp`

### UI
- `Public/UI/FPMCharacterCreationShellWidget.h`
- `Private/UI/FPMCharacterCreationShellWidget.cpp`

### Bridge
- `Public/Character/Creation/FPMCharacterCreationBridgeComponent.h`
- `Private/Character/Creation/FPMCharacterCreationBridgeComponent.cpp`

---

## 7. Testing Plan

### Console Commands (No UI Required)
| Command | Purpose |
|---------|---------|
| `FPM.TestCharacterCreation "Name"` | Test valid creation |
| `FPM.TestInvalidName` | Test name validation errors |
| `FPM.TestInvalidAppearance` | Test appearance validation errors |
| `FPM.TestInvalidAffinities` | Test affinity validation errors |
| `FPM.TestRateLimit` | Test rate limiting (spam 10 requests) |
| `FPM.TestAllValidation` | Run all tests in sequence |

### How to Test
1. Start dedicated server (or PIE with dedicated server mode)
2. Enable cheats: `EnableCheats`
3. Run test commands
4. Check logs: `LogFPMCharacterCreation` category

---

## 8. Implementation Roadmap

### Phase 1: Foundation (✅ Design Complete — Code NOT Created)
- [x] UI scope boundary defined
- [x] Server lifecycle and authority model documented
- [x] Data ownership model defined
- [x] Data contract structs designed
- [x] Validator rules specified
- [x] RPC entry point architecture decided
- [x] Replication boundaries documented
- [ ] **Actually write the code**

### Phase 2: UI Implementation
1. Create UMG character creation menu (`WBP_CharacterCreationShell`)
2. Add 3D character preview (Mutable + CC5 pipeline)
3. Wire up customization controls (unified inline approach)
4. Implement inline affinity redistribution (zero-sum model)
5. Call `SubmitCharacterCreation()` on button click

### Phase 3: Database Persistence
1. Create `characters` table schema
2. Implement `PersistCharacter()` in subsystem
3. Implement `IsNameTaken()` uniqueness check
4. Add character count limit enforcement (max 5 per account)
5. Create `character_creation_log` audit table

### Phase 4: Character Spawning
1. Create character selection UI
2. Load character from database
3. Spawn character pawn in world
4. Replicate appearance to clients

---

## 9. Configuration

```ini
; Config/DefaultGame.ini
[/Script/FaldoranPrimeMMO.FPMCharacterCreationSubsystem]
MaxCharactersPerAccount=5

[/Script/FaldoranPrimeMMO.FPMServerStartupGateSubsystem]
bFPMStartupGateEnabled=true
FPMStartupGateHoldSeconds=3.0
```

---

## Related Documents
- `Design_Contract_Boundary.md` — System boundary and authority rules
- `Server_Lifecycle_and_Authority.md` — Startup gate and server authority model
- `Technical_Infrastructure.md` — Networking, sharding, persistence architecture
- `../Design/03_Progression_and_Attributes.md` — Affinity system design rules

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
