# Prototype Implementation Plan — Minimum Viable MMO

**Goal:** Get a client connecting to a dedicated server with database persistence as fast as possible.  
**Created:** 2026-02-07  
**Status:** 📋 PLANNED

> ⚠️ **MMO Security Reminder:** This is a Massively Multiplayer Online game. Even at the prototype stage, every system follows server-authority principles: never trust client input, validate everything server-side, rate-limit all RPCs, and audit all state changes. See `Documents/Design/00_Rules_and_Constraints.md` §MMO Security Mandate for the full policy. Prototype security measures are a **subset** of production requirements — they are not skipped, just scoped appropriately.

---

## What "Prototype Done" Looks Like

A player can:
1. Launch the client
2. Connect to a dedicated server
3. Create an account (username/password)
4. Create a character (name + basic appearance)
5. Character is saved to a database
6. Select that character and spawn into a world
7. See other connected players moving around
8. Disconnect and reconnect — character persists

**That's it.** No combat, no inventory, no crafting. Just proof that the MMO spine works.

---

## Tech Stack

| Component | Choice | Why |
|-----------|--------|-----|
| **Engine** | UE 5.7.1 (Custom Compile) at `E:\UEInstalled\Windows` | Already installed |
| **Server** | UE Dedicated Server target | Built into engine, handles replication |
| **Database** | PostgreSQL 16+ | Industry standard for MMOs, free, ACID, scales to production |
| **DB Driver** | libpq (C API) wrapped in a UE subsystem | Direct connection, no middleware for prototype |
| **Local Dev** | PostgreSQL running locally (or Docker) | Zero cloud cost during development. To go remote: change `Host=` in `DefaultGame.ini`. Code doesn't change. |

---

## Phase Map (7 Phases, ~9-11 sessions)

**Session Definition:** 1 session ≈ **2–3 hours** of focused work.  
**Total Estimated Time:** ~20–30 hours to reach a working prototype.

```
Phase 0: Verify Project Builds          ← ~30 min
Phase 1: Dedicated Server Launches       ← ~1 session
Phase 2: Client Connects to Server       ← ~1 session
Phase 3: PostgreSQL + DB Subsystem       ← ~2 sessions
Phase 4: Account System (Login)          ← ~2 sessions
Phase 5: Character Create + Persist      ← ~2 sessions
Phase 6: Character Select + Spawn        ← ~1-2 sessions
Phase 7: Gameplay Prototypes (Nodes/Caravans) ← ~2 sessions
```

---

## Phase 0: Verify Project Builds

### Goal
Confirm the UE project compiles for Editor, Server, and Client targets.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Open project | Double-click `E:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject` |
| Generate VS files | Right-click `.uproject` → **"Generate Visual Studio project files"** |
| Open in VS | Double-click `FaldoranPrimeMMO.sln` in the project root |
| Change build target | Visual Studio toolbar → **Solution Configuration** dropdown (top bar). Options: `Development Editor`, `Development Server`, `Development Client` |
| Change platform | Visual Studio toolbar → **Solution Platform** dropdown → `Win64` |
| Build | Visual Studio menu bar → **Build** → **Build Solution** (or `Ctrl+Shift+B`) |
| Output log | Visual Studio → **View** → **Output** (bottom pane, shows compile errors) |
| Launch editor | After building Editor target, press **F5** in VS or launch from `.uproject` |

### Deliverables
- [ ] Editor launches without errors
- [ ] Server target compiles (`Development Server | Win64`)
- [ ] Client target compiles (`Development Client | Win64`)

### Agent Prompt
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 0 — Verify the FaldoranPrimeMMO Unreal Engine 5.7.1 project builds.

This is a custom UE 5.7.1 compile installed at E:\UEInstalled\Windows.
The project is at E:\FaldoranPrimeMMO\FaldoranPrimeMMO.uproject.

Do this in micro-steps, one at a time, each must compile:
0. Go through all the setup steps for unreal engine 5.7.1, saving the project in E:\FaldoranPrimeMMO
1. Generate Visual Studio project files from the .uproject
2. Open the .sln and build Development Editor | Win64 — fix any errors
3. Build Development Server | Win64 — fix any errors
4. Build Development Client | Win64 — fix any errors
5. Verify the editor launches from the .uproject without errors

