# Pillar 02 — PostgreSQL Remote Server Setup & Code Migration

**Created:** 2026-02-09  
**Status:** ✅ COMPLETE — 2026-02-09  
**Pillar:** 02 — Remote Database Migration  
**Phases:** 2AB (Server Setup) + 2DE (Code Migration)  
**Prerequisite:** Prototype Phases 0–6 complete  
**Skipped:** Pillar 1 (deferred until CC5 integration)

---

## Phase 2AB — PostgreSQL Server Installation & Configuration

### Step 1: Download & Install PostgreSQL 16+

1. On your **dedicated server machine**, open a browser and go to:  
   👉 https://www.postgresql.org/download/windows/
2. Click **"Download the installer"** → Choose **PostgreSQL 16.x** (or latest 16+) for **Windows x86-64**
3. Run the installer. During installation:
   - **Installation Directory:** Accept default (`C:\Program Files\PostgreSQL\16`) or customize
   - **Data Directory:** Note this path — you'll need it later (default: `C:\Program Files\PostgreSQL\16\data`)
   - **Password:** Set a strong password for the `postgres` superuser. **WRITE THIS DOWN.**
   - **Port:** `5432` (default)
   - **Locale:** Default
   - **Components:** Select all (PostgreSQL Server, pgAdmin 4, Stack Builder, Command Line Tools)
4. Finish the installer. Uncheck "Launch Stack Builder" — you don't need it.

### Step 2: Verify PostgreSQL Is Running

Open **PowerShell** on the server machine and run:

```powershell
# Check if the PostgreSQL service is running
Get-Service -Name "postgresql*"

# If it says "Running", you're good. If not:
Start-Service -Name "postgresql-x64-16"
```

Also test with `pg_isready`:

```powershell
& "C:\Program Files\PostgreSQL\16\bin\pg_isready.exe"
```

Expected output: `/tmp:5432 - accepting connections` (or similar)

### Step 3: Configure for Remote Access — postgresql.conf

1. Open the PostgreSQL config file in a text editor **as Administrator**:

```
C:\Program Files\PostgreSQL\16\data\postgresql.conf
```

2. Find this line (around line 60):
```
#listen_addresses = 'localhost'
```

3. Change it to:
```
listen_addresses = '*'
```

> This tells PostgreSQL to accept connections on **all** network interfaces, not just localhost.
> For tighter security, you can set this to `'localhost,YOUR_DEV_MACHINE_IP'` instead of `'*'`.

4. While you're here, also verify these settings (they should be defaults):
```
port = 5432
max_connections = 100
```

5. **Save the file.**

### Step 4: Configure Client Authentication — pg_hba.conf

1. Open the HBA (Host-Based Authentication) file **as Administrator**:

```
C:\Program Files\PostgreSQL\16\data\pg_hba.conf
```

2. Scroll to the bottom. You'll see existing rules. **Add these lines at the end:**

```
# Allow dev machine to connect to all databases with password auth
# Replace YOUR_DEV_IP with your dev machine's actual LAN IP (e.g., 192.168.1.100)
host    all    all    YOUR_DEV_IP/32    scram-sha-256

# If you want to allow your entire LAN subnet (less secure, but convenient):
# host    all    all    192.168.1.0/24    scram-sha-256
```

> **How to find your dev machine's IP:** On your dev machine, run `ipconfig` in PowerShell and look for `IPv4 Address` under your active adapter.

3. **Save the file.**

### Step 5: Open Windows Firewall Port 5432

On the **server machine**, open PowerShell **as Administrator** and run:

```powershell
New-NetFirewallRule -DisplayName "PostgreSQL 5432" -Direction Inbound -Protocol TCP -LocalPort 5432 -Action Allow -Profile Private
```

> This opens port 5432 for inbound TCP connections on the Private network profile.
> If your network is classified as "Public" instead of "Private", add `-Profile Any` or change the profile.

To verify the rule was created:
```powershell
Get-NetFirewallRule -DisplayName "PostgreSQL 5432" | Format-List
```

### Step 6: Restart PostgreSQL Service

The config changes require a service restart:

```powershell
Restart-Service -Name "postgresql-x64-16"
```

Verify it's back up:
```powershell
& "C:\Program Files\PostgreSQL\16\bin\pg_isready.exe"
```

