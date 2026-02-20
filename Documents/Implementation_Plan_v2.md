# FaldoranPrimeMMO — Improvement Implementation Plan v2
**Created:** 2026-02-19
**Status:** In Progress

## Phase 1: Security (Highest Priority)

### 1.1 ✅ Remove Hard-Coded Database Credentials — DONE
- **Files:** `Config/DefaultGame.ini`, `Config/ServerSecrets.ini.template`, `.gitignore`,
  `Public/Core/FPMGameInstance.h`, `Private/Core/FPMGameInstance.cpp`,
  `Private/Database/FPMDatabaseSubsystem.cpp`
- **Changes:**
  - Removed real DB password `Jdsdm14e2026!` and real server IP `152.86.63.18` from VCS
  - Created `Config/ServerSecrets.ini.template` with instructions
  - Added ServerSecrets.ini, .env, .pem/.key files to .gitignore
  - DB config now loads: env vars > ServerSecrets.ini > DefaultGame.ini
  - GameInstance config uses same priority chain
- **⚠️ ACTION REQUIRED:** Rotate the exposed database password immediately

### 1.2 ✅ Replace BLAKE3 with Iterated SHA-256 — DONE
- **Files:** `Private/Account/FPMAccountSubsystem.cpp`, `Public/Account/FPMAccountSubsystem.h`
- **Changes:**
  - Replaced single-pass BLAKE3 with 10,000 iterations of SHA-256 (PBKDF2-like)
  - Added production TODO for Argon2id migration
- **Note:** Existing password hashes in DB will be invalidated (accounts need reset)

### 1.3 ✅ Replace FMath::RandRange() with Crypto RNG — DONE
- **Files:** `Private/Account/FPMAccountSubsystem.cpp`
- **Changes:**
  - Salt upgraded from 128-bit to 256-bit
  - Windows: BCryptGenRandom (FIPS-compliant CNG)
  - Non-Windows: FGuid::NewGuid() (uses /dev/urandom)
  - Fallback to FGuid if BCrypt fails

### 1.4 ✅ Add Rate Limiting for Login/Account-Creation RPCs — DONE
- **Files:** `Private/Player/FPMPlayerController.cpp`, `Public/Player/FPMPlayerController.h`
- **Changes:**
  - Login: 5 attempts per 60s sliding window
  - Account creation: 3 per 60s sliding window
  - Sliding window prunes expired timestamps automatically

### 1.5 ✅ Clamp Username/Password Sizes at RPC Boundaries — DONE
- **Files:** `Private/Player/FPMPlayerController.cpp`
- **Changes:** All string inputs clamped to 256 chars max

### 1.6 ✅ Add Per-Connection Auth Attempt Limits — DONE
- **Files:** `Private/Player/FPMPlayerController.cpp`, `Public/Player/FPMPlayerController.h`
- **Changes:**
  - Failed login counter with 10-attempt lockout
  - Resets on successful login
  - Logged warnings on lockout

## Phase 2: Build & Engine Compatibility

### 2.1 ✅ Align Targets — VERIFIED (already consistent at V6/UE5.7)

## Phase 3: Configuration Fixes

### 3.1 ✅ Fix Collision Profiles in DefaultEngine.ini — DONE
- **Files:** `Config/DefaultEngine.ini`
- **Changes:**
  - Spectator profile: Added `Response=ECR_Block` to WorldStatic channel
  - UI profile: Added `Response=ECR_Block` to Visibility channel

### 3.2 ✅ Stop Manual DefaultGame.ini Loading — DONE
- **Files:** `Private/Core/FPMGameInstance.cpp`
- **Changes:** Replaced manual FConfigCacheIni path construction with GGameIni

## Phase 4: Database & Backend

### 4.1 ✅ Add character_affinities Table to Schema — DONE
- **Files:** `Documents/Technical/Database_Schema_v1.sql`
- **Changes:** Added character_affinities table with composite PK and index

### 4.2 ✅ Add DB Connection Locking — DONE
- **Files:** `Private/Database/FPMDatabaseSubsystem.cpp`, `Public/Database/FPMDatabaseSubsystem.h`
- **Changes:**
  - Added FCriticalSection member
  - FScopeLock guard at top of ExecuteQuery

### 4.3 ✅ DB Failure/Reconnect — VERIFIED (already implemented)

## Phase 5: Code Quality

### 5.1 ✅ Replace LogTemp with Module-Specific Log Categories — DONE
- **Files:** `FPMGameInstance.cpp`, `FPMGameMode.cpp`, `FPMPlayerController.cpp`
- **Changes:** All LogTemp replaced with LogFPMGameInstance, LogFPMGameMode, LogFPMPlayerController

## Phase 6: Project Hygiene

### 6.1 ✅ Update .gitignore for Secrets — DONE
### 6.2 ✅ Create Server Secrets Config Template — DONE

## Deferred (Low Priority — documented, not implemented)

- Argon2id integration (needs libsodium)
- TLS/SSL for client-server communication
- Session tokens and anti-cheat basics
- Performance optimizations (LOD, NetCullDistance, tick reduction)
- RLPlugin error macro refactoring (third-party code)