Report what you find. Do not create any new files unless fixing a build error requires it.
Follow all rules in 00_Rules_and_Constraints.md (especially: no invention, micro-steps, each step must compile, Visual Studio only).
```

---

## Phase 1: Dedicated Server Launches

### Goal
A UE dedicated server starts, loads a map, and waits for connections.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Create C++ class | UE Editor → **Tools** menu → **New C++ Class** → choose parent class → set path |
| GameMode parent class | When creating: select **Game Mode Base** as parent |
| GameInstance parent class | When creating: select **Game Instance** as parent |
| Create a new map | **Content Browser** (bottom panel) → right-click in `Content/Maps/` → **Level** |
| Set default map | UE Editor → **Edit** → **Project Settings** → **Maps & Modes** → **Default Maps** section → **Game Default Map** |
| Set default GameMode | UE Editor → **Edit** → **Project Settings** → **Maps & Modes** → **Default Modes** section → **Default GameMode** |
| Set default GameInstance | UE Editor → **Edit** → **Project Settings** → **Maps & Modes** → **Game Instance Class** |
| Config files | `E:\FaldoranPrimeMMO\Config\` folder → `DefaultEngine.ini`, `DefaultGame.ini` |
| Server launch (cmd) | `E:\FaldoranPrimeMMO\Binaries\Win64\FaldoranPrimeMMOServer.exe L_PrototypeWorld -log` |
| Output log in editor | UE Editor → **Window** → **Developer Tools** → **Output Log** |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMGameMode.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Core/` and `Private/Core/` | `AGameModeBase` |
| `FPMGameInstance.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Core/` and `Private/Core/` | `UGameInstance` |
| `L_PrototypeWorld` | `Content/Maps/` | Level asset |

### Agent Prompt
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 1 — Create the minimum classes needed for a dedicated server to launch, load a map, and wait for connections.

Do this in micro-steps, one at a time, each must compile:
1. Create AFPMGameMode (inherits AGameModeBase) in Source/FaldoranPrimeMMO/Public/Core/ and Private/Core/
   - Override InitGame() to log "FPM: Server started, waiting for connections"
   - Set DefaultPawnClass to ADefaultPawn (placeholder)
2. Create UFPMGameInstance (inherits UGameInstance) in Source/FaldoranPrimeMMO/Public/Core/ and Private/Core/
   - Override Init() to log "FPM: GameInstance initialized"
3. Compile and verify no errors
4. Tell me the exact steps to: create a flat test map called L_PrototypeWorld in Content/Maps/, set AFPMGameMode as the default GameMode, set UFPMGameInstance as the default GameInstance (via Edit → Project Settings → Maps & Modes)
5. Tell me how to launch the dedicated server from command line and verify it's running

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. .generated.h at bottom of includes. Always use absolute file paths.
```

---

## Phase 2: Client Connects to Server

### Goal
A client connects to the dedicated server, spawns a default pawn, and two players see each other.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Create PlayerController class | **Tools** → **New C++ Class** → Parent: **Player Controller** |
| Set PlayerController in GameMode | In `AFPMGameMode` constructor: `PlayerControllerClass = AFPMPlayerController::StaticClass();` |
| Open console in-game | Press **`** (grave/tilde key) during Play |
| Connect to server (console) | In client console: `open 127.0.0.1:7777` |
| Play In Editor (PIE) | UE Editor → **Play** button (top toolbar) → dropdown arrow → **Advanced Settings** for dedicated server options |
| PIE with dedicated server | **Play** dropdown → **Multiplayer Options** → check **Run Dedicated Server**, set **Number of Players** to 2+ |
| Net mode settings | UE Editor → **Play** dropdown → **Multiplayer Options** → **Net Mode** → **Play As Client** |
| Launch standalone | **Play** dropdown → **Standalone Game** |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMPlayerController.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Player/` and `Private/Player/` | `APlayerController` |

### Agent Prompt
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 2 — Create a PlayerController so clients can connect to the dedicated server and see each other.

Prerequisites: Phase 0 and Phase 1 are complete. AFPMGameMode and UFPMGameInstance exist in Source/FaldoranPrimeMMO/Public/Core/ and Private/Core/.

Do this in micro-steps, one at a time, each must compile:
1. Create AFPMPlayerController (inherits APlayerController) in Source/FaldoranPrimeMMO/Public/Player/ and Private/Player/
   - Log "FPM: Player connected" on BeginPlay (server-side only, use HasAuthority())
   - Set bShowMouseCursor = true in constructor
2. Update AFPMGameMode constructor to set PlayerControllerClass = AFPMPlayerController::StaticClass()
3. Compile and verify
4. Walk me through testing with PIE: how to enable "Run Dedicated Server" in the Play dropdown → Multiplayer Options, set 2 players, and verify both clients can see each other's pawns
5. Also tell me how to test with a standalone server + standalone client using the console command "open 127.0.0.1:7777"

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. .generated.h at bottom of includes.
```

