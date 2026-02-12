# Phase 6 — Character Select + Spawn

**Status:** ✅ CODE COMPLETE — 2026-02-08  
**Prerequisite:** Phases 0–5 complete  

---

## What Was Built

### New Files Created
| File | Path | Purpose |
|------|------|---------|
| `FPMPlayerCharacter.h` | `Public/Player/` | Player character with replicated appearance |
| `FPMPlayerCharacter.cpp` | `Private/Player/` | Third-person camera, WASD input, OnRep appearance |
| `FPMCharacterSelectWidget.h` | `Public/UI/` | Character select screen backing class |
| `FPMCharacterSelectWidget.cpp` | `Private/UI/` | Dynamic character list, button handlers |
| `FFPMCharacterSummary` | Added to `FPMCharacterCreationDataContract.h` | Lightweight character data for select list |

### Files Modified
| File | Changes |
|------|---------|
| `FPMGameMode.h/.cpp` | DefaultPawnClass=nullptr, PostLogin override (no auto-spawn) |
| `FPMPlayerController.h/.cpp` | Added character select RPCs, enter world flow, UI transitions |
| `FPMCharacterCreationWidget.cpp` | Back button now goes to character select (not login) |

### New RPCs Added to AFPMPlayerController
| RPC | Direction | Purpose |
|-----|-----------|---------|
| `ServerRequestCharacterList()` | Client → Server | Query DB for account's characters |
| `ClientReceiveCharacterList()` | Server → Client | Send character summaries for UI |
| `ServerRequestEnterWorld(FGuid)` | Client → Server | Validate ownership, spawn, possess |
| `ClientEnterWorldSuccess()` | Server → Client | Hide UI, switch to game input |
| `ClientEnterWorldFailed(FString)` | Server → Client | Show error on select screen |

---

## UI Flow (Complete)

```
     ┌─────────────┐
     │  Login Screen │
     └──────┬────────┘
            │ Login Success
            ▼
     ┌──────────────────┐
     │ Request Char List │
     └──────┬───────────┘
            │
     ┌──────┴──────────────┐
     │                     │
     ▼ (has characters)    ▼ (no characters)
┌─────────────────┐  ┌──────────────────┐
│ Character Select │  │ Create Character │
│   - Char List    │  │   (auto-redirect)│
│   - Enter World  │  └────────┬─────────┘
│   - Create New   │           │ Created
│   - Delete (stub)│           ▼
└────────┬─────────┘  ┌──────────────────┐
         │            │ Return to Select  │
         │ Enter World└──────────────────┘
         ▼
┌──────────────────┐
│ Server Spawns    │
│ AFPMPlayerCharacter │
│ → Possess → Hide UI │
│ → Game Input Mode│
└──────────────────┘
```

---

## Security Measures (Phase 6 Specific)

1. **Character ownership validated server-side**: `WHERE character_id = $1 AND account_id = $2`
2. **Server spawns pawn**: Client never instantiates `AFPMPlayerCharacter`
3. **Authentication required**: All RPCs check `bIsAuthenticated` before processing
4. **DefaultPawnClass = nullptr**: No pawn auto-spawned — must go through RPC flow
5. **Replication is one-way**: Appearance props use `ReplicatedUsing` (server → client only)

---

## Build WBP_CharacterSelect in UE Editor

See `Phase6_WBP_CharacterSelect_BuildGuide.md` for step-by-step editor instructions.

---

## Testing Checklist

- [ ] Login → see character list → select → spawn in world
- [ ] Open 2nd client → login with different account → both players see each other
- [ ] Disconnect and reconnect → character persists
- [ ] Login with no characters → auto-redirected to character creation
- [ ] Create character → redirected to character select with new character
- [ ] "Create New" button → goes to character creation → Back → returns to select
- [ ] Enter world with valid character → UI hidden, game input enabled
- [ ] Enter world with invalid character ID → error shown on select screen

🎉 **THIS IS THE PROTOTYPE MILESTONE** 🎉
