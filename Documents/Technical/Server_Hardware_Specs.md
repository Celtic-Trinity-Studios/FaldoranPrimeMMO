# Server Hardware Specifications

**Document Version:** 1.0  
**Date:** 2026-02-17  
**Purpose:** Document production server hardware for capacity planning and server meshing architecture

---

## Primary Server: CelticTrinityStudiosOne

| Component | Specification |
|---|---|
| **Device Name** | CelticTrinityStudiosOne |
| **Processor** | 2× Intel Xeon E5-2697 v4 @ 2.30 GHz (Dual Socket) |
| **Architecture** | x64 (64-bit) |
| **Total Cores** | 36 (18 per socket) |
| **Total Threads** | 72 (36 per socket, Hyper-Threading) |
| **Installed RAM** | 512 GB (512 GB usable) |
| **Operating System** | Windows Server 2022 Standard (21H2, Build 20348.4052) |
| **Installed** | 2023-08-19 |

### CPU Details — Xeon E5-2697 v4 (Broadwell-EP)
- 18 cores / 36 threads per socket
- Base clock: 2.30 GHz
- Turbo boost: up to 3.60 GHz
- L3 Cache: 45 MB per socket (90 MB total)
- TDP: 145W per socket

---

## Capacity Planning for Server Meshing

### Per Dedicated Server Instance (Estimated)
| Resource | Estimate |
|---|---|
| RAM | 2-6 GB (depends on loaded chunks + entity count) |
| CPU | 1-2 threads under normal load, up to 4 under heavy load (siege) |
| Network | ~50-200 Kbps per connected player |

### Maximum Instance Estimates (CelticTrinityStudiosOne)

| Scenario | RAM/Instance | Max Instances | CPU Headroom |
|---|---|---|---|
| Light (few players, small hex) | ~2 GB | ~200+ | Excellent |
| Moderate (20-30 players/hex) | ~4 GB | ~120 | Good |
| Heavy (siege, 100+ players) | ~6 GB | ~80 | Moderate |
| Worst case (massive battle) | ~8 GB | ~60 | Tight on threads |

### Reserved Resources
| Service | RAM | CPU |
|---|---|---|
| Windows Server OS | ~4 GB | 2 threads |
| PostgreSQL Database | 4-8 GB | 2-4 threads |
| Orchestrator Service | 0.5 GB | 1 thread |
| Monitoring / Logging | 1 GB | 1 thread |
| **Total Reserved** | **~10-14 GB** | **6-8 threads** |

### Net Available for Game Instances
- **RAM:** ~498-502 GB
- **CPU Threads:** ~64-66

---

## Network Specifications

| Metric | Value |
|---|---|
| **ISP** | MLConnect (Centerville, TN) |
| **Download** | 937.16 Mbps |
| **Upload** | 765.56 Mbps |
| **Ping (idle)** | 3 ms |
| **Ping (download load)** | 15 ms |
| **Ping (upload load)** | 6 ms |
| **Connection Type** | Multi |

### Player Capacity by Upload Bandwidth
Upload is the bottleneck for game servers (server → client replication).

| Per-Player Bandwidth | Max Concurrent Players | Notes |
|---|---|---|
| 100 Kbps (light, exploring) | ~7,600 | Mostly position updates |
| 150 Kbps (normal, combat) | ~5,100 | Combat + inventory replication |
| 200 Kbps (heavy, siege) | ~3,800 | Mass entity replication |
| 300 Kbps (worst case) | ~2,500 | Large-scale PvP with AOE |

### Network TODO
- [ ] Document static IP / DNS configuration
- [ ] Document firewall port ranges for server instances (7777+)
- [ ] Document DDoS mitigation strategy
- [ ] Investigate if MLConnect offers dedicated/business IP or higher upload tier

## Storage Considerations
- [ ] Document disk type (SSD/NVMe/HDD) and capacity
- [ ] Document RAID configuration if applicable
- [ ] Document backup strategy

## Future Scaling Notes
- This single machine can comfortably handle alpha and beta testing
- For production launch, consider cloud burst capacity (AWS/Azure) for peak events
- The same server meshing code runs identically on cloud VMs — only IP/port config changes
- Dual-socket Xeon with 512 GB RAM is well-suited for running many lightweight dedicated server processes