---

## Phase 3: PostgreSQL + Database Subsystem

### Goal
UE server can read/write to a PostgreSQL database.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Edit Build.cs | `E:\FaldoranPrimeMMO\Source\FaldoranPrimeMMO\FaldoranPrimeMMO.Build.cs` |
| Add third-party libs | Create folder: `E:\FaldoranPrimeMMO\Source\ThirdParty\libpq\` with `include/`, `lib/`, `bin/` subdirs |
| Create GameInstance Subsystem | **Tools** → **New C++ Class** → search for **Subsystem** → choose **Game Instance Subsystem** as parent |
| Console commands | UE Editor → **Output Log** (Window → Developer Tools → Output Log) → type commands in the input box at bottom |
| Config files | `E:\FaldoranPrimeMMO\Config\DefaultGame.ini` |

### External Tools
| Tool | Where to Get It | Purpose |
|------|----------------|---------|
| PostgreSQL 16+ | https://www.postgresql.org/download/windows/ | Database server |
| pgAdmin 4 | Bundled with PostgreSQL installer | GUI for managing database |
| psql | Bundled with PostgreSQL installer, in `C:\Program Files\PostgreSQL\16\bin\` | Command-line SQL |
| libpq headers + libs | Inside PostgreSQL install: `C:\Program Files\PostgreSQL\16\include\` and `C:\Program Files\PostgreSQL\16\lib\` | C API for connecting to PostgreSQL from UE |
| libpq.dll | `C:\Program Files\PostgreSQL\16\bin\libpq.dll` | Must be copied to UE binaries for runtime |

### Database Setup
| Action | Where / How |
|--------|-------------|
| Launch pgAdmin | Windows Start Menu → **pgAdmin 4** |
| Create database | pgAdmin → right-click **Databases** → **Create** → **Database** → name: `faldoran_prime` |
| Create user | pgAdmin → right-click **Login/Group Roles** → **Create** → name: `fpm_server`, set password |
| Run SQL | pgAdmin → click `faldoran_prime` → **Tools** → **Query Tool** → paste SQL → click **Execute** (▶ button) |
| Test connection | Open `psql` (Start Menu → SQL Shell) → enter: `host=localhost`, `database=faldoran_prime`, `user=fpm_server` |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMDatabaseSubsystem.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Database/` and `Private/Database/` | `UGameInstanceSubsystem` |

### Agent Prompt (Session 1 of 2: libpq integration)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 3A — Integrate the PostgreSQL C library (libpq) into the Unreal Engine 5.7.1 project so we can connect to a database from the dedicated server.

Prerequisites: Phases 0-2 are complete. PostgreSQL 16+ is installed locally at C:\Program Files\PostgreSQL\16\.

Do this in micro-steps, one at a time, each must compile:
1. Create the folder structure: Source/ThirdParty/libpq/ with include/, lib/, and bin/ subdirectories
2. Copy the needed files from the PostgreSQL installation:
   - Headers from C:\Program Files\PostgreSQL\16\include\ (specifically libpq-fe.h and its dependencies) → Source/ThirdParty/libpq/include/
   - Library from C:\Program Files\PostgreSQL\16\lib\libpq.lib → Source/ThirdParty/libpq/lib/
   - DLL from C:\Program Files\PostgreSQL\16\bin\libpq.dll → Source/ThirdParty/libpq/bin/
3. Update FaldoranPrimeMMO.Build.cs to:
   - Add the include path for libpq headers
   - Add the library path and link libpq.lib
   - Add a RuntimeDependency or PostBuildStep to copy libpq.dll next to the executable
