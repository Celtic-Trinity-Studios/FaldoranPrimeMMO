# Phase 5A — Character Creation Backend

**Status:** ✅ COMPLETE  
**Date:** 2026-02-08  
**Sessions:** 1

---

## What Was Done

### Step 1: Data Contracts
Created `FPMCharacterCreationDataContract.h` with:
- `EFPMCharacterCreationError` — enum with 7 error codes (None, InvalidName, NameTaken, InvalidAppearance, TooManyCharacters, RateLimited, ServerError)
- `FFPMCharacterCreationRequest` — untrusted client payload (CharacterName, BodyType, SkinTone, HairStyle, HairColor)
- `FFPMCharacterCreationResult` — server response (bSuccess, CharacterId, ErrorCode, ErrorMessage)

### Step 2: Validator
Created `FFPMCharacterCreationValidator` (plain C++ class, not UObject):
- `ValidateName()` — 3-20 chars, alphanumeric + space/hyphen/apostrophe, no leading/trailing/consecutive spaces, profanity blocklist
- `ValidateAppearance()` — body type 0-3, hair style 0-7, color RGB components in [0.0, 1.0]
- `ValidateRequest()` — calls both, returns first error (fail closed)
- All bounds are named constants (no magic numbers)

### Step 3: Subsystem
Created `UFPMCharacterCreationSubsystem` (GameInstanceSubsystem):
- `SubmitCharacterCreation(AccountId, Request)` → full server-authoritative flow:
  1. Server guard (`IsDedicatedServerContext()`)
  2. Rate limiting (5 requests/minute per account, sliding window)
  3. Input validation via `FFPMCharacterCreationValidator`
  4. Character count check (max 5 per account)
  5. Name uniqueness check (case-insensitive DB query)
  6. Insert into `characters` table with `gen_random_uuid()`
  7. Audit logging (all attempts, success and failure)
- Fail closed throughout — any uncertainty = rejection
- Handles race conditions on name uniqueness (DB unique constraint as fallback)

### Step 4: Test Commands
Created `UFPMCharacterCreationTestCommands`:
- `FPM.TestCreateCharacter <name>` — creates character with default appearance using first DB account
- Supports multi-word names (e.g., `FPM.TestCreateCharacter Jon Snow`)
- PIE-compatible (finds server world automatically)

### Step 5: Compile & Test
- Built successfully (Development Editor | Win64)
- Tested in PIE: created character "Aragorn" — verified in pgAdmin

---

## Files Created

| File | Lines | Purpose |
|------|-------|---------|
| `Public/Character/FPMCharacterCreationDataContract.h` | 103 | Structs + enum (no .cpp needed) |
| `Public/Character/FPMCharacterCreationValidator.h` | 89 | Validator header |
| `Private/Character/FPMCharacterCreationValidator.cpp` | 138 | Validator implementation |
| `Public/Character/FPMCharacterCreationSubsystem.h` | 140 | Subsystem header |
| `Private/Character/FPMCharacterCreationSubsystem.cpp` | 295 | Subsystem implementation |
| `Public/Character/FPMCharacterCreationTestCommands.h` | 41 | Test command header |
| `Private/Character/FPMCharacterCreationTestCommands.cpp` | 166 | Test command implementation |

All files under 500-line limit. All `.generated.h` includes at bottom of headers.

## Security Features

- ✅ Server authority — all operations gated by `IsDedicatedServerContext()`
- ✅ Input validation — name length/characters/profanity, appearance bounds, color ranges
- ✅ Rate limiting — 5 requests/minute per account (sliding window)
- ✅ Character count limit — max 5 per account
- ✅ Name uniqueness — DB check + unique constraint (handles race conditions)
- ✅ Fail closed — any uncertainty results in rejection
- ✅ Audit logging — all attempts logged with account, name, and result
- ✅ Parameterized queries — SQL injection prevention
- ✅ No magic numbers — all validation bounds are named constants

## How to Test

1. Launch PIE with dedicated server (Play → Multiplayer Options → Run Dedicated Server)
2. Log in with an existing account (or create one: `FPM.CreateAccount <user> <pass>`)
3. In console: `FPM.TestCreateCharacter Aragorn`
4. Check log for `LogFPMCharacterCreation` entries
5. Verify in pgAdmin: `SELECT * FROM characters;`

## Database Schema Used

Uses existing `characters` table from `Database_Schema_v1.sql`:
- `character_id` UUID PK
- `account_id` UUID FK → accounts
- `character_name` VARCHAR(20) UNIQUE
- `body_type` SMALLINT
- `skin_color_r/g/b` REAL
- `hair_style` SMALLINT
- `hair_color_r/g/b` REAL
- `created_at` / `last_played` TIMESTAMPTZ

---

*Copyright Celtic Trinity Studios, 2026. All Rights Reserved.*
