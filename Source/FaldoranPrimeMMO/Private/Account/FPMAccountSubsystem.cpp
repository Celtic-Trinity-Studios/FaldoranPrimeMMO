// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Account/FPMAccountSubsystem.h"
#include "Account/FPMAccountTestCommands.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/SecureHash.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFPMAccount, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMAccountSubsystem::Initialize(FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

#if WITH_EDITOR
  // In PIE, always proceed — the world isn't assigned yet so we can't
  // distinguish server from client. Operations are guarded at call time.
  UE_LOG(LogFPMAccount, Log,
         TEXT("FPM Account: Subsystem initializing (Editor/PIE mode)."));
#else
  if (!IsRunningDedicatedServer()) {
    UE_LOG(LogFPMAccount, Log,
           TEXT("FPM Account: Skipping initialization (not a dedicated "
                "server)."));
    return;
  }
#endif

  UFPMAccountTestCommands::RegisterCommands();
  UE_LOG(LogFPMAccount, Log, TEXT("FPM Account: Subsystem initialized."));
}

void UFPMAccountSubsystem::Deinitialize() {
  UFPMAccountTestCommands::UnregisterCommands();
  UE_LOG(LogFPMAccount, Log, TEXT("FPM Account: Subsystem deinitialized."));
  Super::Deinitialize();
}

// -------------------------------------------------------------------
// Account Operations
// -------------------------------------------------------------------

FFPMLoginResult UFPMAccountSubsystem::CreateAccount(const FString &Username,
                                                    const FString &Password) {
  FFPMLoginResult Result;

  if (!IsDedicatedServerContext()) {
    Result.ErrorMessage = TEXT("Account creation is server-only.");
    UE_LOG(LogFPMAccount, Warning, TEXT("FPM Account: %s"),
           *Result.ErrorMessage);
    return Result;
  }

  // --- Validate input (never trust client) ---
  FString ValidationError;
  if (!ValidateUsername(Username, ValidationError)) {
    Result.ErrorMessage = ValidationError;
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: CreateAccount failed — %s"),
           *Result.ErrorMessage);
    return Result;
  }

  if (!ValidatePassword(Password, ValidationError)) {
    Result.ErrorMessage = ValidationError;
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: CreateAccount failed — %s"),
           *Result.ErrorMessage);
    return Result;
  }

  // --- Get database subsystem ---
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: No GameInstance available."));
    return Result;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: Database not available or not connected."));
    return Result;
  }

  // --- Hash the password with a random salt ---
  const FString Salt = GenerateSalt();
  const FString PasswordHash = HashPassword(Password, Salt);

  // --- Insert into the accounts table ---
  // gen_random_uuid() generates the account_id server-side in PostgreSQL
  const FString SQL =
      TEXT("INSERT INTO accounts (account_id, username, password_hash, "
           "password_salt, created_at, last_login) "
           "VALUES (gen_random_uuid(), LOWER($1), $2, $3, NOW(), NOW()) "
           "RETURNING account_id");

  FFPMDatabaseQueryResult DBResult =
      DB->ExecuteQuery(SQL, {Username, PasswordHash, Salt});

  if (!DBResult.bSuccess) {
    // Check for unique constraint violation (duplicate username)
    if (DBResult.ErrorMessage.Contains(TEXT("unique")) ||
        DBResult.ErrorMessage.Contains(TEXT("duplicate"))) {
      Result.ErrorMessage = TEXT("Username already exists.");
      UE_LOG(LogFPMAccount, Warning,
             TEXT("FPM Account: Duplicate username '%s'."), *Username);
    } else {
      Result.ErrorMessage = TEXT("Internal server error.");
      UE_LOG(LogFPMAccount, Error, TEXT("FPM Account: DB insert failed — %s"),
             *DBResult.ErrorMessage);
    }
    return Result;
  }

  // --- Extract the returned account_id ---
  if (DBResult.Rows.Num() > 0) {
    const FString *AccountIdStr = DBResult.Rows[0].Find(TEXT("account_id"));
    if (AccountIdStr) {
      FGuid::Parse(*AccountIdStr, Result.AccountId);
    }
  }

  Result.bSuccess = true;
  UE_LOG(LogFPMAccount, Log,
         TEXT("FPM Account: Account created — user='%s', id=%s"), *Username,
         *Result.AccountId.ToString());

  return Result;
}