4. Create a minimal test: #include "libpq-fe.h" in a .cpp file and call PQlibVersion() to verify linkage
5. Compile and verify no linker errors

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Use Visual Studio to compile. Provide absolute file paths for everything.
```

### Agent Prompt (Session 2 of 2: Database Subsystem)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 3B — Create the UFPMDatabaseSubsystem that connects to PostgreSQL and can execute queries.

Prerequisites: Phases 0-2 complete. libpq is integrated into the project (Phase 3A). PostgreSQL is running locally with database "faldoran_prime" and user "fpm_server".

Do this in micro-steps, one at a time, each must compile:
1. Create UFPMDatabaseSubsystem (inherits UGameInstanceSubsystem) in Source/FaldoranPrimeMMO/Public/Database/ and Private/Database/
   - Initialize(): Read DB config from DefaultGame.ini (host, port, dbname, user, password)
   - Connect(): Use PQconnectdb() to establish connection. Log success/failure.
   - Disconnect(): Use PQfinish() in Deinitialize()
   - IsConnected(): Return connection status
   - ExecuteQuery(FString SQL, TArray<FString> Params): Execute parameterized query, return results
   - All database operations must happen on the server only (check IsRunningDedicatedServer())
2. Add config section to Config/DefaultGame.ini:
   [FPM.Database]
   Host=localhost
   Port=5432
   DatabaseName=faldoran_prime
   Username=fpm_server
   Password=dev_password_change_me
3. Create the initial database schema (provide the SQL for me to run in pgAdmin):
   - accounts table (account_id UUID PK, username VARCHAR UNIQUE, password_hash, created_at, last_login)
   - characters table (character_id UUID PK, account_id FK, character_name VARCHAR UNIQUE, body_type, skin colors, hair_style, hair colors, created_at, last_played)
4. Add console commands for testing:
   - FPM.TestDBConnect — attempts connection and logs result
   - FPM.TestDBWrite — inserts a test row into accounts
   - FPM.TestDBRead — reads it back and logs it
5. Compile and verify

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. .generated.h at bottom. Server-only operations (never trust client). Credentials never exposed to client builds.
```

---

## Phase 4: Account System (Login)

### Goal
Client can create an account and log in; server validates credentials against the database.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Create UMG Widget Blueprint | **Content Browser** → right-click in `Content/UI/` → **User Interface** → **Widget Blueprint** |
| Reparent widget to C++ | Open Widget Blueprint → **Graph** tab → **Class Settings** → **Parent Class** → search for your C++ class |
| Create Widget C++ class | **Tools** → **New C++ Class** → parent: **User Widget** |
| Add UMG module | In `Build.cs` → add `"UMG"` to PublicDependencyModuleNames (should already be there) |
| Show widget in game | Level Blueprint or PlayerController: `CreateWidget()` → `AddToViewport()` |
| Set input mode UI | In PlayerController: `SetInputMode(FInputModeUIOnly)` + `SetShowMouseCursor(true)` |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMAccountSubsystem.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Account/` and `Private/Account/` | `UGameInstanceSubsystem` |
| `FPMLoginWidget.h/.cpp` | `Source/FaldoranPrimeMMO/Public/UI/` and `Private/UI/` | `UUserWidget` |
| `WBP_LoginScreen` | `Content/UI/` (Widget Blueprint, reparented to FPMLoginWidget) | FPMLoginWidget |

### Agent Prompt (Session 1 of 2: Account Subsystem)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 4A — Create the UFPMAccountSubsystem that handles account creation and login with password hashing.

Prerequisites: Phases 0-3 complete. UFPMDatabaseSubsystem exists and can execute queries against PostgreSQL.

Do this in micro-steps, one at a time, each must compile:
1. Create UFPMAccountSubsystem (inherits UGameInstanceSubsystem) in Source/FaldoranPrimeMMO/Public/Account/ and Private/Account/
   - CreateAccount(FString Username, FString Password) → returns success/error
     - Validates username (3-20 chars, alphanumeric)
     - Hashes password with SHA-256 + random salt (use FPlatformMisc and FSHA256 from UE)
     - Inserts into accounts table via UFPMDatabaseSubsystem
     - Handles duplicate username error gracefully
   - Login(FString Username, FString Password) → returns AccountId or error
     - Queries accounts table for username
     - Verifies password hash matches
     - Updates last_login timestamp
     - Returns the account_id UUID on success
   - All operations server-side only
2. Create data structs:
   - FFPMLoginRequest (Username, Password)
   - FFPMLoginResult (Success bool, AccountId FGuid, ErrorMessage FString)
3. Add console commands for testing:
   - FPM.CreateAccount "username" "password"
   - FPM.Login "username" "password"
4. Compile and test via console commands

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Server authority — password hashing is ALWAYS server-side. Never store plaintext passwords. Never trust client input.
```

