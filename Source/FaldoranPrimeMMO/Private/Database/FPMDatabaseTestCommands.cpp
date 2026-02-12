// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Database/FPMDatabaseTestCommands.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMDBTest, Log, All);

TArray<IConsoleObject *> UFPMDatabaseTestCommands::RegisteredCommands;

// -------------------------------------------------------------------
// Helper: Find the server world in PIE
// -------------------------------------------------------------------
// In PIE the Output Log's console runs commands in the CLIENT world.
// We need the SERVER world to access server-only subsystems.
// Wrapped in named namespace to avoid Unity Build collisions.
namespace FPMDBTestHelpers {
static UWorld *FindServerWorld(UWorld *FallbackWorld) {
#if WITH_EDITOR
  if (GEngine) {
    for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
      UWorld *W = Context.World();
      if (W) {
        const ENetMode NetMode = W->GetNetMode();
        if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer) {
          return W;
        }
      }
    }
  }
#endif
  return FallbackWorld;
}

// -------------------------------------------------------------------
// Helper: Get the database subsystem from a World pointer
// -------------------------------------------------------------------
static UFPMDatabaseSubsystem *GetDBSubsystem(UWorld *World) {
  UWorld *ServerWorld = FindServerWorld(World);
  if (!ServerWorld) {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: No world available."));
    return nullptr;
  }

  UGameInstance *GI = ServerWorld->GetGameInstance();
  if (!GI) {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: No GameInstance found."));
    return nullptr;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB) {
    UE_LOG(LogFPMDBTest, Error,
           TEXT("FPM DBTest: UFPMDatabaseSubsystem not found."));
    return nullptr;
  }

  return DB;
}
} // namespace FPMDBTestHelpers

// -------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------

void UFPMDatabaseTestCommands::RegisterCommands() {
  // Only register if not already registered
  if (RegisteredCommands.Num() > 0) {
    return;
  }

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestDBConnect"),
      TEXT("Connect to the PostgreSQL database and log the result."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleTestDBConnect),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestDBWrite"),
      TEXT("Create a db_test table and insert a test row."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTestDBWrite),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestDBRead"), TEXT("Read all rows from db_test and log them."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTestDBRead),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestRemoteDBConnect"),
      TEXT("Full remote connection test: connect + query + latency."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleTestRemoteDBConnect),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestDBHealth"),
      TEXT("Test IsConnectionHealthy() with auto-reconnect."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleTestDBHealth),
      ECVF_Default));

  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: Console commands registered."));
}

void UFPMDatabaseTestCommands::UnregisterCommands() {
  for (IConsoleObject *Cmd : RegisteredCommands) {
    if (Cmd) {
      IConsoleManager::Get().UnregisterConsoleObject(Cmd);
    }
  }
  RegisteredCommands.Empty();
  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: Console commands unregistered."));
}

// -------------------------------------------------------------------
// Command Handlers
// -------------------------------------------------------------------

void UFPMDatabaseTestCommands::HandleTestDBConnect(const TArray<FString> &Args,
                                                   UWorld *World) {
  UFPMDatabaseSubsystem *DB = FPMDBTestHelpers::GetDBSubsystem(World);
  if (!DB) {
    return;
  }

  if (DB->IsConnected()) {
    UE_LOG(LogFPMDBTest, Log,
           TEXT("FPM DBTest: Already connected to database."));
    return;
  }

  const bool bSuccess = DB->Connect();
  if (bSuccess) {
    UE_LOG(LogFPMDBTest, Log,
           TEXT("FPM DBTest: === CONNECTION SUCCESSFUL ==="));
  } else {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: === CONNECTION FAILED ==="));
  }
}

void UFPMDatabaseTestCommands::HandleTestDBWrite(const TArray<FString> &Args,
                                                 UWorld *World) {
  UFPMDatabaseSubsystem *DB = FPMDBTestHelpers::GetDBSubsystem(World);
  if (!DB) {
    return;
  }

  if (!DB->IsConnected()) {
    UE_LOG(LogFPMDBTest, Warning,
           TEXT("FPM DBTest: Not connected. Run FPM.TestDBConnect first."));
    return;
  }

  // Create the test table if it doesn't exist
  FFPMDatabaseQueryResult CreateResult = DB->ExecuteQuery(
      TEXT("CREATE TABLE IF NOT EXISTS db_test (id SERIAL PRIMARY KEY, message "
           "TEXT, created_at TIMESTAMPTZ DEFAULT NOW())"));

  if (!CreateResult.bSuccess) {
    UE_LOG(LogFPMDBTest, Error,
           TEXT("FPM DBTest: Failed to create test table — %s"),
           *CreateResult.ErrorMessage);
    return;
  }

  // Insert a test row with a timestamp message
  const FString TestMessage = FString::Printf(
      TEXT("Hello from UE server at %s"), *FDateTime::Now().ToString());
  FFPMDatabaseQueryResult InsertResult = DB->ExecuteQuery(
      TEXT("INSERT INTO db_test (message) VALUES ($1)"), {TestMessage});

  if (InsertResult.bSuccess) {
    UE_LOG(LogFPMDBTest, Log,
           TEXT("FPM DBTest: Inserted test row — \"%s\" (%d row(s) affected)"),
           *TestMessage, InsertResult.RowsAffected);
  } else {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: Insert failed — %s"),
           *InsertResult.ErrorMessage);
  }
}