FFPMLoginResult UFPMAccountSubsystem::Login(const FString &Username,
                                            const FString &Password) {
  FFPMLoginResult Result;

  if (!IsDedicatedServerContext()) {
    Result.ErrorMessage = TEXT("Login is server-only.");
    UE_LOG(LogFPMAccount, Warning, TEXT("FPM Account: %s"),
           *Result.ErrorMessage);
    return Result;
  }

  // --- Basic validation (same constraints as creation) ---
  FString ValidationError;
  if (!ValidateUsername(Username, ValidationError)) {
    // Generic error to avoid leaking whether the username format was wrong
    Result.ErrorMessage = TEXT("Invalid username or password.");
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: Login validation failed for '%s' — %s"),
           *Username, *ValidationError);
    return Result;
  }

  // --- Get database subsystem ---
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: No GameInstance available."));
    return Result;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: Database not available or not connected."));
    return Result;
  }

  // --- Query the accounts table for the username ---
  const FString SQL =
      TEXT("SELECT account_id, password_hash, password_salt FROM accounts "
           "WHERE username = LOWER($1)");

  FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(SQL, {Username});

  if (!DBResult.bSuccess) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error, TEXT("FPM Account: Login query failed — %s"),
           *DBResult.ErrorMessage);
    return Result;
  }

  // Use generic error message to avoid leaking whether the username exists
  if (DBResult.Rows.Num() == 0) {
    Result.ErrorMessage = TEXT("Invalid username or password.");
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: Login failed — user '%s' not found."), *Username);
    return Result;
  }

  // --- Verify password hash ---
  const TMap<FString, FString> &Row = DBResult.Rows[0];
  const FString *StoredHash = Row.Find(TEXT("password_hash"));
  const FString *StoredSalt = Row.Find(TEXT("password_salt"));
  const FString *AccountIdStr = Row.Find(TEXT("account_id"));

  if (!StoredHash || !StoredSalt || !AccountIdStr) {
    Result.ErrorMessage = TEXT("Internal server error.");
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: Missing columns in account row for '%s'."),
           *Username);
    return Result;
  }

  if (!VerifyPassword(Password, *StoredHash, *StoredSalt)) {
    Result.ErrorMessage = TEXT("Invalid username or password.");
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: Login failed — wrong password for '%s'."),
           *Username);
    return Result;
  }

  // --- Parse account ID ---
  FGuid::Parse(*AccountIdStr, Result.AccountId);

  // --- Update last_login timestamp ---
  const FString UpdateSQL =
      TEXT("UPDATE accounts SET last_login = NOW() WHERE account_id = $1");
  FFPMDatabaseQueryResult UpdateResult =
      DB->ExecuteQuery(UpdateSQL, {*AccountIdStr});

  if (!UpdateResult.bSuccess) {
    // Non-fatal — log but don't fail the login
    UE_LOG(LogFPMAccount, Warning,
           TEXT("FPM Account: Failed to update last_login for '%s' — %s"),
           *Username, *UpdateResult.ErrorMessage);
  }

  Result.bSuccess = true;
  UE_LOG(LogFPMAccount, Log,
         TEXT("FPM Account: Login successful — user='%s', id=%s"), *Username,
         *Result.AccountId.ToString());

  return Result;
}

// -------------------------------------------------------------------
// Validation
// -------------------------------------------------------------------

bool UFPMAccountSubsystem::ValidateUsername(const FString &Username,
                                            FString &OutError) const {
  if (Username.Len() < MinUsernameLength) {
    OutError = FString::Printf(TEXT("Username must be at least %d characters."),
                               MinUsernameLength);
    return false;
  }

  if (Username.Len() > MaxUsernameLength) {
    OutError = FString::Printf(TEXT("Username must be at most %d characters."),
                               MaxUsernameLength);
    return false;
  }

  // Only allow alphanumeric characters (A-Z, a-z, 0-9)
  for (TCHAR Char : Username) {
    if (!FChar::IsAlnum(Char)) {
      OutError = TEXT("Username must contain only letters and numbers.");
      return false;
    }
  }

  return true;
}

bool UFPMAccountSubsystem::ValidatePassword(const FString &Password,
                                            FString &OutError) const {
  if (Password.Len() < MinPasswordLength) {
    OutError = FString::Printf(TEXT("Password must be at least %d characters."),
                               MinPasswordLength);
    return false;
  }

  return true;
}

// -------------------------------------------------------------------
// Password Hashing
// -------------------------------------------------------------------