### Agent Prompt (Session 2 of 2: Login UI + RPC)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 4B — Create the login UI widget and wire it to the Account Subsystem via RPC.

Prerequisites: Phases 0-3 complete. UFPMAccountSubsystem exists with CreateAccount() and Login() methods.

Do this in micro-steps, one at a time, each must compile:
1. Create UFPMLoginWidget (inherits UUserWidget) in Source/FaldoranPrimeMMO/Public/UI/ and Private/UI/
   Required BindWidgets (Blueprint must have these exact names):
   - UsernameInput (EditableTextBox)
   - PasswordInput (EditableTextBox)
   - LoginButton (Button)
   - CreateAccountButton (Button)
   - ResultText (TextBlock)
   - NativeConstruct() — bind button click events
2. Create Server RPC on AFPMPlayerController:
   - UFUNCTION(Server, Reliable) ServerRequestLogin(FFPMLoginRequest Request)
   - UFUNCTION(Client, Reliable) ClientReceiveLoginResult(FFPMLoginResult Result)
   - Server RPC calls UFPMAccountSubsystem::Login(), sends result back via Client RPC
   - Same pattern for CreateAccount
3. Update AFPMPlayerController:
   - On BeginPlay (client): show WBP_LoginScreen widget
   - On successful login: hide login widget, transition to character select (placeholder log for now)
   - Store authenticated AccountId in a server-side variable (NOT replicated to other clients)
4. Tell me the exact steps to create WBP_LoginScreen in UE Editor:
   - Content Browser path: Content/UI/
   - How to reparent to FPMLoginWidget
   - What widgets to add (exact names)
5. Compile and test: launch PIE with dedicated server, see login screen, create account, log in

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Never send passwords in plaintext over the network in production — but for prototype, RPC is acceptable. Note that as a future improvement.
```

---

## Phase 5: Character Create + Persist

### Goal
Logged-in player creates a character with name + basic appearance, validated and persisted to the database.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Data Tables (for validation bounds) | **Content Browser** → right-click → **Miscellaneous** → **Data Table** → select your struct |
| Create Subsystem | **Tools** → **New C++ Class** → **Game Instance Subsystem** |
| UENUM for affinities | Defined in C++ header, accessible in Blueprint via `UENUM(BlueprintType)` |
| Widget Switcher | In Widget Blueprint Designer → **Palette** panel (left side) → search **Widget Switcher** |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMCharacterCreationSubsystem.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Character/` and `Private/Character/` | `UGameInstanceSubsystem` |
| `FPMCharacterCreationValidator.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Character/` and `Private/Character/` | None (plain C++ class) |
| `FPMCharacterCreationDataContract.h` | `Source/FaldoranPrimeMMO/Public/Character/` | Structs + Enums only (no .cpp needed) |
| `FPMCharacterCreationWidget.h/.cpp` | `Source/FaldoranPrimeMMO/Public/UI/` and `Private/UI/` | `UUserWidget` |
| `WBP_CharacterCreation` | `Content/UI/` (Widget Blueprint) | FPMCharacterCreationWidget |

### Agent Prompt (Session 1 of 2: Backend)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Technical/Character_Creation_System.md for the full character creation design.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 5A — Create the character creation backend: data contracts, validator, and subsystem that persists characters to PostgreSQL.

Prerequisites: Phases 0-4 complete. UFPMDatabaseSubsystem and UFPMAccountSubsystem exist and work.

Do this in micro-steps, one at a time, each must compile:
1. Create FPMCharacterCreationDataContract.h in Source/FaldoranPrimeMMO/Public/Character/
   - FFPMCharacterCreationRequest struct (CharacterName, BodyType, SkinTone, HairStyle, HairColor)
   - FFPMCharacterCreationResult struct (bSuccess, CharacterId, ErrorMessage)
   - EFPMCharacterCreationError enum (None, InvalidName, NameTaken, InvalidAppearance, TooManyCharacters, RateLimited, ServerError)