void UFPMDatabaseTestCommands::HandleTestDBRead(const TArray<FString> &Args,
                                                UWorld *World) {
  UFPMDatabaseSubsystem *DB = FPMDBTestHelpers::GetDBSubsystem(World);
  if (!DB) {
    return;
  }

  if (!DB->IsConnected()) {
    UE_LOG(LogFPMDBTest, Warning,
           TEXT("FPM DBTest: Not connected. Run FPM.TestDBConnect first."));
    return;
  }

  FFPMDatabaseQueryResult ReadResult = DB->ExecuteQuery(
      TEXT("SELECT id, message, created_at FROM db_test ORDER BY id"));

  if (!ReadResult.bSuccess) {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: Read failed — %s"),
           *ReadResult.ErrorMessage);
    return;
  }

  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: Read %d row(s) from db_test:"),
         ReadResult.Rows.Num());
  for (int32 i = 0; i < ReadResult.Rows.Num(); ++i) {
    const TMap<FString, FString> &Row = ReadResult.Rows[i];
    const FString *Id = Row.Find(TEXT("id"));
    const FString *Msg = Row.Find(TEXT("message"));
    const FString *CreatedAt = Row.Find(TEXT("created_at"));

    UE_LOG(LogFPMDBTest, Log, TEXT("  [%s] %s (created: %s)"),
           Id ? **Id : TEXT("?"), Msg ? **Msg : TEXT("?"),
           CreatedAt ? **CreatedAt : TEXT("?"));
  }
}

void UFPMDatabaseTestCommands::HandleTestRemoteDBConnect(
    const TArray<FString> &Args, UWorld *World) {
  UFPMDatabaseSubsystem *DB = FPMDBTestHelpers::GetDBSubsystem(World);
  if (!DB) {
    return;
  }

  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: === REMOTE CONNECTION TEST ==="));

  // Step 1: Check current connection status
  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: Current connection status: %s"),
         DB->IsConnected() ? TEXT("CONNECTED") : TEXT("DISCONNECTED"));

  // Step 2: If not connected, attempt connection with retry logic
  if (!DB->IsConnected()) {
    UE_LOG(LogFPMDBTest, Log,
           TEXT("FPM DBTest: Attempting connection with retry logic..."));
    const double ConnStartTime = FPlatformTime::Seconds();
    const bool bConnected = DB->Connect();
    const double ConnElapsed = FPlatformTime::Seconds() - ConnStartTime;

    if (!bConnected) {
      UE_LOG(LogFPMDBTest, Error,
             TEXT("FPM DBTest: === REMOTE CONNECTION TEST FAILED ==="));
      UE_LOG(LogFPMDBTest, Error,
             TEXT("FPM DBTest: Could not connect after %.2fs."), ConnElapsed);
      return;
    }
    UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: Connected in %.3fs."),
           ConnElapsed);
  }

  // Step 3: Run a simple query to verify the connection is functional
  const double QueryStartTime = FPlatformTime::Seconds();
  FFPMDatabaseQueryResult PingResult =
      DB->ExecuteQuery(TEXT("SELECT 1 AS ping"));
  const double QueryElapsed = FPlatformTime::Seconds() - QueryStartTime;

  if (!PingResult.bSuccess) {
    UE_LOG(LogFPMDBTest, Error, TEXT("FPM DBTest: Ping query failed — %s"),
           *PingResult.ErrorMessage);
    UE_LOG(LogFPMDBTest, Error,
           TEXT("FPM DBTest: === REMOTE CONNECTION TEST FAILED ==="));
    return;
  }

  UE_LOG(LogFPMDBTest, Log,
         TEXT("FPM DBTest: Ping query succeeded (latency: %.1fms)"),
         QueryElapsed * 1000.0);

  // Step 4: Verify the schema tables exist
  FFPMDatabaseQueryResult SchemaResult = DB->ExecuteQuery(
      TEXT("SELECT table_name FROM information_schema.tables "
           "WHERE table_schema = 'public' ORDER BY table_name"));

  if (SchemaResult.bSuccess) {
    UE_LOG(LogFPMDBTest, Log,
           TEXT("FPM DBTest: Found %d table(s) in public schema:"),
           SchemaResult.Rows.Num());
    for (const TMap<FString, FString> &Row : SchemaResult.Rows) {
      const FString *TableName = Row.Find(TEXT("table_name"));
      UE_LOG(LogFPMDBTest, Log, TEXT("  - %s"),
             TableName ? **TableName : TEXT("?"));
    }
  }

  UE_LOG(LogFPMDBTest, Log,
         TEXT("FPM DBTest: === REMOTE CONNECTION TEST PASSED ==="));
}

void UFPMDatabaseTestCommands::HandleTestDBHealth(const TArray<FString> &Args,
                                                  UWorld *World) {
  UFPMDatabaseSubsystem *DB = FPMDBTestHelpers::GetDBSubsystem(World);
  if (!DB) {
    return;
  }

  UE_LOG(LogFPMDBTest, Log,
         TEXT("FPM DBTest: === CONNECTION HEALTH CHECK ==="));
  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: IsConnected() = %s"),
         DB->IsConnected() ? TEXT("true") : TEXT("false"));

  const bool bHealthy = DB->IsConnectionHealthy();
  UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: IsConnectionHealthy() = %s"),
         bHealthy ? TEXT("true") : TEXT("false"));

  if (bHealthy) {
    UE_LOG(LogFPMDBTest, Log, TEXT("FPM DBTest: === HEALTH CHECK PASSED ==="));
  } else {
    UE_LOG(LogFPMDBTest, Error,
           TEXT("FPM DBTest: === HEALTH CHECK FAILED ==="));
  }
}