### Step 7: Create Database and User

On the **server machine**, open PowerShell and connect to PostgreSQL as the superuser:

```powershell
& "C:\Program Files\PostgreSQL\16\bin\psql.exe" -U postgres
```

Enter the `postgres` superuser password you set during install.

Then run these SQL commands:

```sql
-- Create the application user with a STRONG password
CREATE USER fpm_server WITH PASSWORD 'YOUR_SECURE_PASSWORD_HERE';

-- Create the database owned by the app user
CREATE DATABASE faldoran_prime OWNER fpm_server;

-- Connect to the new database
\c faldoran_prime

-- Grant the app user full privileges on the database
GRANT ALL PRIVILEGES ON DATABASE faldoran_prime TO fpm_server;
```

> ⚠️ **Choose a real, strong password.** Not `dev_password_change_me`. Use something like a 20+ character passphrase with mixed case, numbers, and symbols.

### Step 8: Apply the Database Schema

Still connected via `psql` (now to the `faldoran_prime` database), run the existing schema.

Option A — Run the SQL file directly:
```powershell
& "C:\Program Files\PostgreSQL\16\bin\psql.exe" -U fpm_server -d faldoran_prime -f "E:\FaldoranPrimeMMO\Documents\Technical\Database_Schema_v1.sql"
```

Option B — Copy/paste the SQL into pgAdmin or psql:
```sql
-- Enable UUID generation
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- Accounts Table
CREATE TABLE IF NOT EXISTS accounts (
    account_id      UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username        VARCHAR(20) NOT NULL UNIQUE,
    password_hash   TEXT NOT NULL,
    password_salt   TEXT NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login      TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts (username);

-- Characters Table
CREATE TABLE IF NOT EXISTS characters (
    character_id    UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    account_id      UUID NOT NULL REFERENCES accounts(account_id) ON DELETE CASCADE,
    character_name  VARCHAR(20) NOT NULL UNIQUE,
    body_type       SMALLINT NOT NULL DEFAULT 0,
    skin_color_r    REAL NOT NULL DEFAULT 0.8,
    skin_color_g    REAL NOT NULL DEFAULT 0.6,
    skin_color_b    REAL NOT NULL DEFAULT 0.5,
    hair_style      SMALLINT NOT NULL DEFAULT 0,
    hair_color_r    REAL NOT NULL DEFAULT 0.3,
    hair_color_g    REAL NOT NULL DEFAULT 0.2,
    hair_color_b    REAL NOT NULL DEFAULT 0.1,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_played     TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_characters_account_id ON characters (account_id);
CREATE INDEX IF NOT EXISTS idx_characters_name ON characters (character_name);
```

### Step 9: Create the character_affinities Table

Run this SQL in the same `faldoran_prime` database (from Pillar 1D, applied early):

```sql
-- Affinities table (covers both Playstyle and Magical pools)
-- affinity_pool: 0 = Playstyle, 1 = Magical
-- affinity_type: index within the pool (e.g., 0=Martial, 1=Ranged, ...)
CREATE TABLE character_affinities (
    character_id  UUID REFERENCES characters(character_id) ON DELETE CASCADE,
    affinity_pool SMALLINT NOT NULL,
    affinity_type SMALLINT NOT NULL,
    points        SMALLINT NOT NULL DEFAULT 0,
    PRIMARY KEY (character_id, affinity_pool, affinity_type)
);
```

### Step 10: Verify Remote Connection from Dev Machine

On your **dev machine**, open PowerShell and run:

```powershell
# Replace SERVER_IP with your dedicated server's LAN IP
& "C:\Program Files\PostgreSQL\16\bin\psql.exe" -h SERVER_IP -U fpm_server -d faldoran_prime
```

Enter the `fpm_server` password. If you see the `faldoran_prime=>` prompt, **you're connected remotely!** 🎉

Verify the tables exist:
```sql
\dt
```

Expected output:
```
              List of relations
 Schema |         Name          | Type  |   Owner
--------+-----------------------+-------+------------
 public | accounts              | table | fpm_server
 public | character_affinities  | table | fpm_server
 public | characters            | table | fpm_server
```

Type `\q` to exit.

---

## Troubleshooting

### "Connection refused" (port/firewall issue)