2. Create FPMCharacterCreationValidator in Source/FaldoranPrimeMMO/Public/Character/ and Private/Character/
   - ValidateName(): 3-20 chars, alphanumeric + space/hyphen/apostrophe, no leading/trailing spaces, basic profanity check
   - ValidateAppearance(): bounds checking on all indices and color ranges 0.0-1.0
   - ValidateRequest(): calls both, returns first error found
3. Create UFPMCharacterCreationSubsystem (inherits UGameInstanceSubsystem) in Source/FaldoranPrimeMMO/Public/Character/ and Private/Character/
   - SubmitCharacterCreation(FGuid AccountId, FFPMCharacterCreationRequest Request) → FFPMCharacterCreationResult
   - Calls validator, checks character count (max 5 per account), checks name uniqueness in DB, inserts into characters table
   - Rate limiting: max 5 requests per minute per account
   - Audit logging: log all attempts (success and failure)
4. Add console commands: FPM.TestCreateCharacter "Name"
5. Compile and test

Follow all rules in 00_Rules_and_Constraints.md and Character_Creation_System.md. Server authority. Never trust client. No file over 500 lines.
```

### Agent Prompt (Session 2 of 2: UI)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Technical/Character_Creation_System.md for the full character creation design.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 5B — Create the character creation UI widget and wire it to the subsystem via RPC.

Prerequisites: Phases 0-4 and 5A complete. UFPMCharacterCreationSubsystem exists with SubmitCharacterCreation().

Do this in micro-steps, one at a time, each must compile:
1. Create UFPMCharacterCreationWidget (inherits UUserWidget) in Source/FaldoranPrimeMMO/Public/UI/ and Private/UI/
   Required BindWidgets (exact names for Blueprint):
   - NameInput (EditableTextBox)
   - BodyTypeSlider (Slider)
   - SkinRedSlider, SkinGreenSlider, SkinBlueSlider (Sliders)
   - HairStyleComboBox (ComboBoxString)
   - HairColorRedSlider, HairColorGreenSlider, HairColorBlueSlider (Sliders)
   - SubmitButton (Button)
   - BackButton (Button)
   - ResultText (TextBlock)
2. Create Server RPC on AFPMPlayerController:
   - ServerRequestCreateCharacter(FFPMCharacterCreationRequest) — calls subsystem
   - ClientReceiveCreateCharacterResult(FFPMCharacterCreationResult) — sends result back
3. Update AFPMPlayerController:
   - After login success: transition to character creation screen (or character select if characters exist)
4. Tell me exact steps to create WBP_CharacterCreation in UE Editor:
   - Where in Content Browser
   - How to reparent to FPMCharacterCreationWidget
   - What widgets to add with exact names
5. Compile and test: login → create character → verify it appears in database via pgAdmin

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. For prototype, use a simple mannequin preview (no Mutable/CC5 yet).
```

---

## Phase 6: Character Select + Spawn

