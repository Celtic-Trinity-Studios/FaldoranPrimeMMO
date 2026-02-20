// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

// Forward-declare the libpq connection type so we don't expose the header
// publicly
struct pg_conn;
typedef struct pg_conn PGconn;

#include "FPMDatabaseSubsystem.generated.h"

/**
 * FFPMDatabaseQueryResult
 *
 * Holds the result of a database query as an array of rows,
 * where each row is a map of column name to string value.
 */
USTRUCT(BlueprintType)
struct FALDORANPRIMEMMO_API FFPMDatabaseQueryResult {
  GENERATED_BODY()

  /** Whether the query executed successfully. */
  UPROPERTY(BlueprintReadOnly)
  bool bSuccess = false;

  /** Error message if query failed, empty on success. */
  UPROPERTY(BlueprintReadOnly)
  FString ErrorMessage;

  /** Number of rows affected (for INSERT/UPDATE/DELETE). */
  UPROPERTY(BlueprintReadOnly)
  int32 RowsAffected = 0;

  /** Result rows: each row is a map of column name → value as string. */
  TArray<TMap<FString, FString>> Rows;
};

/**
 * UFPMDatabaseSubsystem
 *
 * Server-authoritative subsystem that manages the PostgreSQL database
 * connection. Provides Connect/Disconnect/ExecuteQuery for all other server
 * systems.
 *
 * This subsystem only operates on dedicated servers — all methods are no-ops on
 * clients. Connection credentials are read from Config/DefaultGame.ini
 * [FPM.Database].
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMDatabaseSubsystem
    : public UGameInstanceSubsystem {
  GENERATED_BODY()

public:
  // --- Lifecycle ---
  virtual void Initialize(FSubsystemCollectionBase &Collection) override;
  virtual void Deinitialize() override;

  // --- Connection Management ---

  /**
   * Attempt to connect to the PostgreSQL database using config values.
   * Retries up to MaxConnectionRetries times with BackoffDelaySeconds between
   * attempts. Uses connect_timeout from config to limit each attempt.
   */
  bool Connect();

  /** Disconnect from the database, releasing the libpq connection. */
  void Disconnect();

  /** Returns true if we have an active, valid database connection. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Database")
  bool IsConnected() const;

  /**
   * Checks if the connection is healthy by verifying PQstatus.
   * Unlike IsConnected(), this also attempts a reconnect if the connection
   * has been lost, making it suitable for use before critical operations.
   */
  UFUNCTION(BlueprintCallable, Category = "FPM|Database")
  bool IsConnectionHealthy();

  // --- Query Execution ---

  /**
   * Execute a parameterized SQL query.
   * Uses PQexecParams to prevent SQL injection — never concatenate user input
   * into SQL.
   *
   * @param SQL        The SQL statement with $1, $2, etc. parameter
   * placeholders.
   * @param Params     Array of parameter values (as strings) matching the
   * placeholders.
   * @return           FFPMDatabaseQueryResult with success status, rows, and
   * error info.
   */
  FFPMDatabaseQueryResult ExecuteQuery(const FString &SQL,
                                       const TArray<FString> &Params = {});

private:
  /** The active libpq connection handle. Null when disconnected. */
  PGconn *Connection = nullptr;

  // --- Connection Retry Configuration ---
  /** Maximum number of connection attempts before giving up. */
  static constexpr int32 MaxConnectionRetries = 3;

  /** Seconds to wait between retry attempts (linear backoff). */
  static constexpr float BackoffDelaySeconds = 2.0f;

  /** Connection timeout in seconds (used in libpq connect string). */
  static constexpr int32 ConnectTimeoutSeconds = 10;

  // --- Config values read from DefaultGame.ini ---
  FString ConfigHost;
  FString ConfigPort;
  FString ConfigDatabaseName;
  FString ConfigUsername;
  FString ConfigPassword;

  /** Read database connection settings.
   *  Priority: env vars > ServerSecrets.ini > DefaultGame.ini [FPM.Database].
   */
  void LoadConfigFromIni();

  /** Critical section for thread-safe database access. */
  FCriticalSection DbCriticalSection;

  /** Returns true only if running on a dedicated server. All DB ops check this.
   */
  bool IsDedicatedServerContext() const;

  /**
   * Attempt a single connection to PostgreSQL without retries.
   * Used internally by Connect() for each retry attempt.
   * @return true if connection was established successfully.
   */
  bool AttemptSingleConnection();

  /**
   * Attempt to re-establish a lost connection.
   * Disconnects cleanly first, then calls Connect() with retries.
   * @return true if reconnection succeeded.
   */
  bool AttemptReconnect();

  /** Timer handle for deferred reconnection attempts in PIE. */
  FTimerHandle ReconnectTimerHandle;
};