FString UFPMAccountSubsystem::GenerateSalt() {
  // 32 random bytes = 256-bit salt (upgraded from 128-bit)
  static constexpr int32 SaltBytes = 32;
  TArray<uint8> SaltData;
  SaltData.SetNumUninitialized(SaltBytes);

#if PLATFORM_WINDOWS
  // Use Windows CNG cryptographic RNG (FIPS-compliant)
  NTSTATUS Status = BCryptGenRandom(nullptr, SaltData.GetData(), SaltBytes,
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (!BCRYPT_SUCCESS(Status)) {
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: BCryptGenRandom failed (status=0x%08X). "
                "Falling back to FGuid-based salt."),
           Status);
    // Fallback: Use FGuid which is platform-generated (still better than
    // FMath::Rand)
    const FGuid G1 = FGuid::NewGuid();
    const FGuid G2 = FGuid::NewGuid();
    FMemory::Memcpy(SaltData.GetData(), &G1, 16);
    FMemory::Memcpy(SaltData.GetData() + 16, &G2, 16);
  }
#else
  // On non-Windows: FGuid::NewGuid() uses /dev/urandom or platform crypto
  const FGuid G1 = FGuid::NewGuid();
  const FGuid G2 = FGuid::NewGuid();
  FMemory::Memcpy(SaltData.GetData(), &G1, 16);
  FMemory::Memcpy(SaltData.GetData() + 16, &G2, 16);
#endif

  return BytesToHex(SaltData.GetData(), SaltData.Num());
}

/** Helper: SHA-256 hash using Windows BCrypt API.
 *  Writes a 32-byte digest into OutHash. Returns true on success. */
static bool BCryptSHA256(const uint8 *Data, int32 DataLen, uint8 OutHash[32]) {
#if PLATFORM_WINDOWS
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) !=
      0) {
    return false;
  }

  BCRYPT_HASH_HANDLE hHash = nullptr;
  bool bSuccess = false;
  if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0) {
    if (BCryptHashData(hHash, const_cast<uint8 *>(Data), DataLen, 0) == 0) {
      if (BCryptFinishHash(hHash, OutHash, 32, 0) == 0) {
        bSuccess = true;
      }
    }
    BCryptDestroyHash(hHash);
  }
  BCryptCloseAlgorithmProvider(hAlg, 0);
  return bSuccess;
#else
  // Non-Windows fallback: use FSHA1 double-pass (less ideal, but functional).
  // Production builds should integrate libsodium for cross-platform SHA-256.
  FSHA1 Sha;
  Sha.Update(Data, DataLen);
  Sha.Final();
  uint8 Sha1Hash[20];
  Sha.GetHash(Sha1Hash);
  FMemory::Memzero(OutHash, 32);
  FMemory::Memcpy(OutHash, Sha1Hash, 20);
  return true;
#endif
}

FString UFPMAccountSubsystem::HashPassword(const FString &Password,
                                           const FString &Salt) {
  // PBKDF2-like key stretching using iterated SHA-256.
  // This is significantly more resistant to brute force than single-pass
  // hashing.
  //
  // Production TODO: Replace with Argon2id once a vetted C library is
  //                  integrated (e.g. libsodium crypto_pwhash).
  static constexpr int32 Iterations = 10000;

  const FString Combined = Salt + Password;
  const FTCHARToUTF8 UTF8Combined(*Combined);

  // Initial hash — 32-byte SHA-256 digest
  uint8 Hash[32];
  if (!BCryptSHA256(reinterpret_cast<const uint8 *>(UTF8Combined.Get()),
                    UTF8Combined.Length(), Hash)) {
    UE_LOG(LogFPMAccount, Error,
           TEXT("FPM Account: SHA-256 hash failed! Returning empty hash."));
    return FString();
  }

  // Iterate to stretch
  for (int32 i = 1; i < Iterations; ++i) {
    BCryptSHA256(Hash, 32, Hash);
  }

  return BytesToHex(Hash, 32);
}

bool UFPMAccountSubsystem::VerifyPassword(const FString &Password,
                                          const FString &StoredHash,
                                          const FString &StoredSalt) {
  const FString ComputedHash = HashPassword(Password, StoredSalt);
  // Case-insensitive compare of hex strings
  return ComputedHash.Equals(StoredHash, ESearchCase::IgnoreCase);
}

// -------------------------------------------------------------------
// Server Guard
// -------------------------------------------------------------------

bool UFPMAccountSubsystem::IsDedicatedServerContext() const {
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
