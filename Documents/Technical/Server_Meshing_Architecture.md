# Server Meshing & Dynamic Node Architecture

**Document Version:** 1.0  
**Date:** 2026-03-09  
**Status:** Design — Do NOT implement until core gameplay loop is complete (Steps 1-8 in Full Analysis)  
**Author:** Celtic Trinity Studios  

---

## Table of Contents
1. [Overview & Goals](#overview--goals)
2. [Architecture Layers](#architecture-layers)
3. [Layer 1 — Hex-Zone Server Meshing](#layer-1--hex-zone-server-meshing)
4. [Layer 2 — Dynamic Node Scaling](#layer-2--dynamic-node-scaling)
5. [Seamless Player Handoff](#seamless-player-handoff)
6. [Edge Replication (Cross-Zone Visibility)](#edge-replication-cross-zone-visibility)
7. [Orchestrator Service](#orchestrator-service)
8. [Database Architecture](#database-architecture)
9. [Network Topology](#network-topology)
10. [Integration with Existing Systems](#integration-with-existing-systems)
11. [Implementation Phases](#implementation-phases)
12. [Capacity Planning](#capacity-planning)
13. [Failure Modes & Recovery](#failure-modes--recovery)

---

## Overview & Goals

Faldoran Prime is an Earth-scale planet (~510 million km²) with ~15,654 latitude bands × ~31,309 longitude cells = **~490 million hex chunks**. No single server process can own the entire world. The architecture must:

1. **Seamless traversal** — Players walk/fly/ride across zone boundaries with zero loading screens
2. **Distribute CPU load** — Each server instance only simulates its assigned region, keeping FPS stable
3. **Scale with population** — Empty regions sleep; crowded regions get more server resources
4. **Maintain authority** — The server owning a region is the single source of truth for everything in it
5. **Be failure-resilient** — A crashed server instance is respawned; players reconnect transparently

### What This Is NOT
- This is **not** UE5 World Partition streaming — that's client-side content streaming
- This is **not** Unreal's experimental "Mass Entity" system — we use a lighter custom approach
- This is **not** a peer-to-peer system — all authority remains server-side

---

## Architecture Layers

The system has two nested layers:

```
┌─────────────────────────────────────────────────────────────┐
│                    ORCHESTRATOR SERVICE                     │
│         (External process — manages all instances)          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  ZONE A     │  │  ZONE B     │  │  ZONE C     │  ...   │
│  │  Server     │  │  Server     │  │  Server     │        │
│  │  Instance   │  │  Instance   │  │  Instance   │        │
│  │             │  │             │  │             │        │
│  │ SuperHex    │  │ SuperHex    │  │ SuperHex    │        │
│  │ (R=50 hex   │  │ (R=50 hex   │  │ (R=50 hex   │        │
│  │  chunks)    │  │  chunks)    │  │  chunks)    │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                │                │
│         └────────────────┼────────────────┘                │
│                          │                                  │
│              LAYER 1: HEX-ZONE MESHING                      │
│              (Fixed spatial partitioning)                    │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │           LAYER 2: DYNAMIC NODE SCALING              │  │
│  │                                                      │  │
│  │  Zone B has 200 players? → SPLIT into B1 + B2        │  │
│  │  Zone D has 0 players?   → SLEEP (no instance)       │  │
│  │  Siege at Zone A border? → MERGE edge handling       │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Layer 1 (Hex-Zone Meshing)** divides the planet into fixed-size **SuperHexes** — large hexagonal regions that each map to one server instance. This provides the spatial foundation.

**Layer 2 (Dynamic Node Scaling)** operates *within* Layer 1 to dynamically split, merge, and sleep SuperHexes based on player density. This is the intelligence layer.

---

## Layer 1 — Hex-Zone Server Meshing

### SuperHex Definition

A **SuperHex** is a group of hex chunks that forms one server instance's territory. Using the existing `FFPMChunkCoord` system:

```
SuperHex Radius: 50 hex chunks (~64 km diameter at equator)
SuperHex Area:   ~7,500 hex chunks per SuperHex
                 ~3,200 km² per SuperHex (roughly Rhode Island-sized)
Total SuperHexes: ~160,000 to cover the planet
```

Each SuperHex is identified by a **SuperHex coordinate** derived from its center chunk:

```cpp
// Proposed: SuperHex coordinate
struct FFPMSuperHexCoord {
    int32 SQ = 0;  // SuperHex Q (column)
    int32 SR = 0;  // SuperHex R (row)
    
    // Convert chunk coord to owning SuperHex
    static FFPMSuperHexCoord FromChunkCoord(const FFPMChunkCoord& Chunk) {
        // Integer division groups chunks into SuperHexes
        return { 
            FMath::FloorToInt32(Chunk.Q / 50.0f),
            FMath::FloorToInt32(Chunk.R / 50.0f)
        };
    }
    
    // Get the center chunk of this SuperHex
    FFPMChunkCoord CenterChunk() const {
        return FFPMChunkCoord(SQ * 50 + 25, SR * 50 + 25);
    }
};
```

### Zone Ownership Map

The Orchestrator maintains a global **Zone Ownership Map**:

```
ZoneOwnershipMap: SuperHexCoord → ServerInstanceID
```

This map is:
- **Authoritative**: Only the Orchestrator modifies it
- **Replicated**: Every server instance and gateway has a cached copy
- **Versioned**: Each update increments a global version counter for consistency

### Server Instance Responsibilities

Each server instance (`FaldoranPrimeMMOServer.exe`) runs as a dedicated server owning one or more SuperHexes:

| Responsibility | Scope |
|---|---|
| **Entity simulation** | All NPCs, dropped items, structures within its SuperHexes |
| **Player authority** | All players whose *primary chunk* is in its SuperHexes |
| **Terrain generation** | On-demand for its chunk range (deterministic from seed) |
| **DB persistence** | Writes for entities/structures in its SuperHexes |
| **Edge replication** | Sends entity updates to neighboring zone servers (see §6) |

---

## Layer 2 — Dynamic Node Scaling

Dynamic Node Scaling operates *within* the Layer 1 grid, adjusting server resources based on player activity.

### Node States

Each SuperHex exists in one of four states:

```
┌──────────┐    Players    ┌──────────┐    Players    ┌──────────┐
│ SLEEPING │ ──arrive──▸   │  WAKING  │ ──loaded──▸   │  ACTIVE  │
│          │               │          │               │          │
│ No server│               │ Instance │               │ Instance │
│ instance │               │ booting  │               │ running  │
│          │  ◂──empty──   │          │  ◂──empty──   │          │
└──────────┘   (timeout)   └──────────┘   (timeout)   └────┬─────┘
                                                           │
                                          Overloaded       │
                                          (>threshold)     │
                                                           ▼
                                                     ┌──────────┐
                                                     │ SPLITTING│
                                                     │          │
                                                     │ 1 Super  │
                                                     │ → 2 Sub  │
                                                     └──────────┘
```

| State | Description | Server Instance |
|---|---|---|
| **Sleeping** | No players within or approaching. No server process running. | None |
| **Waking** | A player is approaching (within 2 SuperHexes). Instance is booting. | Starting up |
| **Active** | ≥1 player present. Normal operation. | Running |
| **Splitting** | Player count exceeds threshold. SuperHex subdivides into 2-4 sub-zones. | 1 → N instances |

### Wake-Ahead System

Players don't notice zone boundaries because the Orchestrator **pre-wakes** servers ahead of them:

```
Player moving East at speed V
├── Current SuperHex: ACTIVE (owned by Instance A)
├── Next SuperHex East: WAKING (Instance B booting)
├── SuperHex +2 East: WAKING (Instance C booting, lower priority)
└── SuperHex +3 East: SLEEPING (will wake when +2 becomes current)

Wake-ahead distance = max(2 SuperHexes, V × 30 seconds)
```

Since a SuperHex is ~64 km wide and even top-tier flight (Rift Speed) covers ~10 km/s, a 2-SuperHex look-ahead gives **~6+ seconds** of boot time — sufficient for a dedicated server to start.

### Splitting Rules

When a SuperHex is overloaded:

```
Threshold: >80 players in one SuperHex  (configurable)
Split Strategy: Bisect along longest axis of player distribution

Before Split:
┌──────────────────────┐
│     SuperHex A       │
│  ● ● ●  ●  ●● ●     │
│    ●  ●● ●   ●      │
│      (120 players)   │
└──────────────────────┘

After Split:
┌──────────┬───────────┐
│  Zone A1 │  Zone A2  │
│  ● ● ●  │ ●  ●● ●   │
│  Instance│ Instance  │
│    #7    │   #12     │
│ (55 plr) │ (65 plr)  │
└──────────┴───────────┘
```

Players in the split zone experience no loading screen — they simply start receiving entity updates from their new authoritative server. The split is **transparent** because:
1. Terrain is deterministic (same seed → same mesh on any server)
2. Entity state is transferred between instances via the Orchestrator
3. The client already replicates appearance/position — it just changes which server is authoritative

### Merging Rules

When adjacent sub-zones both drop below threshold:

```
Merge Threshold: <20 players total across both sub-zones
Cooldown: 5 minutes after last split (prevents thrashing)
Merge: Both sub-zones collapse back to one SuperHex, one instance
```

### Sleep Rules

```
Sleep Threshold: 0 players for 5 minutes AND no approaching players within 2 SuperHexes
Sleep Action: Save all entity state to DB → terminate instance
Wake Cost: ~3-5 seconds (instance start + load state from DB)
```

---

## Seamless Player Handoff

The core of "no loading screens" — how a player crosses from Zone A to Zone B.

### The Handoff Corridor

Each SuperHex has a **corridor** of shared authority along its borders — a strip of chunks that *both* neighboring servers simulate. This is the overlap zone:

```
                    ┌─ Corridor Width: 3 hex chunks (~3.8 km) ─┐
                    │                                           │
┌───────────────────┼───┬───────────────────────────────┬───┼───────────────────┐
│                   │   │                               │   │                   │
│    Zone A         │   │     HANDOFF CORRIDOR          │   │    Zone B         │
│    (Server #1)    │ A │    (Both servers simulate)    │ B │    (Server #2)    │
│                   │ u │                               │ u │                   │
│                   │ t │  Player transitions here:     │ t │                   │
│                   │ h │  1. Both servers get updates  │ h │                   │
│                   │ o │  2. Authority transfers       │ o │                   │
│                   │ r │  3. Old server releases       │ r │                   │
│                   │ i │                               │ i │                   │
│                   │ t │                               │ t │                   │
│                   │ y │                               │ y │                   │
│                   │   │                               │   │                   │
│                   │ A │                               │ B │                   │
└───────────────────┼───┴───────────────────────────────┴───┼───────────────────┘
```

### Handoff Sequence (Player Crossing A → B)

```
Step 1: APPROACH
  Player enters corridor (3 chunks from border)
  Server A notifies Orchestrator: "Player X approaching Zone B boundary"
  Orchestrator ensures Server B is ACTIVE (wake if sleeping)
  Orchestrator tells Server B: "Expect Player X, here's their state"

Step 2: DUAL AUTHORITY  
  Server B creates a "shadow" of the player (receives inputs, simulates, but doesn't replicate)
  Server A remains authoritative — client still connected to A
  Server B begins simulating the corridor chunks (already doing this for edge replication)

Step 3: BOUNDARY CROSS
  Player's primary chunk enters Zone B territory
  Orchestrator atomically updates ZoneOwnershipMap: Player X → Server B
  Server B becomes authoritative for Player X
  Client receives "authority migration" packet: { NewServerIP, NewPort, SessionToken }
  Client opens UDP channel to Server B (keeps A channel alive briefly)

Step 4: CONFIRMATION
  Client confirms connection to Server B
  Server B begins replicating to client as primary
  Server A receives "release" notification
  Server A removes Player X from its player list

Step 5: CLEANUP
  After 5 seconds: Client closes channel to Server A
  Server A's shadow of Player X is destroyed
  
Total time: <200ms perceived by player (sub-frame)
```

### Client-Side Implementation

The client (`UFPMGameInstance`) needs a small extension:

```cpp
// Proposed addition to UFPMGameInstance
UCLASS()
class UFPMGameInstance : public UGameInstance {
    // ... existing ...
    
    // --- Server Meshing (Phase: Server Meshing) ---
    
    /** Current authoritative server connection */
    UPROPERTY()
    FString PrimaryServerIP;
    int32 PrimaryServerPort;
    
    /** Handle authority migration — called by Orchestrator packet */
    void OnAuthorityMigration(const FString& NewServerIP, int32 NewPort, 
                              const FString& SessionToken);
    
    /** Secondary connection for corridor overlap */
    UPROPERTY()
    class UFPMSecondaryConnection* CorridorConnection;
};
```

---

## Edge Replication (Cross-Zone Visibility)

Players near a zone border need to **see** entities (other players, NPCs, structures) in the neighboring zone. This is edge replication.

### What Gets Replicated Across Edges

| Data | Replicated? | Update Rate | Notes |
|---|---|---|---|
| Player position/rotation | ✅ | 20 Hz | Interpolated on receiving server |
| Player appearance | ✅ | On change | Name, species, equipment visuals |
| NPC position/rotation | ✅ | 10 Hz | Lower priority than players |
| NPC health (boss) | ✅ | On change | Only for bosses near the border |
| Structures | ✅ | On place/destroy | Static once placed |
| Dropped items | ❌ | — | Only visible on owning server |
| Particles/VFX | ❌ | — | Cosmetic, not replicated |
| Terrain modifications | ✅ | On change | Voxel deltas sent on terraform |

### Edge Replication Protocol

Neighboring servers communicate via a **direct server-to-server UDP channel** (not through the Orchestrator — too slow):

```
Server A ◄──── Direct UDP ────► Server B
              │
              ├── EntityUpdate { EntityID, Position, Rotation, Velocity }
              ├── EntitySpawn  { EntityID, Type, FullState }
              ├── EntityDespawn { EntityID }
              └── TerraformDelta { ChunkCoord, VoxelKey, Delta }
              
Edge Distance: 5 hex chunks from border = ~6.4 km
  (matches MediumDetailRange — entities beyond this aren't rendered)
```

Each server maintains **proxy actors** for edge entities:

```cpp
// Proposed: Proxy actor for cross-zone entities
UCLASS()
class AFPMEdgeProxy : public AActor {
    GENERATED_BODY()
    
    /** The unique ID of the entity on its home server */
    UPROPERTY()
    FGuid RemoteEntityID;
    
    /** Which server owns this entity */
    UPROPERTY()
    int32 HomeServerInstanceID;
    
    /** Visual representation (skeletal mesh, etc.) */
    UPROPERTY()
    USkeletalMeshComponent* ProxyMesh;
    
    // No collision, no gameplay logic — purely visual
    // Updated via edge replication packets
};
```

---

## Orchestrator Service

The Orchestrator is an **external process** (not an Unreal server) that manages the entire server fleet. It runs on `CelticTrinityStudiosOne` alongside the game servers.

### Responsibilities

```
1. ZONE MANAGEMENT     — Maintain ZoneOwnershipMap
2. INSTANCE LIFECYCLE  — Start/stop/restart server instances
3. PLAYER ROUTING      — Tell clients which server to connect to
4. HEALTH MONITORING   — Detect crashed/stalled instances
5. DYNAMIC SCALING     — Split/merge/sleep decisions (Layer 2)
6. STATE TRANSFER      — Coordinate entity handoff between instances
```

### Technology Choice

| Option | Pros | Cons | Recommendation |
|---|---|---|---|
| **Custom C++ Service** | Full control, low latency | More code to write | ✅ **Recommended** |
| **Node.js Service** | Faster to prototype, JSON-native | Higher latency, GC pauses | Good for v1 prototype |
| **Python Service** | Fastest to write | Too slow for real-time decisions | ❌ Not suitable |

The Orchestrator communicates with game servers via **TCP** (reliable) and with clients via **UDP** (fast routing updates).

### Orchestrator Data Model

```
┌─────────────────────────────────────────────────────────┐
│ Orchestrator State                                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ ZoneOwnershipMap:                                       │
│   SuperHex(0,0)  → Instance #1  [ACTIVE,  45 players]  │
│   SuperHex(1,0)  → Instance #2  [ACTIVE,  12 players]  │
│   SuperHex(0,1)  → (none)       [SLEEPING, 0 players]  │
│   SuperHex(-1,0) → Instance #3  [WAKING,   0 players]  │
│                                                         │
│ InstanceRegistry:                                       │
│   Instance #1: PID=4521, Port=7777, RAM=3.2GB, CPU=8%  │
│   Instance #2: PID=4522, Port=7778, RAM=2.1GB, CPU=3%  │
│   Instance #3: PID=4523, Port=7779, RAM=0.5GB, CPU=1%  │
│                                                         │
│ PlayerRegistry:                                         │
│   Player "DragonSlayer99" → Instance #1, Chunk(12, 7)   │
│   Player "ElvenCrafter"   → Instance #2, Chunk(55, 3)   │
│                                                         │
│ EdgeConnections:                                        │
│   Instance #1 ←→ Instance #2  (shared border)          │
│   Instance #1 ←→ Instance #3  (shared border)          │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Orchestrator API

```
// Server → Orchestrator (TCP)
RegisterInstance(InstanceID, Port, SuperHexList)
PlayerEnteredZone(PlayerID, ChunkCoord)
PlayerApproachingBorder(PlayerID, TargetSuperHex, ETA_seconds)
ReportHealth(InstanceID, RAM_MB, CPU_pct, PlayerCount, TickRate_ms)
RequestSplit(SuperHexCoord, SplitAxis)
RequestMerge(SuperHexA, SuperHexB)

// Orchestrator → Server (TCP)
AssignSuperHex(SuperHexCoord)
ReleaseSuperHex(SuperHexCoord)
PrepareHandoff(PlayerID, PlayerState, TargetInstance)
AcceptHandoff(PlayerID)
EntityTransfer(EntityList, TargetInstance)
Shutdown(GracePeriod_seconds)

// Orchestrator → Client (UDP, via Gateway)
AuthorityMigration(NewServerIP, NewPort, SessionToken)
ServerList(AvailableServers)  // For initial connection
```

---

## Database Architecture

The PostgreSQL database is the **persistent source of truth**. Server instances read/write entity state, but the Orchestrator controls *which* instance has write authority for which region.

### Region-Aware Tables

```sql
-- Existing tables (no changes needed)
-- accounts, characters, character_affinities, inventory, equipment

-- New: Zone assignment tracking
CREATE TABLE zone_assignments (
    super_hex_q     INTEGER NOT NULL,
    super_hex_r     INTEGER NOT NULL,
    instance_id     INTEGER,          -- NULL = sleeping
    state           VARCHAR(16) NOT NULL DEFAULT 'sleeping',
    player_count    INTEGER NOT NULL DEFAULT 0,
    last_active     TIMESTAMPTZ,
    PRIMARY KEY (super_hex_q, super_hex_r)
);

-- New: Persistent world modifications (structures, terrain edits, depletion)
CREATE TABLE world_modifications (
    mod_id          BIGSERIAL PRIMARY KEY,
    super_hex_q     INTEGER NOT NULL,
    super_hex_r     INTEGER NOT NULL,
    chunk_q         INTEGER NOT NULL,
    chunk_r         INTEGER NOT NULL,
    mod_type        VARCHAR(32) NOT NULL,  -- 'structure', 'terraform', 'depletion'
    mod_data        JSONB NOT NULL,         -- Serialized modification data
    created_by      INTEGER REFERENCES characters(id),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    
    -- Index for fast region loading
    CONSTRAINT fk_superhex FOREIGN KEY (super_hex_q, super_hex_r) 
        REFERENCES zone_assignments(super_hex_q, super_hex_r)
);

CREATE INDEX idx_world_mods_superhex 
    ON world_modifications(super_hex_q, super_hex_r);

-- New: Server instance registry (runtime, cleared on full restart)
CREATE TABLE server_instances (
    instance_id     SERIAL PRIMARY KEY,
    process_id      INTEGER,
    port            INTEGER NOT NULL,
    started_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_heartbeat  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ram_mb          INTEGER,
    cpu_percent     REAL,
    player_count    INTEGER NOT NULL DEFAULT 0,
    state           VARCHAR(16) NOT NULL DEFAULT 'starting'
);
```

### Write Authority Rules

| Data | Write Authority | Read Access |
|---|---|---|
| Player position/state | Server owning player's primary chunk | Any server (via edge replication) |
| NPC state | Server owning NPC's home SuperHex | Neighbors (via edge replication) |
| Structures | Server owning structure's chunk | Neighbors |
| Terrain mods | Server owning chunk | Neighbors |
| Inventory | Server owning player (locks to one server) | Only owning server |
| Zone assignments | Orchestrator ONLY | All servers (read-only cache) |

---

## Network Topology

```
                              INTERNET
                                 │
                    ┌────────────┼────────────┐
                    │                          │
              ┌─────┴─────┐            ┌──────┴──────┐
              │  GATEWAY   │            │  GATEWAY    │
              │  (public)  │            │  (public)   │
              │ Port 7776  │            │  Port 7776  │
              └─────┬──────┘            └──────┬──────┘
                    │          LAN              │
        ┌───────────┼───────────┬──────────────┘
        │           │           │
  ┌─────┴─────┐ ┌──┴──┐  ┌────┴────┐
  │ Instance 1│ │Inst 2│  │ Inst N  │
  │ Port 7777 │ │ 7778 │  │ 7777+N  │
  └───────────┘ └──────┘  └─────────┘
        │           │           │
        └───────────┼───────────┘
                    │
              ┌─────┴─────┐
              │ PostgreSQL │
              │ Port 5432  │
              └────────────┘
```

### Gateway Server

A lightweight proxy that:
1. Accepts client connections from the internet
2. Queries the Orchestrator for the player's assigned server instance
3. Forwards UDP traffic to the correct instance
4. Handles authority migration transparently (the client's *public* endpoint doesn't change)

This means the **client only ever connects to the Gateway** — it doesn't need to know about individual server instances. When authority migrates, the Gateway redirects packets internally.

### Port Allocation

```ini
; Proposed addition to DefaultGame.ini
[FPM.ServerMesh]
GatewayPort=7776
BaseInstancePort=7777
MaxInstances=200
OrchestratorPort=9000
OrchestratorIP=127.0.0.1
EdgeReplicationPort=9100
```

---

## Integration with Existing Systems

### What Changes

| Existing System | Change Required | When |
|---|---|---|
| `UFPMGameInstance` | Add `OnAuthorityMigration()`, connect via Gateway | Phase 1 |
| `AFPMGameMode` | Add zone ownership check, edge replication hooks | Phase 1 |
| `AFPMWorldChunkManager` | Receive chunk range from Orchestrator (only generate owned chunks) | Phase 1 |
| `UFPMDatabaseSubsystem` | Add region-scoped queries, write-lock awareness | Phase 1 |
| `UFPMServerStartupGateSubsystem` | Extended: gate waits for Orchestrator assignment | Phase 1 |
| `AFPMPlayerController` | Authority migration handling | Phase 2 |
| `UFPMInventoryComponent` | Lock to one server during handoff (brief freeze) | Phase 2 |
| `FPMChunkGenerator` | No changes — deterministic by seed, works anywhere | None |
| `FFPMGeoCoord` | No changes — already supports full planet | None |
| `FPMChunkConstants` | No changes — already defines full planet scale | None |

### What Doesn't Change

Critical: **The terrain generation system is already perfect for server meshing** because:
- `FPMChunkGenerator::GenerateChunk()` is a **pure function** of `(ChunkCoord, Seed)`
- Any server can generate any chunk independently with identical results
- No chunk data is "transmitted" between servers — each generates its own copy
- The same `WorldSeed` is shared across all instances

---

## Implementation Phases

> [!IMPORTANT]
> Do NOT start until Steps 1-8 from the Full Analysis are complete. Server meshing without gameplay to mesh is meaningless.

### Phase M1 — Gateway + Multi-Instance (Foundation)
**Prerequisite:** Core gameplay loop working on a single dedicated server

```
1. Build the Gateway proxy (standalone C++ or Node.js process)
2. Build the Orchestrator (standalone process)
3. Modify UFPMGameInstance to connect via Gateway
4. Modify AFPMGameMode to register with Orchestrator on startup
5. Test: 2 server instances, each owning half the world (fixed assignment)
6. Client connects to Gateway → routed to correct instance

Deliverables:
  - Tools/Orchestrator/   (orchestrator service)
  - Tools/Gateway/        (gateway proxy)
  - Config/ServerMesh.ini (zone configuration)
```

### Phase M2 — Seamless Handoff
**Prerequisite:** Phase M1 working

```
1. Implement handoff corridor (3-chunk overlap)
2. Implement OnAuthorityMigration on client
3. Implement PrepareHandoff/AcceptHandoff on servers
4. Test: Player walks from Zone A to Zone B with zero loading screen
5. Verify inventory integrity across handoff

Deliverables:
  - UFPMAuthorityMigrationComponent
  - Edge replication protocol (server-to-server UDP)
```

### Phase M3 — Edge Replication
**Prerequisite:** Phase M2 working

```
1. Implement AFPMEdgeProxy for cross-zone entity visibility
2. Implement server-to-server entity update channel
3. Test: Player in Zone A can see another player in Zone B near the border
4. Test: NPC near border is visible from both zones

Deliverables:
  - AFPMEdgeProxy actor
  - UFPMEdgeReplicationSubsystem
```

### Phase M4 — Dynamic Nodes (Layer 2)
**Prerequisite:** Phase M3 working

```
1. Implement sleep/wake in Orchestrator
2. Implement split/merge in Orchestrator
3. Implement wake-ahead prediction (player velocity × 30s)
4. Test: Walk into empty region → server wakes ahead
5. Test: 100 players in one zone → zone splits, both halves stable
6. Test: Players leave → zones merge → zone sleeps

Deliverables:
  - Orchestrator v2 with dynamic scaling
  - Monitoring dashboard (web UI)
```

### Phase M5 — Production Hardening
**Prerequisite:** Phase M4 working

```
1. Crash recovery: Orchestrator detects lost heartbeat → respawns instance
2. State recovery: New instance loads from DB + receives entity transfers
3. DDoS protection at Gateway level
4. Monitoring, alerting, logging
5. Cloud burst support (spin up AWS/Azure instances for events)

Deliverables:
  - Automated crash recovery
  - Monitoring/alerting system
  - Cloud deployment scripts
```

---

## Capacity Planning

Based on `Server_Hardware_Specs.md` (CelticTrinityStudiosOne):

### Per-Instance Resources (Updated)

| Scenario | RAM | CPU Threads | Max Instances on CTS-One |
|---|---|---|---|
| Sleeping zone | 0 MB | 0 | ∞ (not running) |
| Empty active zone | ~500 MB | 1 | ~900 |
| Normal zone (20 players) | ~2 GB | 2 | ~240 |
| Busy zone (50 players) | ~4 GB | 3 | ~120 |
| Siege zone (100 players, pre-split) | ~6 GB | 4 | ~80 |
| After split (50 per sub-zone) | ~4 GB each | 3 each | ~120 |

### Reserved Resources

| Service | RAM | CPU |
|---|---|---|
| Windows Server OS | 4 GB | 2 threads |
| PostgreSQL | 8 GB | 4 threads |
| Orchestrator | 500 MB | 1 thread |
| Gateway (×2) | 500 MB each | 1 thread each |
| Monitoring | 1 GB | 1 thread |
| **Total Reserved** | **~15 GB** | **~10 threads** |

### Net Available: ~497 GB RAM, ~62 threads

### Realistic Concurrent Player Estimates

| Player Count | Active Zones | RAM Used | CPU Used | Status |
|---|---|---|---|---|
| 100 | ~5-10 | ~15 GB | 15 threads | 🟢 Comfortable |
| 500 | ~20-30 | ~60 GB | 40 threads | 🟢 Comfortable |
| 1,000 | ~40-60 | ~120 GB | 55 threads | 🟡 Moderate |
| 2,000 | ~80-120 | ~240 GB | 60 threads | 🟡 Near CPU limit |
| 3,000+ | ~120+ | ~360 GB+ | 62+ threads | 🔴 Need cloud burst |

> [!NOTE]
> The CelticTrinityStudiosOne machine can comfortably handle alpha/beta with 500-1,000 concurrent players. For launch with 3,000+, cloud burst capacity on AWS/Azure supplements the bare metal.

---

## Failure Modes & Recovery

| Failure | Detection | Recovery | Player Impact |
|---|---|---|---|
| **Instance crash** | Heartbeat timeout (5s) | Orchestrator restarts instance, loads state from DB | Players reconnected within ~10s, minor rubberbanding |
| **Orchestrator crash** | Watchdog process | Auto-restart, rebuild state from DB `zone_assignments` table | Handoffs paused 3-5s, existing connections unaffected |
| **Gateway crash** | Load balancer health check | Failover to secondary Gateway | Client reconnects within ~2s |
| **DB unreachable** | Connection timeout | Retry with exponential backoff, instances continue with cached state | Saves paused, no data loss (queued writes) |
| **Network partition** | Edge replication timeout | Instances operate independently, reconcile on reconnect | Brief invisibility of cross-zone entities |
| **Instance overloaded** | Tick rate drops below 20 Hz | Orchestrator force-splits the zone | Brief stutter during split migration |

### Data Integrity Guarantees

1. **Player inventory is NEVER duplicated** — during handoff, the old server writes to DB and the new server reads. There's an atomic DB lock on the character row during handoff.
2. **At-most-once writes** — modifications use DB transactions with server instance ID checks. If two servers somehow both try to write the same entity, the DB rejects the second write.
3. **Terrain is always recoverable** — deterministic generation from seed means terrain is never "lost," only player modifications (stored in DB) need protection.

---

## Glossary

| Term | Definition |
|---|---|
| **SuperHex** | A group of ~7,500 hex chunks forming one server instance's territory (~64 km diameter) |
| **Orchestrator** | External service managing server instance lifecycle, zone assignment, and player routing |
| **Gateway** | Public-facing UDP proxy that routes client traffic to the correct server instance |
| **Handoff Corridor** | 3-chunk-wide overlap zone along SuperHex borders where two servers co-simulate |
| **Edge Proxy** | Read-only visual representation of an entity owned by a neighboring server |
| **Authority Migration** | The process of transferring a player's authoritative server from one instance to another |
| **Wake-Ahead** | Pre-booting a server instance before a player arrives, based on velocity prediction |
| **Dynamic Node** | A SuperHex that dynamically splits/merges/sleeps based on player density |

---

*Copyright Celtic Trinity Studios, 2026.*
