# Dedicated Server Lifecycle and Authority Model

**Document Version:** 1.0  
**Date:** 2026-01-30  
**Purpose:** Document the dedicated server lifecycle spine and authority rules for all systems

> **Binding Rules:** Server authority principles and anti-cheat rules extracted to [00 Rules and Constraints — §2](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md). This document covers the **implementation details** (startup gate, subsystem patterns, code examples).

---

## Lifecycle State Owner: Startup Gate Subsystem

**Current Implementation:** `UFPMServerStartupGateSubsystem`

### Lifecycle States

The dedicated server follows this lifecycle:

```
┌─────────────┐
│  Starting   │ ← Server initializing, subsystems loading
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Holding   │ ← Startup gate is closed, waiting for configured hold time
└──────┬──────┘
       │
       ▼ (Timer expires OR gate disabled)
┌─────────────┐
│    Ready    │ ← Gate opens, OnGateOpened broadcast fires
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Running   │ ← All systems operational, accepting client connections
└─────────────┘
```

### State Transitions

1. **Starting → Holding**
   - Occurs in `UFPMServerStartupGateSubsystem::Initialize()`
   - Only on dedicated servers (`NM_DedicatedServer`)
   - Only if `bFPMStartupGateEnabled = true`
   - Gate closes (`bGateOpen = false`), hold begins (`bHolding = true`)

2. **Holding → Ready**
   - Occurs when timer expires (configured by `FPMStartupGateHoldSeconds`)
   - `OpenGate()` called, broadcasts `OnGateOpened`
   - Gate opens (`bGateOpen = true`), hold ends (`bHolding = false`)

3. **Ready → Running**
   - Implicit transition - once gate is open, server is operational
   - All systems that depend on startup gate can now proceed

### Authority Rules

**The Startup Gate is the ONLY ready blocker:**
- ✅ All subsystems MUST check `IsGateOpen()` before performing server-critical initialization
- ✅ No system may transition to operational state until gate opens
- ✅ Systems bind to `OnGateOpened` delegate to receive notification
- ❌ Systems MUST NOT poll or race the gate state

### Current Compliance

**Compliant Systems:**
- `UFPMDatabaseSubsystem` - Defers auto-connect until gate opens via `OnGateOpened` binding

**Systems to Audit:**
- Character creation (not yet implemented)
- Replication systems (future)
- Gameplay subsystems (future)

---

## Server Authority Model

### Core Principle: Server is ALWAYS Authoritative

**Rule:** The dedicated server is the single source of truth for all game state.

**Implications:**
1. **Clients request, server validates** - No client action is trusted without server validation
2. **Server owns all persistent state** - Database writes only occur server-side
3. **Replication is one-way** - Server → Client (clients never replicate to server)
4. **Client prediction is cosmetic only** - Any client-side state is preview/UX, not authoritative

### Authority Boundaries by System

| System | Authority | Client Role | Validation |
|--------|-----------|-------------|------------|
| Character Creation | Server | Submit request | Server validates all fields |
| Database | Server | None | Server-only access |
| Combat | Server | Send input | Server simulates, validates |
| Inventory | Server | Request action | Server validates, applies |
| Economy | Server | Request trade | Server validates, commits |
| Building | Server | Request placement | Server validates, spawns |

### Anti-Cheat Design Principles

1. **Never trust client input**
   - All client data is considered hostile until validated
   - Validation happens server-side with design contract rules
   - Rate limiting on all client requests

2. **Audit everything**
   - All state changes logged with timestamp, account, IP
   - Failed validation attempts logged for pattern detection
   - Database transactions are atomic and logged

3. **Fail closed**
   - If validation fails, reject the entire request
   - No partial application of client requests
   - Errors return detailed reasons (for legitimate users) but don't leak exploitable info

4. **Separation of concerns**
   - Client UI code NEVER contains validation logic (only UX hints)
   - Server validation code is separate from client code
   - Design contracts are the single source of truth for rules

---

## Startup Gate Integration Pattern

### For New Subsystems

All new subsystems that require server-ready state MUST follow this pattern:

```cpp
// In Initialize()
if (UWorld* World = GetWorld())
{
    if (World->GetNetMode() == NM_DedicatedServer)
    {
        if (UFPMServerStartupGateSubsystem* Gate = 
            GetGameInstance()->GetSubsystem<UFPMServerStartupGateSubsystem>())
        {
            if (Gate->IsGateOpen())
            {
                // Gate already open, proceed immediately
                OnServerReady();
            }
            else
            {
                // Bind to gate opened event
                Gate->OnGateOpened.AddDynamic(this, &UYourSubsystem::OnServerReady);
            }
        }
    }
}
```

### Anti-Pattern: DO NOT DO THIS

```cpp
// ❌ WRONG: Polling the gate
while (!Gate->IsGateOpen()) { /* wait */ }

// ❌ WRONG: Ignoring the gate
Initialize() { DoServerStuff(); } // No gate check!

// ❌ WRONG: Racing the gate
if (SomeCondition) { DoServerStuff(); } // Gate not checked
```

---

## Configuration

**File:** `Config/DefaultGame.ini`

```ini
[/Script/FaldoranPrimeMMO.FPMServerStartupGateSubsystem]
; Enable startup gate on dedicated servers
bFPMStartupGateEnabled=true

; Hold time in seconds (0 = open immediately but still broadcast event)
FPMStartupGateHoldSeconds=3.0
```

---

## Security Implications

### Why This Matters for Anti-Hacking

1. **Prevents race conditions** - Systems can't start in undefined order
2. **Ensures validation is ready** - Design contracts loaded before accepting requests
3. **Database consistency** - DB connection established before any writes
4. **Audit trail integrity** - Logging systems ready before any actions occur

### Future Hardening

- Add health check before opening gate (DB connection, design contracts loaded)
- Add "emergency close" capability if critical system fails
- Add startup gate timeout (if hold exceeds threshold, log error and open anyway)

---

## Verification Checklist

- [x] Startup gate exists and is authoritative
- [x] Gate only activates on dedicated servers
- [x] Gate broadcasts `OnGateOpened` event
- [x] Database subsystem respects gate
- [ ] Character creation respects gate (not yet implemented)
- [ ] All future systems documented to require gate compliance

---

## Related Documents

- `UI_and_Character_Creation_Scope.md` - Defines scope boundaries for UI work
- `CONTRACT_BOUNDARY_README.md` - Design contract system documentation
- `Technical_Infrastructure.md` - Overall technical architecture