### Goal
Player selects a saved character and spawns in the world. Other players visible.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Create Character class | **Tools** → **New C++ Class** → parent: **Character** |
| Skeletal Mesh component | Already on ACharacter → `GetMesh()` returns `USkeletalMeshComponent*` |
| Set skeletal mesh | In constructor: `GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Path/To/Mesh")))` |
| CharacterMovementComponent | Already on ACharacter → `GetCharacterMovement()` — handles replication automatically |
| Replicated properties | In header: `UPROPERTY(Replicated)` + override `GetLifetimeReplicatedProps()` |
| Possess a pawn | `PlayerController->Possess(SpawnedCharacter)` (server-side only) |
| Mannequin mesh (UE5) | `Content/Characters/Mannequins/Meshes/SKM_Quinn` or `SKM_Manny` (UE5 default) |
| Default Third Person map | `Content/ThirdPerson/Maps/ThirdPersonMap` (if Third Person template was used) |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMPlayerCharacter.h/.cpp` | `Source/FaldoranPrimeMMO/Public/Player/` and `Private/Player/` | `ACharacter` |
| `FPMCharacterSelectWidget.h/.cpp` | `Source/FaldoranPrimeMMO/Public/UI/` and `Private/UI/` | `UUserWidget` |
| `WBP_CharacterSelect` | `Content/UI/` (Widget Blueprint) | FPMCharacterSelectWidget |

### Agent Prompt
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 6 — Create the character select screen and player character that spawns in the world with replicated appearance.

Prerequisites: Phases 0-5 complete. Accounts and characters are persisted in PostgreSQL. Login and character creation work.

Do this in micro-steps, one at a time, each must compile:
1. Create AFPMPlayerCharacter (inherits ACharacter) in Source/FaldoranPrimeMMO/Public/Player/ and Private/Player/
   - Use UE5 mannequin mesh (SKM_Manny or SKM_Quinn) as placeholder
   - Replicated properties: CharacterName (FString), BodyType (uint8), SkinTone (FLinearColor), HairColor (FLinearColor)
   - Override GetLifetimeReplicatedProps() to register replicated vars
   - OnRep functions to apply appearance changes on clients
   - Basic third-person camera setup (SpringArm + Camera component)
2. Create UFPMCharacterSelectWidget (inherits UUserWidget) in Source/FaldoranPrimeMMO/Public/UI/ and Private/UI/
   Required BindWidgets:
   - CharacterListBox (VerticalBox) — populated dynamically with character buttons
   - EnterWorldButton (Button)
   - DeleteCharacterButton (Button)
   - CreateNewButton (Button) — goes to character creation
   - SelectedCharacterText (TextBlock)
3. Create Server RPCs on AFPMPlayerController:
   - ServerRequestCharacterList() — queries DB for characters belonging to this account
   - ClientReceiveCharacterList(TArray<FFPMCharacterSummary>) — sends list back
   - ServerRequestEnterWorld(FGuid CharacterId) — server validates ownership, loads character data, spawns AFPMPlayerCharacter, possesses it
4. Update AFPMGameMode:
   - Override PostLogin() to NOT auto-spawn a pawn (player starts in UI)
   - Default pawn class = nullptr (pawn spawned manually on character select)
5. Update AFPMPlayerController flow:
   - Login → Character Select (if characters exist) or Character Creation (if no characters)
   - On "Enter World" → server spawns character → hide UI → set input mode to Game
   - On disconnect → server saves last_played, destroys character pawn
6. Tell me exact steps to create WBP_CharacterSelect in UE Editor
7. Compile and test:
   - Login → see character list → select → spawn in world
   - Open 2nd client → login with different account → both players see each other
   - Disconnect and reconnect → character persists
   - THIS IS THE PROTOTYPE MILESTONE 🎉

Follow all rules in 00_Rules_and_Constraints.md. No file over 500 lines. Server spawns characters (never client). Character ownership validated server-side.
```

---

## Phase 7: Gameplay Prototypes (Nodes & Caravans)

### Goal
Implement the core loops of the "Ashes-inspired" dynamic world: Nodes that level up based on player activity, and Caravans for transporting resources.

### UE Locations
| Action | Where to Find It |
|--------|-----------------|
| Create World Subsystem | **Tools** → **New C++ Class** → **World Subsystem** |
| Create Actor | **Tools** → **New C++ Class** → **Actor** (or Pawn) |
| Spline Component | **Add Component** in Blueprint -> **Spline** |

### Files to Create
| File | Path | Parent Class |
|------|------|-------------|
| `FPMNodeSubsystem.h/.cpp` | `Source/FaldoranPrimeMMO/Public/World/` and `Private/World/` | `UWorldSubsystem` |
| `FPMCaravanActor.h/.cpp` | `Source/FaldoranPrimeMMO/Public/World/` and `Private/World/` | `AActor` (or `APawn`) |

### Agent Prompt (Session 1 of 2: Node System)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 7A — Create the Node System backend to track regional progression.

Prerequisites: Phases 0-6 complete. DatabaseSubsystem works.

Do this in micro-steps, one at a time, each must compile:
1. Create Database Schema for Nodes (provide SQL):
   - world_nodes table (node_id VARCHAR PK, level INT, current_xp INT, next_level_xp INT)
   - Insert default node 'starting_area' with level 1
2. Create UFPMNodeSubsystem (inherits UWorldSubsystem) in Source/FaldoranPrimeMMO/Public/World/
   - On Initialize: Load node state from DB for the current map
   - AddNodeXP(int32 Amount)
     - Updates local state
     - Checks for LevelUp
     - Flushes to DB (periodically or on change)
   - OnLevelUp delegate/event
3. Console Command test:
   - FPM.AddNodeXP 100
   - Verify DB updates
