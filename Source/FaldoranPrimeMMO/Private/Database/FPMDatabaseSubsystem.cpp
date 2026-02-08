// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Database/FPMDatabaseSubsystem.h"
#include "Database/FPMDatabaseTestCommands.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"

THIRD_PARTY_INCLUDES_START
#include "libpq-fe.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogFPMDatabase, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMDatabaseSubsystem::Initialize(FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

#if WITH_EDITOR
  // In PIE, the world isn't assigned yet at Initialize time, so we can't
  // check net mode here. Always proceed — operations are guarded at call time.
  UE_LOG(LogFPMDatabase, Log,
         TEXT("FPM Database: Subsystem initializing (Editor/PIE mode)."));
#else
  if (!IsRunningDedicatedServer()) {
    UE_LOG(
        LogFPMDatabase, Log,
        TEXT(
            "FPM Database: Skipping initialization (not a dedicated server)."));
    return;
  }
#endif

  LoadConfigFromIni();
  UE_LOG(LogFPMDatabase, Log,
         TEXT("FPM Database: Subsystem initialized. Host=%s, Port=%s, DB=%s, "
              "User=%s"),
         *ConfigHost, *ConfigPort, *ConfigDatabaseName, *ConfigUsername);

  UFPMDatabaseTestCommands::RegisterCommands();

  // Auto-connect to the database
#if WITH_EDITOR
  // In PIE, the world net mode isn't set yet during Initialize().
  // Defer the connection attempt so IsDedicatedServerContext() works.
  if (UGameInstance *GI = GetGameInstance()) {
    if (UWorld *World = GI->GetWorld()) {
      World->GetTimerManager().SetTimerForNextTick(
          FTimerDelegate::CreateWeakLambda(this, [this]() {
            if (IsDedicatedServerContext()) {
              UE_LOG(LogFPMDatabase, Log,
                     TEXT("FPM Database: Deferred PIE connect attempt..."));
              Connect();
            }
          }));
    }
  }
#else
  // In packaged builds, connect immediately on the dedicated server
  Connect();
#endif
}

void UFPMDatabaseSubsystem::Deinitialize() {
  UFPMDatabaseTestCommands::UnregisterCommands();
  Disconnect();
  UE_LOG(LogFPMDatabase, Log, TEXT("FPM Database: Subsystem deinitialized."));
  Super::Deinitialize();
}

// -------------------------------------------------------------------
// Connection Management
// -------------------------------------------------------------------

bool UFPMDatabaseSubsystem::Connect() {
  if (!IsDedicatedServerContext()) {
    UE_LOG(LogFPMDatabase, Log,
           TEXT("FPM Database: Connect() skipped (not a server context)."));
    return false;
  }

  if (Connection != nullptr) {
    UE_LOG(LogFPMDatabase, Warning,
           TEXT("FPM Database: Already connected. Call Disconnect() first."));
    return true;
  }

  // Build the libpq connection string from config values
  const FString ConnInfo = FString::Printf(
      TEXT("host=%s port=%s dbname=%s user=%s password=%s connect_timeout=10"),
      *ConfigHost, *ConfigPort, *ConfigDatabaseName, *ConfigUsername,
      *ConfigPassword);

  // PQconnectdb expects a UTF-8 C string
  Connection = PQconnectdb(TCHAR_TO_UTF8(*ConnInfo));

  if (PQstatus(Connection) != CONNECTION_OK) {
    const FString ErrorMsg = UTF8_TO_TCHAR(PQerrorMessage(Connection));
    UE_LOG(LogFPMDatabase, Error, TEXT("FPM Database: Connection FAILED — %s"),
           *ErrorMsg);

    // Clean up the failed connection handle
    PQfinish(Connection);
    Connection = nullptr;
    return false;
  }

  UE_LOG(LogFPMDatabase, Log,
         TEXT("FPM Database: Connected successfully to %s:%s/%s (libpq version "
              "%d)"),
         *ConfigHost, *ConfigPort, *ConfigDatabaseName, PQlibVersion());

  return true;
}

void UFPMDatabaseSubsystem::Disconnect() {
  if (Connection != nullptr) {
    PQfinish(Connection);
    Connection = nullptr;
    UE_LOG(LogFPMDatabase, Log, TEXT("FPM Database: Disconnected."));
  }
}

bool UFPMDatabaseSubsystem::IsConnected() const {
  if (Connection == nullptr) {
    return false;
  }

  // PQstatus checks the actual connection state (handles dropped connections)
  return PQstatus(Connection) == CONNECTION_OK;
}

// -------------------------------------------------------------------
// Query Execution
// -------------------------------------------------------------------

