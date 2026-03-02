# Server Lifecycle & Authority

> **Authority rules** → [00_Rules_and_Constraints §2](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md).  
> This doc covers the startup gate implementation.

---

## Startup Gate — `UFPMServerStartupGateSubsystem`

### Lifecycle
```
Starting → Holding → Ready → Running
```
- Gate only activates on `NM_DedicatedServer` with `bFPMStartupGateEnabled=true`
- Timer duration: `FPMStartupGateHoldSeconds` (default 3.0s)
- On expiry: calls `OpenGate()` → broadcasts `OnGateOpened`

### Integration Pattern (all new subsystems must use this)
```cpp
// In Initialize()
if (World->GetNetMode() == NM_DedicatedServer)
{
    auto* Gate = GetGameInstance()->GetSubsystem<UFPMServerStartupGateSubsystem>();
    if (Gate->IsGateOpen()) { OnServerReady(); }
    else { Gate->OnGateOpened.AddDynamic(this, &UYourSubsystem::OnServerReady); }
}
```

### Anti-Patterns — NEVER do this
```cpp
while (!Gate->IsGateOpen()) { /* wait */ }  // ❌ polling
Initialize() { DoServerStuff(); }             // ❌ ignoring gate
```

### Config (`Config/DefaultGame.ini`)
```ini
[/Script/FaldoranPrimeMMO.FPMServerStartupGateSubsystem]
bFPMStartupGateEnabled=true
FPMStartupGateHoldSeconds=3.0
```

### Compliance Status
- ✅ `UFPMDatabaseSubsystem` — gate-compliant
- ⬜ Character creation (not yet built)
- ⬜ All future subsystems (required)

---

*Copyright Celtic Trinity Studios, 2026.*
