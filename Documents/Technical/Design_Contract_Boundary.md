# Design Contract Boundary

> **Binding rules** are in [00_Rules_and_Constraints §2](file:///e:/FaldoranPrimeMMO/Documents/Design/00_Rules_and_Constraints.md).  
> This doc covers per-subsystem contracts and data patterns.

---

## Authority Hierarchy
```
Design Documents (intent) → Code (behavior) → Database (state) → Runtime (values)
```
Conflict → higher layer wins.

## System Read/Write Summary

| System | Design Docs | Database | Server State | Client State |
|--------|:-----------:|:--------:|:------------:|:------------:|
| Design Documents | **Write** | Read | Read | Read |
| FPMDatabaseSubsystem | Read | **Write** | Read | None |
| Dedicated Server | Read | Write* | **Write** | Read |
| Player Client | Read | None | Read | Write (local) |
| Data Tables | Read | None | Read | Read |
| Procedural Generation | Read | Read | Write (temp) | Write (temp) |

*Via FPMDatabaseSubsystem only

---

## Subsystem Contracts

### FPMDatabaseSubsystem
- **Write:** PostgreSQL (player + world state, audit logs)
- **Read:** Config files, design constants (for validation)
- Instantiated on **dedicated servers only** (never PIE/client)
- Auto-connect gated by `UFPMServerStartupGateSubsystem`
- All queries async; all statements parameterized (no raw SQL exposure)
- Connection credentials never sent to clients

### Procedural Generation
- Deterministic: same seed → same output always
- In-memory cache only — never writes to database
- Player-built structures override procedural data (stored in DB)

### Crafting & Templates
- Base recipes defined in **data tables** (derived from design docs)
- Player templates validated against design constraints before DB writes
- Server validates all crafting operations before committing

### Reputation & Soul Debt
- Formulas defined in design docs; calculations server-authoritative
- Regional reputation cached at tile level, synced to global DB periodically
- Clients receive values via replication (read-only)

---

## Cross-Boundary Patterns

| From → To | Mechanism |
|-----------|-----------|
| Design → Code | Data Tables, Config files |
| Code → Database | `FPMDatabaseSubsystem` API (server only) |
| Server → Client | UE Replication, RPCs |
| Client → Server | RPC requests only (always validated) |

---

*Copyright Celtic Trinity Studios, 2026.*