FFPMDatabaseQueryResult
UFPMDatabaseSubsystem::ExecuteQuery(const FString &SQL,
                                    const TArray<FString> &Params) {
  FFPMDatabaseQueryResult Result;

  if (!IsDedicatedServerContext()) {
    Result.ErrorMessage = TEXT("Database queries are server-only.");
    UE_LOG(LogFPMDatabase, Warning, TEXT("FPM Database: %s"),
           *Result.ErrorMessage);
    return Result;
  }

  if (!IsConnected()) {
    Result.ErrorMessage = TEXT("Not connected to database.");
    UE_LOG(LogFPMDatabase, Warning, TEXT("FPM Database: %s"),
           *Result.ErrorMessage);
    return Result;
  }

  // Convert FString params to UTF-8 C strings for PQexecParams
  TArray<FTCHARToUTF8> ParamConverters;
  TArray<const char *> ParamValues;
  ParamConverters.Reserve(Params.Num());
  ParamValues.Reserve(Params.Num());

  for (const FString &Param : Params) {
    ParamConverters.Emplace(*Param);
    ParamValues.Add(ParamConverters.Last().Get());
  }

  // Execute parameterized query — prevents SQL injection by design
  PGresult *PgResult =
      PQexecParams(Connection, TCHAR_TO_UTF8(*SQL), ParamValues.Num(),
                   nullptr, // Let PostgreSQL infer parameter types
                   ParamValues.GetData(),
                   nullptr, // Parameter lengths (null = text format)
                   nullptr, // Parameter formats (null = text format)
                   0        // Result format: 0 = text
      );

  if (PgResult == nullptr) {
    Result.ErrorMessage = UTF8_TO_TCHAR(PQerrorMessage(Connection));
    UE_LOG(LogFPMDatabase, Error,
           TEXT("FPM Database: Query failed (null result) — %s"),
           *Result.ErrorMessage);
    return Result;
  }

  const ExecStatusType Status = PQresultStatus(PgResult);

  if (Status == PGRES_COMMAND_OK) {
    // Successful command with no result rows (INSERT, UPDATE, DELETE, CREATE,
    // etc.)
    Result.bSuccess = true;
    const char *RowsAffectedStr = PQcmdTuples(PgResult);
    if (RowsAffectedStr && RowsAffectedStr[0] != '\0') {
      Result.RowsAffected = FCString::Atoi(UTF8_TO_TCHAR(RowsAffectedStr));
    }
  } else if (Status == PGRES_TUPLES_OK) {
    // Successful SELECT — extract rows
    Result.bSuccess = true;
    const int32 NumRows = PQntuples(PgResult);
    const int32 NumCols = PQnfields(PgResult);

    Result.Rows.Reserve(NumRows);

    for (int32 Row = 0; Row < NumRows; ++Row) {
      TMap<FString, FString> RowData;
      for (int32 Col = 0; Col < NumCols; ++Col) {
        const FString ColName = UTF8_TO_TCHAR(PQfname(PgResult, Col));
        if (PQgetisnull(PgResult, Row, Col)) {
          RowData.Add(ColName, TEXT("NULL"));
        } else {
          RowData.Add(ColName, UTF8_TO_TCHAR(PQgetvalue(PgResult, Row, Col)));
        }
      }
      Result.Rows.Add(MoveTemp(RowData));
    }

    Result.RowsAffected = NumRows;
  } else {
    // Query failed
    Result.ErrorMessage = UTF8_TO_TCHAR(PQresultErrorMessage(PgResult));
    UE_LOG(LogFPMDatabase, Error, TEXT("FPM Database: Query error — %s"),
           *Result.ErrorMessage);
  }

  PQclear(PgResult);
  return Result;
}

// -------------------------------------------------------------------
// Private Helpers
// -------------------------------------------------------------------

void UFPMDatabaseSubsystem::LoadConfigFromIni() {
  const FString Section = TEXT("FPM.Database");

  GConfig->GetString(*Section, TEXT("Host"), ConfigHost, GGameIni);
  GConfig->GetString(*Section, TEXT("Port"), ConfigPort, GGameIni);
  GConfig->GetString(*Section, TEXT("DatabaseName"), ConfigDatabaseName,
                     GGameIni);
  GConfig->GetString(*Section, TEXT("Username"), ConfigUsername, GGameIni);
  GConfig->GetString(*Section, TEXT("Password"), ConfigPassword, GGameIni);

  // Provide sensible defaults if config is missing
  if (ConfigHost.IsEmpty()) {
    ConfigHost = TEXT("localhost");
  }
  if (ConfigPort.IsEmpty()) {
    ConfigPort = TEXT("5432");
  }
}

bool UFPMDatabaseSubsystem::IsDedicatedServerContext() const {
  // In packaged builds, check the binary type
  if (IsRunningDedicatedServer()) {
    return true;
  }

#if WITH_EDITOR
  // In PIE, the binary is the Editor so IsRunningDedicatedServer() is always
  // false. Instead, check if this GameInstance's world is acting as a server.
  if (const UGameInstance *GI = GetGameInstance()) {
    if (const UWorld *World = GI->GetWorld()) {
      const ENetMode NetMode = World->GetNetMode();
      return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
    }
  }
#endif

  return false;
}
