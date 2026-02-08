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
  UFPMDatabaseSubsystem *DB = GetDBSubsystem(World);
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
  UFPMDatabaseSubsystem *DB = GetDBSubsystem(World);
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
  UFPMDatabaseSubsystem *DB = GetDBSubsystem(World);
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