4. Compile and test
```

### Agent Prompt (Session 2 of 2: Caravan System)
```
Read the file Documents/Design/00_Rules_and_Constraints.md for project rules.
Read the file Documents/Workflows/Prototype_Implementation_Plan.md for full context.

TASK: Phase 7B — Create the Caravan Actor for resource transport.

Prerequisites: Phases 0-7A complete.

Do this in micro-steps, one at a time, each must compile:
1. Create AFPMCaravanActor (inherits APawn) in Source/FaldoranPrimeMMO/Public/World/
   - Replicated movement (slow speed)
   - Health component (can be damaged by players - rudimentary PvP test)
   - Simple Inventory (TMap<ResourceId, Quantity>)
   - Function: InitializeCaravan(FVector Destination)
2. Interaction:
   - Simple "Interact" interface to let owner "Push" or "Drive" it (attach to player or follow spline)
3. DB Integration hooks (placeholders):
   - On Destroyed by Enemy -> Drop loot (log for now)
   - On Arrive -> Grant rewards (log for now)
4. Compile and test:
   - Spawn caravan -> Walk it around -> Have another client shoot it -> Verify Health drops
```

---

## File Structure (Complete Prototype)

```
Source/FaldoranPrimeMMO/
├── FaldoranPrimeMMO.Build.cs
├── Public/
│   ├── Core/
│   │   ├── FPMGameMode.h
│   │   └── FPMGameInstance.h
│   ├── Player/
│   │   ├── FPMPlayerController.h
│   │   └── FPMPlayerCharacter.h
│   ├── Database/
│   │   └── FPMDatabaseSubsystem.h
│   ├── Account/
│   │   └── FPMAccountSubsystem.h
│   ├── Character/
│   │   ├── FPMCharacterCreationSubsystem.h
│   │   ├── FPMCharacterCreationValidator.h
│   │   └── FPMCharacterCreationDataContract.h
│   ├── World/
│   │   ├── FPMNodeSubsystem.h
│   │   └── FPMCaravanActor.h
│   └── UI/
│       ├── FPMLoginWidget.h
│       ├── FPMCharacterCreationWidget.h
│       └── FPMCharacterSelectWidget.h
├── Private/
│   └── (mirrors Public/ structure)
└── ThirdParty/
    └── libpq/
        ├── include/
        ├── lib/
        └── bin/

Content/
├── Maps/
│   └── L_PrototypeWorld.umap
└── UI/
    ├── WBP_LoginScreen.uasset
    ├── WBP_CharacterCreation.uasset
    └── WBP_CharacterSelect.uasset
```

---

## What This Prototype Does NOT Include

These are all future work AFTER the prototype proves the spine works:

- ❌ Mutable/CC5 character customization (uses mannequin for now)
- ❌ Affinity redistribution UI (uses defaults)
- ❌ Combat, inventory, crafting, building
- ❌ NPC/AI, chat system
- ❌ World persistence (terrain, buildings)
- ❌ Multiple servers / sharding
- ❌ Cloud/remote database deployment (local PostgreSQL for now — migration is a config change)

### Security: What IS vs. IS NOT in the Prototype

**✅ Included in Prototype (non-negotiable for an MMO):**
- Server-authoritative game state (clients never write directly)
- Password hashing with salt (SHA-256 for prototype)
- Parameterized SQL queries (prevents SQL injection)
- Input validation on all client requests (name length, appearance bounds, etc.)
- Rate limiting on RPCs (character creation, login)
- Audit logging of all state changes
- Database credentials server-side only (never in client builds)
- `HasAuthority()` / `IsRunningDedicatedServer()` guards on all server-only code

**❌ Deferred to Production (see `00_Rules_and_Constraints.md` §Production Security Roadmap):**
- TLS/SSL encryption for client-server traffic
- bcrypt/Argon2 password hashing upgrade
- Session tokens with expiry
- IP-based connection throttling and auto-ban
- Anti-cheat detection (speed hacks, teleportation)
- Debug command stripping from shipping builds
- Penetration testing

---

## Prerequisites Before Starting Phase 0

- [ ] UE 5.7.1 (Custom Compile) installed at `E:\UEInstalled\Windows`
- [ ] Visual Studio installed with C++ game development workload
- [ ] GitHub Desktop configured with repo
- [ ] PostgreSQL 16+ installed locally (needed by Phase 3)

---

*Document created: 2026-02-07*  
*Estimated sessions to prototype: 9-11*
