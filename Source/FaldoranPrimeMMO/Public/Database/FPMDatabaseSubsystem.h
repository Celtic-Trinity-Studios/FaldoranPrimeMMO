// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

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

  /** Attempt to connect to the PostgreSQL database using config values. */
  bool Connect();

  /** Disconnect from the database, releasing the libpq connection. */
  void Disconnect();

  /** Returns true if we have an active, valid database connection. */
  UFUNCTION(BlueprintCallable, Category = "FPM|Database")
  bool IsConnected() const;

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

  // --- Config values read from DefaultGame.ini ---
  FString ConfigHost;
  FString ConfigPort;
  FString ConfigDatabaseName;
  FString ConfigUsername;
  FString ConfigPassword;

  /** Read database connection settings from DefaultGame.ini [FPM.Database]. */
  void LoadConfigFromIni();

  /** Returns true only if running on a dedicated server. All DB ops check this.
   */
  bool IsDedicatedServerContext() const;
};
