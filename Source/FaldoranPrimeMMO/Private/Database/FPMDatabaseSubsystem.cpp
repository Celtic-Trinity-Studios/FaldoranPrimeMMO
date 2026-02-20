// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Database/FPMDatabaseSubsystem.h"
#include "Database/FPMDatabaseTestCommands.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

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

  if (Connection != nullptr && PQstatus(Connection) == CONNECTION_OK) {
    UE_LOG(LogFPMDatabase, Warning,
           TEXT("FPM Database: Already connected. Call Disconnect() first."));
    return true;
  }

  // Retry loop: attempt connection up to MaxConnectionRetries times
  for (int32 Attempt = 1; Attempt <= MaxConnectionRetries; ++Attempt) {
    UE_LOG(LogFPMDatabase, Log,
           TEXT("FPM Database: Connection attempt %d/%d to %s:%s/%s..."),
           Attempt, MaxConnectionRetries, *ConfigHost, *ConfigPort,
           *ConfigDatabaseName);

    if (AttemptSingleConnection()) {
      UE_LOG(LogFPMDatabase, Log,
             TEXT("FPM Database: Connected successfully on attempt %d "
                  "(libpq version %d)"),
             Attempt, PQlibVersion());
      return true;
    }

    // Don't sleep after the last failed attempt
    if (Attempt < MaxConnectionRetries) {
      const float DelayThisAttempt = BackoffDelaySeconds * Attempt;
      UE_LOG(LogFPMDatabase, Warning,
             TEXT("FPM Database: Attempt %d failed. Retrying in %.1fs..."),
             Attempt, DelayThisAttempt);
      FPlatformProcess::Sleep(DelayThisAttempt);
    }
  }

  UE_LOG(LogFPMDatabase, Error,
         TEXT("FPM Database: All %d connection attempts failed. "
              "Server will operate without database."),
         MaxConnectionRetries);
  return false;
}

