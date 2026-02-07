# Phase 3 — PostgreSQL + Database Subsystem

**Status:** ✅ COMPLETE  
**Date:** 2026-02-07  
**Sessions:** 1 (combined 3A + 3B)

---

## What Was Done

### Phase 3A: libpq Integration
1. Installed PostgreSQL 16.4 at `C:\Program Files\PostgreSQL\16\`
2. Created `Source/ThirdParty/libpq/` with include/, lib/, and bin/ subdirectories
3. Copied headers (`libpq-fe.h`, `postgres_ext.h`, `pg_config_ext.h`)
4. Copied `libpq.lib` and DLLs (`libpq.dll` + 4 runtime dependencies)
5. Updated `FaldoranPrimeMMO.Build.cs` with include paths, library linking, and RuntimeDependencies
6. Verified compilation and linkage on both Editor and Server targets

### Phase 3B: Database Subsystem
1. Created `UFPMDatabaseSubsystem` (GameInstanceSubsystem) with:
   - `Initialize()` / `Deinitialize()` lifecycle
   - `Connect()` — uses `PQconnectdb()` with config from `DefaultGame.ini`
   - `Disconnect()` — uses `PQfinish()`
   - `IsConnected()` — checks `PQstatus()`
   - `ExecuteQuery()` — parameterized queries via `PQexecParams()` (SQL injection safe)
   - All operations gated by `IsDedicatedServerContext()`
2. Added database config to `Config/DefaultGame.ini` under `[FPM.Database]`
3. Created database schema (`Documents/Technical/Database_Schema_v1.sql`):
   - `accounts` table (UUID PK, username, password_hash, password_salt, timestamps)
   - `characters` table (UUID PK, FK to accounts, name, appearance, timestamps)
4. Set up PostgreSQL: created `fpm_server` user and `faldoran_prime` database
5. Created `UFPMDatabaseTestCommands` with console commands:
   - `FPM.TestDBConnect` — connects and logs result
   - `FPM.TestDBWrite` — creates db_test table and inserts a row
   - `FPM.TestDBRead` — reads from db_test and logs rows
6. Removed temporary `FPMLibpqTest` files (superseded by subsystem)
7. Compiled both Editor and Server targets successfully

---

## Files Created/Modified

| File | Action |
|------|--------|
| `Source/ThirdParty/libpq/include/libpq-fe.h` | Copied from PostgreSQL |
| `Source/ThirdParty/libpq/include/postgres_ext.h` | Copied from PostgreSQL |
| `Source/ThirdParty/libpq/include/pg_config_ext.h` | Copied from PostgreSQL |
| `Source/ThirdParty/libpq/lib/libpq.lib` | Copied from PostgreSQL |
| `Source/ThirdParty/libpq/bin/libpq.dll` | Copied from PostgreSQL |
| `Source/ThirdParty/libpq/bin/libssl-3-x64.dll` | Copied (runtime dep) |
| `Source/ThirdParty/libpq/bin/libcrypto-3-x64.dll` | Copied (runtime dep) |
| `Source/ThirdParty/libpq/bin/libintl-9.dll` | Copied (runtime dep) |
| `Source/ThirdParty/libpq/bin/libiconv-2.dll` | Copied (runtime dep) |
| `Source/FaldoranPrimeMMO/FaldoranPrimeMMO.Build.cs` | Modified — libpq paths |
| `Source/FaldoranPrimeMMO/Public/Database/FPMDatabaseSubsystem.h` | New |
| `Source/FaldoranPrimeMMO/Private/Database/FPMDatabaseSubsystem.cpp` | New |
| `Source/FaldoranPrimeMMO/Public/Database/FPMDatabaseTestCommands.h` | New |
| `Source/FaldoranPrimeMMO/Private/Database/FPMDatabaseTestCommands.cpp` | New |
| `Config/DefaultGame.ini` | Modified — database config |
| `Documents/Technical/Database_Schema_v1.sql` | New |

## Database Credentials (Local Dev Only)

| Setting | Value |
|---------|-------|
| Host | localhost |
| Port | 5432 |
| Database | faldoran_prime |
| User | fpm_server |
| Password | dev_password_change_me |
| Superuser | postgres / postgres |

## How to Test

1. Launch the dedicated server: `E:\FaldoranPrimeMMO\Binaries\Win64\FaldoranPrimeMMOServer.exe L_PrototypeWorld -log`
2. In the server console, type: `FPM.TestDBConnect`
3. Verify log shows "FPM Database: Connected successfully"
4. Type: `FPM.TestDBWrite` — creates a test table and inserts a row
5. Type: `FPM.TestDBRead` — reads back the row and logs it

## Known Issues
- Stale `LiveCoding` mutex can block Server builds if server processes aren't killed first
- IDE lints about "file not found" are false positives — UBT resolves all UE include paths at compile time
