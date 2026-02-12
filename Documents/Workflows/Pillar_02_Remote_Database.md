# Pillar 02 — Remote Database Migration

**Goal:** Move PostgreSQL from localhost to your dedicated server machine so multiple machines can connect and you're no longer tied to the dev PC.

**Status:** ✅ COMPLETE (See [02_PostgreSQL_Setup.md](file:///e:/FaldoranPrimeMMO/Documents/Workflows/Archive/02_PostgreSQL_Setup.md))

---

## Phase 2A: Install PostgreSQL on the Dedicated Server

1. Download and install PostgreSQL 16+ on the server machine
2. During install, note the `data` directory path and set the `postgres` superuser password
3. Verify the service is running: `pg_isready` from the server's command line

---

## Phase 2B: Configure for Remote Access

1. Edit `postgresql.conf` → set `listen_addresses = '*'` (or your dev machine's IP)
2. Edit `pg_hba.conf` → add a line allowing your dev machine's IP to connect:
   ```
   host  all  all  YOUR_DEV_IP/32  scram-sha-256
   ```
3. Open port `5432` in the server's Windows Firewall (or Linux equivalent)
4. Restart PostgreSQL service

---

## Phase 2C: Create Database and Schema

1. Connect via `psql` on the server: `psql -U postgres`
2. Create the user and database:
   ```sql
   CREATE USER fpm_server WITH PASSWORD 'your_secure_password';
   CREATE DATABASE faldoran_prime OWNER fpm_server;
   ```
3. Run the existing schema from [Database_Schema_v1.sql](file:///e:/FaldoranPrimeMMO/Documents/Technical/Database_Schema_v1.sql)
4. Run the new `character_affinities` table creation (from Pillar 1D)

---

## Phase 2D: Code Changes (Minimal)

The prototype was designed for this — it's mostly a config change:
```ini
[FPM.Database]
Host=YOUR_SERVER_IP
Port=5432
DatabaseName=faldoran_prime
Username=fpm_server
Password=your_secure_password
```

Additional code changes:
- Add connection retry logic (remote connections can drop temporarily)
- Add connection timeout configuration
- Optional: Add SSL mode to the `PQconnectdb()` connection string for encrypted traffic

---

## Phase 2E: Security

- Firewall: only allow connections from known IPs on port 5432
- Use a strong password (not `dev_password_change_me`)
- Enable PostgreSQL logging for audit trail
- If the server is on your LAN, LAN-only access is sufficient for now; for WAN access, consider an SSH tunnel

---

## Agent Prompts — Pillar 2

### Phase 2AB --- 04. PostgreSQL Server Setup
```
CONVERSATION TITLE: Pillar 02, Phase 2AB --- 04. PostgreSQL Server Setup

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Pillar_02_Remote_Database.md for the current plan.

TASK: Walk me through installing and configuring PostgreSQL 16 on my dedicated server machine for remote access.

This is a setup/ops task, not a code task. My dedicated server machine needs:
1. PostgreSQL 16+ installed
2. Configured for remote connections (listen_addresses, pg_hba.conf)
3. Windows Firewall port 5432 opened
4. Database "faldoran_prime" created
5. User "fpm_server" created with appropriate permissions
6. Schema from Documents/Technical/Database_Schema_v1.sql applied
7. New character_affinities table created (see Pillar_01_Character_Creation.md Phase 1D)
8. Verification that I can connect from my dev machine using psql

Provide step-by-step instructions. Include the exact commands and config file edits.
Include troubleshooting steps for common issues (connection refused, auth failed, etc.).
```

### Phase 2DE --- 05. Code Migration
```
CONVERSATION TITLE: Pillar 02, Phase 2DE --- 05. Code Migration

Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Pillar_02_Remote_Database.md for the current plan.

TASK: Update UFPMDatabaseSubsystem to support remote PostgreSQL connections with retry logic and timeout.

Prerequisites: PostgreSQL is installed and accessible on the dedicated server. Phase 2AB is complete.

Do this in micro-steps, one at a time, each must compile:
1. Update Config/DefaultGame.ini:
   - Change Host from localhost to the server IP
   - Update password to the new secure password
2. Update UFPMDatabaseSubsystem:
   - Add connection timeout parameter to PQconnectdb() connection string (connect_timeout=10)
   - Add retry logic: if connection fails, retry up to 3 times with 2-second backoff
   - Add IsConnectionHealthy() check using PQstatus()
   - Add auto-reconnect if connection is lost during gameplay
   - Optional: add sslmode=prefer to connection string
3. Add console command: FPM.TestRemoteDBConnect — verifies remote connection
4. Compile and test:
   - Launch PIE with dedicated server
   - Verify connection to remote PostgreSQL
   - Test creating an account and character (data appears in remote DB)
   - Test connection recovery (manually restart PostgreSQL, verify reconnect)

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines.
```