bool UFPMDatabaseSubsystem::AttemptSingleConnection() {
  // Clean up any stale connection handle
  if (Connection != nullptr) {
    PQfinish(Connection);
    Connection = nullptr;
  }

  // Build the libpq connection string with timeout and SSL preference
  const FString ConnInfo =
      FString::Printf(TEXT("host=%s port=%s dbname=%s user=%s password=%s "
                           "connect_timeout=%d sslmode=prefer"),
                      *ConfigHost, *ConfigPort, *ConfigDatabaseName,
                      *ConfigUsername, *ConfigPassword, ConnectTimeoutSeconds);

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

bool UFPMDatabaseSubsystem::IsConnectionHealthy() {
  if (IsConnected()) {
    return true;
  }

  // Connection is lost — attempt to re-establish it
  UE_LOG(LogFPMDatabase, Warning,
         TEXT("FPM Database: Connection unhealthy. Attempting reconnect..."));
  return AttemptReconnect();
}

bool UFPMDatabaseSubsystem::AttemptReconnect() {
  UE_LOG(LogFPMDatabase, Log, TEXT("FPM Database: Reconnecting to %s:%s/%s..."),
         *ConfigHost, *ConfigPort, *ConfigDatabaseName);

  // Clean disconnect first
  Disconnect();

  // Use the full retry-aware Connect()
  const bool bReconnected = Connect();

  if (bReconnected) {
    UE_LOG(LogFPMDatabase, Log, TEXT("FPM Database: Reconnection successful!"));
  } else {
    UE_LOG(LogFPMDatabase, Error,
           TEXT("FPM Database: Reconnection FAILED after all retries."));
  }

  return bReconnected;
}

// -------------------------------------------------------------------
// Query Execution
// -------------------------------------------------------------------

FFPMDatabaseQueryResult
UFPMDatabaseSubsystem::ExecuteQuery(const FString &SQL,
                                    const TArray<FString> &Params) {
  FFPMDatabaseQueryResult Result;

  // Thread-safe: serialize all DB access through this critical section
  FScopeLock Lock(&DbCriticalSection);

  if (!IsDedicatedServerContext()) {
    Result.ErrorMessage = TEXT("Database queries are server-only.");
    UE_LOG(LogFPMDatabase, Warning, TEXT("FPM Database: %s"),
           *Result.ErrorMessage);
    return Result;
  }

  // Auto-reconnect: if connection was lost, try to re-establish it
  if (!IsConnected()) {
    UE_LOG(LogFPMDatabase, Warning,
           TEXT("FPM Database: Connection lost before query. "
                "Attempting auto-reconnect..."));
    if (!AttemptReconnect()) {
      Result.ErrorMessage =
          TEXT("Not connected to database and reconnect failed.");
      UE_LOG(LogFPMDatabase, Error, TEXT("FPM Database: %s"),
             *Result.ErrorMessage);
      return Result;
    }
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
    // Successful command with no result rows (INSERT, UPDATE, DELETE, etc.)
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

  // --- Priority 1: Environment variables (highest priority) ---
  FString EnvHost = FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_DB_HOST"));
  FString EnvPort = FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_DB_PORT"));
  FString EnvDBName =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_DB_NAME"));
  FString EnvUser = FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_DB_USER"));
  FString EnvPass =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_DB_PASSWORD"));

  if (!EnvHost.IsEmpty())
    ConfigHost = EnvHost;
  if (!EnvPort.IsEmpty())
    ConfigPort = EnvPort;
  if (!EnvDBName.IsEmpty())
    ConfigDatabaseName = EnvDBName;
  if (!EnvUser.IsEmpty())
    ConfigUsername = EnvUser;
  if (!EnvPass.IsEmpty())
    ConfigPassword = EnvPass;

  // --- Priority 2: ServerSecrets.ini (gitignored, server-only) ---
  {
    FString SecretsPath =
        FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("ServerSecrets.ini"));

    if (FPaths::FileExists(SecretsPath)) {
      UE_LOG(LogFPMDatabase, Log,
             TEXT("FPM Database: Loading credentials from ServerSecrets.ini "
                  "(%s)"),
             *SecretsPath);

      // Manual line-by-line parsing — FConfigFile::GetString can be unreliable
      // with section names containing dots (e.g. "FPM.Database")
      FString FileContents;
      if (FFileHelper::LoadFileToString(FileContents, *SecretsPath)) {
        bool bInSection = false;
        TArray<FString> Lines;
        FileContents.ParseIntoArrayLines(Lines);

        for (const FString &Line : Lines) {
          FString Trimmed = Line.TrimStartAndEnd();
          if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT(";")))
            continue;

          // Check for section header
          if (Trimmed.StartsWith(TEXT("["))) {
            bInSection =
                Trimmed.Equals(TEXT("[FPM.Database]"), ESearchCase::IgnoreCase);
            continue;
          }

          if (!bInSection)
            continue;

          // Parse Key=Value
          FString Key, Value;
          if (Trimmed.Split(TEXT("="), &Key, &Value)) {
            Key = Key.TrimStartAndEnd();
            Value = Value.TrimStartAndEnd();

            if (ConfigHost.IsEmpty() &&
                Key.Equals(TEXT("Host"), ESearchCase::IgnoreCase))
              ConfigHost = Value;
            else if (ConfigPort.IsEmpty() &&
                     Key.Equals(TEXT("Port"), ESearchCase::IgnoreCase))
              ConfigPort = Value;
            else if (ConfigDatabaseName.IsEmpty() &&
                     Key.Equals(TEXT("DatabaseName"), ESearchCase::IgnoreCase))
              ConfigDatabaseName = Value;
            else if (ConfigUsername.IsEmpty() &&
                     Key.Equals(TEXT("Username"), ESearchCase::IgnoreCase))
              ConfigUsername = Value;
            else if (ConfigPassword.IsEmpty() &&
                     Key.Equals(TEXT("Password"), ESearchCase::IgnoreCase))
              ConfigPassword = Value;
          }
        }
      } else {
        UE_LOG(LogFPMDatabase, Warning,
               TEXT("FPM Database: ServerSecrets.ini exists but could not be "
                    "read."));
      }
    } else {
      UE_LOG(LogFPMDatabase, Log,
             TEXT("FPM Database: No ServerSecrets.ini found at %s"),
             *SecretsPath);
    }
  }

  // --- Priority 3: DefaultGame.ini via UE config layering (lowest) ---
  if (ConfigHost.IsEmpty())
    GConfig->GetString(*Section, TEXT("Host"), ConfigHost, GGameIni);
  if (ConfigPort.IsEmpty())
    GConfig->GetString(*Section, TEXT("Port"), ConfigPort, GGameIni);
  if (ConfigDatabaseName.IsEmpty())
    GConfig->GetString(*Section, TEXT("DatabaseName"), ConfigDatabaseName,
                       GGameIni);
  if (ConfigUsername.IsEmpty())
    GConfig->GetString(*Section, TEXT("Username"), ConfigUsername, GGameIni);
  if (ConfigPassword.IsEmpty())
    GConfig->GetString(*Section, TEXT("Password"), ConfigPassword, GGameIni);

  // --- Sensible defaults ---
  if (ConfigHost.IsEmpty()) {
    ConfigHost = TEXT("localhost");
  }
  if (ConfigPort.IsEmpty()) {
    ConfigPort = TEXT("5432");
  }

  // --- Log config source (without password) ---
  UE_LOG(LogFPMDatabase, Log,
         TEXT("FPM Database: Config loaded — Host=%s, Port=%s, DB=%s, User=%s, "
              "Password=%s"),
         *ConfigHost, *ConfigPort, *ConfigDatabaseName, *ConfigUsername,
         ConfigPassword.IsEmpty() ? TEXT("<empty>") : TEXT("<set>"));
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
