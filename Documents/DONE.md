# Faldoran Prime — Completed Work

**Last Updated:** 2026-03-02 | Celtic Trinity Studios

---

## Infrastructure & Security
- Remote PostgreSQL DB with `character_affinities` support
- Secret loading chain: env vars → `ServerSecrets.ini` → `DefaultGame.ini`
- Credentials removed from VCS; `.gitignore` covers secrets/keys
- Iterated SHA-256 (PBKDF2-like, 10k rounds) password hashing
- 256-bit Crypto RNG salt (BCryptGenRandom / /dev/urandom fallback)
- Rate limiting: Login 5/60s, Account Creation 3/60s, 10-attempt lockout
- Input clamping at all RPC boundaries (256 char max)
- Startup Gate Subsystem (`UFPMServerStartupGateSubsystem`): Starting → Holding → Ready → Running
- DB connection locking (`FCriticalSection` in `ExecuteQuery`)
- Module-specific log categories (no more `LogTemp`)
- Consolidated `.gitignore`, collision profiles fixed in `DefaultEngine.ini`

## World Engine
- Procedural chunk system: async generation, Marching Cubes (voxel) + Heightmap
- Extreme verticality: Ridge Noise, 1km+ vertical range
- Climate engine: biome-aware height (Snow at altitude, Coast at sea level)
- Biome PCG Framework: HISM-based C++ spawner + `UFPMBiomePCGConfig` data asset
  - Slope filtering per-biome, data-driven density/scale ranges
  - Static mobility, NoCollision foliage, QueryOnly rocks
- Dynamic water: procedural river/lake carving with teal water shader
- Noise composition: Continent → Mountain → Ridge → Detail → Thermal erosion
- Biome assignment: temperature + moisture system (11 biomes)

## Character & Gameplay
- CC5 Pipeline: Reallusion Auto Setup, 240+ morph targets, 3D preview
- Species scaling (`ApplySpeciesScaling`): reads `UFPMSpeciesRegistry`, fallback inline table
  - Capsule, mesh scale, walk/run speed, jump, camera boom, default morph targets
  - Walk = 150 cm/s, Run = 500 cm/s (Left Shift)
- Interaction System: `IFPMInteractionInterface` + `UFPMInteractionComponent`, 10Hz line-trace
- Inventory System: `UFPMInventoryComponent`, server-authoritative 40-slot grid
- Interactable Resource: `AFPMInteractableResource` base class
- Animation base class: `UFPMCharacterAnimInstance`
- Login Widget: breathing glow + gold shimmer (`NativeTick` sine wave); `BackgroundTexture` property

## Database Schema
- `characters` table (base)
- `character_affinities` table with composite PK + index

---

> **Deferred (documented, not started):** Argon2id, TLS/SSL, session tokens, anti-cheat service