1. **Is PostgreSQL running?** On the server, run `Get-Service postgresql*`
2. **Is it listening on the right address?** Run on the server:
   ```powershell
   & "C:\Program Files\PostgreSQL\16\bin\psql.exe" -U postgres -c "SHOW listen_addresses;"
   ```
   Should show `*` or your dev IP.
3. **Is the firewall rule active?**
   ```powershell
   Get-NetFirewallRule -DisplayName "PostgreSQL 5432"
   ```
4. **Can you even reach the server?** From your dev machine:
   ```powershell
   Test-NetConnection -ComputerName SERVER_IP -Port 5432
   ```
   `TcpTestSucceeded` should be `True`.
5. **Did you restart PostgreSQL after editing configs?** Changes to `postgresql.conf` and `pg_hba.conf` require a service restart.

### "FATAL: no pg_hba.conf entry for host" (auth config issue)

- Your dev machine's IP isn't in `pg_hba.conf`, or the subnet mask is wrong.
- Check your dev machine's actual IP with `ipconfig`.
- Make sure you're using the right format: `host all all 192.168.1.100/32 scram-sha-256`
- Restart PostgreSQL after editing `pg_hba.conf`.

### "FATAL: password authentication failed"

- Double-check the password. PostgreSQL is case-sensitive.
- Try resetting the password:
  ```sql
  -- As postgres superuser on the server:
  ALTER USER fpm_server WITH PASSWORD 'new_password_here';
  ```
- Make sure you're connecting as `fpm_server`, not `postgres`.

### "FATAL: database does not exist"

- Check the database name spelling. It's `faldoran_prime` (all lowercase).
- Verify it exists: `\l` in psql lists all databases.

### "CREATE EXTENSION pgcrypto failed"

- The `pgcrypto` extension requires the `postgres` superuser to install it.
- Connect as `postgres` first, then run `CREATE EXTENSION IF NOT EXISTS "pgcrypto";` in the `faldoran_prime` database.
- After that, reconnect as `fpm_server`.

### PostgreSQL service won't start after config changes

- You probably have a syntax error in `postgresql.conf` or `pg_hba.conf`.
- Check the PostgreSQL log file at:
  ```
  C:\Program Files\PostgreSQL\16\data\log\
  ```
- Common mistake: missing quotes around `listen_addresses` value.

---

## Phase 2DE — Code Migration (Retry Logic & Remote Connection Support)

See the code changes implemented alongside this document.

### Changes Made:
1. **Config/DefaultGame.ini** — Updated Host to server IP, updated password
2. **UFPMDatabaseSubsystem** — Added connection retry logic (3 attempts, 2s backoff), `IsConnectionHealthy()`, auto-reconnect on query failure
3. **FPM.TestRemoteDBConnect** — New console command to verify remote connectivity

### UE Locations Table

| What | Where |
|------|-------|
| Database config | `Config/DefaultGame.ini` → `[FPM.Database]` |
| Database subsystem header | `Source/FaldoranPrimeMMO/Public/Database/FPMDatabaseSubsystem.h` |
| Database subsystem impl | `Source/FaldoranPrimeMMO/Private/Database/FPMDatabaseSubsystem.cpp` |
| Test commands header | `Source/FaldoranPrimeMMO/Public/Database/FPMDatabaseTestCommands.h` |
| Test commands impl | `Source/FaldoranPrimeMMO/Private/Database/FPMDatabaseTestCommands.cpp` |
| PostgreSQL config (server) | `C:\Program Files\PostgreSQL\16\data\postgresql.conf` |
| PostgreSQL HBA (server) | `C:\Program Files\PostgreSQL\16\data\pg_hba.conf` |
| Schema SQL | `Documents/Technical/Database_Schema_v1.sql` |
| Console command (PIE) | `~` key → type `FPM.TestRemoteDBConnect` |

### How to Test
1. Launch PIE with **Dedicated Server** (PIE → Advanced Settings → Net Mode → Play As Client w/ Dedicated Server)
2. Open console (`~` key) and type: `FPM.TestRemoteDBConnect`
3. Check Output Log for connection status
4. Try creating an account and character — verify data in pgAdmin on the server
5. **Connection recovery test:** Stop PostgreSQL on the server → wait → restart → try an operation → verify auto-reconnect

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
