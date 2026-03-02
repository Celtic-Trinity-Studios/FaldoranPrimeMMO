# Character Creation System

**Status:** Design complete — code NOT yet written.  
**Location:** `UFPMCharacterCreationSubsystem` (GameInstance subsystem)

> Security & authority rules → [00_Rules_and_Constraints §2](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md)

---

## Architecture

### Why GameInstance Subsystem
- Lives entire session; available on both client and server
- UI calls subsystem — logic not in widgets
- Character creation ≠ character spawning (separate concerns)

### Flow
```
Client UI → FFPMCharacterCreationRequest (UNTRUSTED)
         → [RPC] UFPMCharacterCreationSubsystem::SubmitCharacterCreation()
         → Gate check → rate limit check → UFPMCharacterCreationValidator
         → FFPMCharacterIdentity (server-owned) + audit log
         → FFPMCharacterCreationResult RPC response → Client displays result
```

---

## Data Model

### Server-Assigned Fields (never from client)
| Field | Type |
|-------|------|
| `AccountId` | `FGuid` (from session) |
| `CharacterId` | `FGuid::NewGuid()` |
| `CreationTimestamp` | Server `FDateTime` |

### Client-Requested, Server-Validated Fields
| Field | Type | Validation |
|-------|------|------------|
| `CharacterName` | `FString` | 3–20 chars, `[A-Za-z0-9 \-']`, profanity filter |
| `BodyType` | `uint8` | Valid design index (0–3) |
| `SkinTone` | `FLinearColor` | RGB 0.0–1.0 |
| `HairStyle` | `uint8` | Valid design index |
| `HairColor` | `FLinearColor` | RGB 0.0–1.0 |
| `FacialFeatures` | `TArray<uint8>` | Max 8, each valid index |
| `VoiceType` | `uint8` | Valid design index |
| `PrimaryAffinity` | `EFPMAffinity` | Valid enum |
| `SecondaryAffinity` | `EFPMAffinity` | Must differ from primary |
| `StartingAffinityPoints` | `TMap<EFPMAffinity, int32>` | Total=10 · Primary 3–5 · Secondary 2–4 · Others 0–3 |

### Affinity Enum
```cpp
UENUM(BlueprintType)
enum class EFPMAffinity : uint8
{
    None = 0, Fire, Water, Earth, Air, Light, Shadow, Nature, Arcane
};
```

---

## Data Domains

| Domain | Lifetime | What |
|--------|----------|------|
| **Client-Only Preview** | UI open → close | 3D mesh, morph state, UI selections (never sent to server) |
| **Server-Owned** | Post-validation | `FFPMCharacterIdentity`, persisted to DB |
| **Replicated Post-Spawn** | After world spawn | Appearance values replicated to other clients |

---

## Planned File Manifest

**Public/Character/**
- `FPMCharacterCreationDataContract.h` — Request, Result, Identity, Error codes
- `FPMCharacterCreationValidator.h`
- `FPMCharacterCreationSubsystem.h`

**Private/Character/**
- `FPMCharacterCreationValidator.cpp`
- `FPMCharacterCreationSubsystem.cpp`

**Preview/Bridge/UI**
- `FPMCharacterPreviewActor.h/.cpp`
- `FPMCharacterCreationShellWidget.h/.cpp`
- `FPMCharacterCreationBridgeComponent.h/.cpp`

---

## Config
```ini
[/Script/FaldoranPrimeMMO.FPMCharacterCreationSubsystem]
MaxCharactersPerAccount=1
```

## Console Test Commands
| Command | Purpose |
|---------|---------|
| `FPM.TestCharacterCreation "Name"` | Valid creation |
| `FPM.TestInvalidName` | Name validation |
| `FPM.TestInvalidAffinities` | Affinity validation |
| `FPM.TestRateLimit` | Rate limiting |

---

*Copyright Celtic Trinity Studios, 2026.*
