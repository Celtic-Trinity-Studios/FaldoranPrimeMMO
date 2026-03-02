// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Character/FPMCharacterCreationDataContract.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "FPMCharacterCreationSubsystem.generated.h"

/**
 * UFPMCharacterCreationSubsystem
 *
 * Server-authoritative subsystem that handles character creation requests.
 * Validates all input, enforces rate limits, checks name uniqueness,
 * enforces one-character-per-account limits, and persists to PostgreSQL.
 *
 * This subsystem only operates on the dedicated server. All methods are
 * no-ops on clients. Client input is NEVER trusted.
 *
 * Design: One character per account (classless skill-based system).
 *
 * Flow:
 *   1. Rate limit check (max 5 requests per minute per account)
 *   2. Validate request via FFPMCharacterCreationValidator
 *   3. Check character exists (only 1 allowed per account)
 *   4. Check name uniqueness in database
 *   5. Insert character into database
 *   6. Audit log the attempt (success or failure)
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMCharacterCreationSubsystem
    : public UGameInstanceSubsystem {
  GENERATED_BODY()

public:
  // --- Lifecycle ---
  virtual void Initialize(FSubsystemCollectionBase &Collection) override;
  virtual void Deinitialize() override;

  // --- Character Creation (Server-Only) ---

  /**
   * Submit a character creation request for the given account.
   * Server-side only. Validates all input, checks limits, and persists
   * the character to the database on success.
   *
   * @param AccountId  The authenticated account's UUID (server-derived).
   * @param Request    The untrusted character creation request.
   * @return           FFPMCharacterCreationResult with success or error info.
   */
  FFPMCharacterCreationResult
  SubmitCharacterCreation(const FGuid &AccountId,
                          const FFPMCharacterCreationRequest &Request);

private:
  // --- Rate Limiting ---

  /** Maximum number of creation requests allowed per time window. */
  static constexpr int32 MaxRequestsPerWindow = 5;

  /** Rate limit window duration in seconds. */
  static constexpr double RateLimitWindowSeconds = 60.0;

  /**
   * Per-account rate limiting data.
   * Tracks timestamps of recent creation attempts to prevent spam.
   */
  struct FRateLimitEntry {
    TArray<double> RequestTimestamps;
  };

  /** Map of AccountId to rate limit tracking data. */
  TMap<FGuid, FRateLimitEntry> RateLimitMap;

  /**
   * Check if an account has exceeded the rate limit.
   * Prunes expired timestamps and checks the count.
   *
   * @param AccountId  The account to check.
   * @return           true if the account is rate-limited (reject request).
   */
  bool IsRateLimited(const FGuid &AccountId);

  /**
   * Record a new request timestamp for rate limiting.
   *
   * @param AccountId  The account that made the request.
   */
  void RecordRequest(const FGuid &AccountId);

  // --- Database Operations ---

  /**
   * Check if a character name is already taken in the database.
   * Case-insensitive comparison via LOWER() in SQL.
   *
   * @param CharacterName  The name to check.
   * @return               true if the name is already in use.
   */
  bool IsNameTaken(const FString &CharacterName);

  /**
   * Get the number of characters owned by an account.
   *
   * @param AccountId  The account to check.
   * @return           Number of characters, or -1 on query failure.
   */
  int32 GetCharacterCount(const FGuid &AccountId);

  /**
   * Insert a new character into the database.
   *
   * @param AccountId      The owning account's UUID.
   * @param Request        The validated creation request.
   * @param OutCharacterId Set to the new character's UUID on success.
   * @param OutError       Set to error description on failure.
   * @return               true if the insert succeeded.
   */
  bool InsertCharacter(const FGuid &AccountId,
                       const FFPMCharacterCreationRequest &Request,
                       FGuid &OutCharacterId, FString &OutError);

  /**
   * Insert affinity rows for a newly created character.
   * One row per affinity (6 playstyle + 8 magical = up to 14 rows).
   * Only inserts non-empty pools.
   *
   * @param CharacterId  The new character's UUID.
   * @param Request      The validated creation request containing affinities.
   * @param OutError     Set to error description on failure.
   * @return             true if all affinity rows were inserted.
   */
  bool InsertAffinities(const FGuid &CharacterId,
                        const FFPMCharacterCreationRequest &Request,
                        FString &OutError);

  // --- Audit Logging ---

  /**
   * Log a character creation attempt for security auditing.
   * Logs both successes and failures with all relevant details.
   *
   * @param AccountId   The account that made the attempt.
   * @param Request     The creation request data.
   * @param Result      The result of the attempt.
   */
  static void AuditLog(const FGuid &AccountId,
                       const FFPMCharacterCreationRequest &Request,
                       const FFPMCharacterCreationResult &Result);

  // --- Server Guard ---

  /** Returns true only if running on a dedicated server (or PIE server). */
  bool IsDedicatedServerContext() const;
};
