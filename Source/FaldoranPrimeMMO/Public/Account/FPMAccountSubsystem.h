// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Account/FPMAccountTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FPMAccountSubsystem.generated.h"

/**
 * UFPMAccountSubsystem
 *
 * Server-authoritative subsystem that handles account creation and login.
 * All operations run exclusively on the dedicated server:
 *   - Password hashing with SHA-256 + random salt
 *   - Username validation (3-20 chars, alphanumeric only)
 *   - Database reads/writes via UFPMDatabaseSubsystem
 *
 * Plaintext passwords are NEVER stored. Client input is NEVER trusted.
 */
UCLASS()
class FALDORANPRIMEMMO_API UFPMAccountSubsystem
    : public UGameInstanceSubsystem {
  GENERATED_BODY()

public:
  // --- Lifecycle ---
  virtual void Initialize(FSubsystemCollectionBase &Collection) override;
  virtual void Deinitialize() override;

  // --- Account Operations (Server-Only) ---

  /**
   * Create a new account with the given credentials.
   * Server-side only. Validates username, hashes password with salt,
   * and inserts into the accounts table.
   *
   * @param Username  The desired username (3-20 chars, alphanumeric).
   * @param Password  The plaintext password (hashed before storage).
   * @return          FFPMLoginResult with success status and new AccountId.
   */
  FFPMLoginResult CreateAccount(const FString &Username,
                                const FString &Password);

  /**
   * Authenticate an existing account.
   * Server-side only. Queries the database for the username, verifies
   * the password hash, and updates last_login on success.
   *
   * @param Username  The account username.
   * @param Password  The plaintext password to verify.
   * @return          FFPMLoginResult with success status and AccountId.
   */
  FFPMLoginResult Login(const FString &Username, const FString &Password);

private:
  // --- Validation ---

  /** Minimum allowed username length. */
  static constexpr int32 MinUsernameLength = 3;

  /** Maximum allowed username length. */
  static constexpr int32 MaxUsernameLength = 20;

  /** Minimum allowed password length. */
  static constexpr int32 MinPasswordLength = 6;

  /**
   * Validate that a username meets all requirements:
   * - Length between MinUsernameLength and MaxUsernameLength
   * - Contains only alphanumeric characters (A-Z, a-z, 0-9)
   *
   * @param Username      The username to validate.
   * @param OutError      Set to the error description if validation fails.
   * @return              true if the username is valid.
   */
  bool ValidateUsername(const FString &Username, FString &OutError) const;

  /**
   * Validate that a password meets minimum requirements.
   *
   * @param Password      The password to validate.
   * @param OutError      Set to the error description if validation fails.
   * @return              true if the password is valid.
   */
  bool ValidatePassword(const FString &Password, FString &OutError) const;

  // --- Password Hashing ---

  /**
   * Generate a cryptographically random 16-byte salt, returned as a
   * 32-character hex string.
   */
  static FString GenerateSalt();

  /**
   * Hash a password with the given salt using SHA-256.
   * Format: SHA256(Salt + Password), returned as a 64-character hex string.
   * The salt is prepended to prevent rainbow table attacks.
   *
   * @param Password  The plaintext password.
   * @param Salt      The hex-encoded salt string.
   * @return          The SHA-256 hash as a 64-character hex string.
   */
  static FString HashPassword(const FString &Password, const FString &Salt);

  /**
   * Verify a plaintext password against a stored hash and salt.
   *
   * @param Password    The plaintext password to verify.
   * @param StoredHash  The stored SHA-256 hash to compare against.
   * @param StoredSalt  The stored salt that was used for hashing.
   * @return            true if the password matches.
   */
  static bool VerifyPassword(const FString &Password, const FString &StoredHash,
                             const FString &StoredSalt);

  // --- Server Guard ---

  /** Returns true only if running on a dedicated server. */
  bool IsDedicatedServerContext() const;
};
