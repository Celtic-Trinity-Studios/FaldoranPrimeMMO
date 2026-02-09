// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/FPMCharacterCreationSubsystem.h"
#include "Character/FPMCharacterCreationTestCommands.h"
#include "Character/FPMCharacterCreationValidator.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterCreation, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UFPMCharacterCreationSubsystem::Initialize(
    FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

#if WITH_EDITOR
  // In PIE, always proceed — operations are guarded at call time
  UE_LOG(LogFPMCharacterCreation, Log,
         TEXT("FPM CharacterCreation: Subsystem initializing (Editor/PIE)."));
#else
  if (!IsRunningDedicatedServer()) {
    UE_LOG(LogFPMCharacterCreation, Log,
           TEXT("FPM CharacterCreation: Skipping init (not a server)."));
    return;
  }
#endif

  UFPMCharacterCreationTestCommands::RegisterCommands();
  UE_LOG(LogFPMCharacterCreation, Log,
         TEXT("FPM CharacterCreation: Subsystem initialized."));
}

void UFPMCharacterCreationSubsystem::Deinitialize() {
  UFPMCharacterCreationTestCommands::UnregisterCommands();
  RateLimitMap.Empty();
  UE_LOG(LogFPMCharacterCreation, Log,
         TEXT("FPM CharacterCreation: Subsystem deinitialized."));
  Super::Deinitialize();
}

// -------------------------------------------------------------------
// Character Creation (Server-Only)
// -------------------------------------------------------------------

FFPMCharacterCreationResult
UFPMCharacterCreationSubsystem::SubmitCharacterCreation(
    const FGuid &AccountId, const FFPMCharacterCreationRequest &Request) {
  FFPMCharacterCreationResult Result;

  // --- Server guard ---
  if (!IsDedicatedServerContext()) {
    Result.ErrorCode = EFPMCharacterCreationError::ServerError;
    Result.ErrorMessage = TEXT("Character creation is server-only.");
    UE_LOG(LogFPMCharacterCreation, Warning, TEXT("FPM CharacterCreation: %s"),
           *Result.ErrorMessage);
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Validate account ID ---
  if (!AccountId.IsValid()) {
    Result.ErrorCode = EFPMCharacterCreationError::ServerError;
    Result.ErrorMessage = TEXT("Invalid account.");
    UE_LOG(LogFPMCharacterCreation, Warning,
           TEXT("FPM CharacterCreation: Invalid AccountId provided."));
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Rate limit check ---
  if (IsRateLimited(AccountId)) {
    Result.ErrorCode = EFPMCharacterCreationError::RateLimited;
    Result.ErrorMessage =
        TEXT("Too many requests. Please wait before trying again.");
    UE_LOG(LogFPMCharacterCreation, Warning,
           TEXT("FPM CharacterCreation: Rate limited — account=%s"),
           *AccountId.ToString());
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // Record this attempt timestamp for rate limiting
  RecordRequest(AccountId);

  // --- Validate the request (name + appearance) ---
  EFPMCharacterCreationError ValidationError;
  FString ValidationMessage;
  if (!FFPMCharacterCreationValidator::ValidateRequest(Request, ValidationError,
                                                       ValidationMessage)) {
    Result.ErrorCode = ValidationError;
    Result.ErrorMessage = ValidationMessage;
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Check character count limit ---
  const int32 CharCount = GetCharacterCount(AccountId);
  if (CharCount < 0) {
    Result.ErrorCode = EFPMCharacterCreationError::ServerError;
    Result.ErrorMessage = TEXT("Internal server error.");
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  if (CharCount >= FFPMCharacterCreationValidator::MaxCharactersPerAccount) {
    Result.ErrorCode = EFPMCharacterCreationError::TooManyCharacters;
    Result.ErrorMessage = FString::Printf(
        TEXT("Maximum of %d characters per account."),
        FFPMCharacterCreationValidator::MaxCharactersPerAccount);
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Check name uniqueness ---
  if (IsNameTaken(Request.CharacterName)) {
    Result.ErrorCode = EFPMCharacterCreationError::NameTaken;
    Result.ErrorMessage = TEXT("That character name is already taken.");
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Insert into database ---
  FGuid NewCharacterId;
  FString InsertError;
  if (!InsertCharacter(AccountId, Request, NewCharacterId, InsertError)) {
    // Check if insert failed due to race condition on name uniqueness
    if (InsertError.Contains(TEXT("unique")) ||
        InsertError.Contains(TEXT("duplicate"))) {
      Result.ErrorCode = EFPMCharacterCreationError::NameTaken;
      Result.ErrorMessage = TEXT("That character name is already taken.");
    } else {
      Result.ErrorCode = EFPMCharacterCreationError::ServerError;
      Result.ErrorMessage = TEXT("Internal server error.");
    }
    AuditLog(AccountId, Request, Result);
    return Result;
  }

  // --- Success ---
  Result.bSuccess = true;
  Result.CharacterId = NewCharacterId;
  Result.ErrorCode = EFPMCharacterCreationError::None;

  AuditLog(AccountId, Request, Result);
  return Result;
}

// -------------------------------------------------------------------
// Rate Limiting
// -------------------------------------------------------------------

bool UFPMCharacterCreationSubsystem::IsRateLimited(const FGuid &AccountId) {
  const double Now = FPlatformTime::Seconds();

  FRateLimitEntry *Entry = RateLimitMap.Find(AccountId);
  if (!Entry) {
    return false;
  }

  // Prune timestamps outside the window
  Entry->RequestTimestamps.RemoveAll([Now](double Timestamp) {
    return (Now - Timestamp) > RateLimitWindowSeconds;
  });

  return Entry->RequestTimestamps.Num() >= MaxRequestsPerWindow;
}

void UFPMCharacterCreationSubsystem::RecordRequest(const FGuid &AccountId) {
  FRateLimitEntry &Entry = RateLimitMap.FindOrAdd(AccountId);
  Entry.RequestTimestamps.Add(FPlatformTime::Seconds());
}

// -------------------------------------------------------------------
// Database Operations
// -------------------------------------------------------------------

bool UFPMCharacterCreationSubsystem::IsNameTaken(const FString &CharacterName) {
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    return true; // Fail closed — assume taken if we can't check
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: DB not available for name check."));
    return true; // Fail closed
  }

  const FString SQL = TEXT("SELECT 1 FROM characters WHERE "
                           "LOWER(character_name) = LOWER($1) LIMIT 1");

  FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(SQL, {CharacterName});

  if (!DBResult.bSuccess) {
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: Name check query failed — %s"),
           *DBResult.ErrorMessage);
    return true; // Fail closed
  }

  return DBResult.Rows.Num() > 0;
}

int32 UFPMCharacterCreationSubsystem::GetCharacterCount(
    const FGuid &AccountId) {
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    return -1;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: DB not available for count check."));
    return -1;
  }

  const FString SQL = TEXT(
      "SELECT COUNT(*) AS char_count FROM characters WHERE account_id = $1");

  FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(
      SQL, {AccountId.ToString(EGuidFormats::DigitsWithHyphensLower)});

  if (!DBResult.bSuccess || DBResult.Rows.Num() == 0) {
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: Character count query failed — %s"),
           *DBResult.ErrorMessage);
    return -1;
  }

  const FString *CountStr = DBResult.Rows[0].Find(TEXT("char_count"));
  if (!CountStr) {
    return -1;
  }

  return FCString::Atoi(**CountStr);
}

bool UFPMCharacterCreationSubsystem::InsertCharacter(
    const FGuid &AccountId, const FFPMCharacterCreationRequest &Request,
    FGuid &OutCharacterId, FString &OutError) {
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    OutError = TEXT("No GameInstance.");
    return false;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    OutError = TEXT("Database not available.");
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: DB not available for insert."));
    return false;
  }

  // Insert the character with a server-generated UUID
  const FString SQL =
      TEXT("INSERT INTO characters "
           "(character_id, account_id, character_name, body_type, "
           "skin_color_r, skin_color_g, skin_color_b, "
           "hair_style, hair_color_r, hair_color_g, hair_color_b, "
           "created_at, last_played) "
           "VALUES (gen_random_uuid(), $1, $2, $3, "
           "$4, $5, $6, $7, $8, $9, $10, NOW(), NOW()) "
           "RETURNING character_id");

  const FString AccountIdStr =
      AccountId.ToString(EGuidFormats::DigitsWithHyphensLower);

  TArray<FString> Params;
  Params.Add(AccountIdStr);
  Params.Add(Request.CharacterName);
  Params.Add(FString::Printf(TEXT("%d"), Request.BodyType));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.R));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.G));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.B));
  Params.Add(FString::Printf(TEXT("%d"), Request.HairStyle));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.R));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.G));
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.B));

  FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(SQL, Params);

  if (!DBResult.bSuccess) {
    OutError = DBResult.ErrorMessage;
    UE_LOG(LogFPMCharacterCreation, Error,
           TEXT("FPM CharacterCreation: Insert failed — %s"),
           *DBResult.ErrorMessage);
    return false;
  }

  // Extract the returned character_id
  if (DBResult.Rows.Num() > 0) {
    const FString *CharIdStr = DBResult.Rows[0].Find(TEXT("character_id"));
    if (CharIdStr) {
      FGuid::Parse(*CharIdStr, OutCharacterId);
    }
  }

  UE_LOG(LogFPMCharacterCreation, Log,
         TEXT("FPM CharacterCreation: Character inserted — name='%s', id=%s, "
              "account=%s"),
         *Request.CharacterName, *OutCharacterId.ToString(), *AccountIdStr);

  return true;
}

// -------------------------------------------------------------------
// Audit Logging
// -------------------------------------------------------------------

void UFPMCharacterCreationSubsystem::AuditLog(
    const FGuid &AccountId, const FFPMCharacterCreationRequest &Request,
    const FFPMCharacterCreationResult &Result) {
  if (Result.bSuccess) {
    UE_LOG(LogFPMCharacterCreation, Log,
           TEXT("FPM AUDIT: SUCCESS — account=%s, name='%s', character=%s"),
           *AccountId.ToString(), *Request.CharacterName,
           *Result.CharacterId.ToString());
  } else {
    UE_LOG(LogFPMCharacterCreation, Warning,
           TEXT("FPM AUDIT: FAILED — account=%s, name='%s', error=%s"),
           *AccountId.ToString(), *Request.CharacterName, *Result.ErrorMessage);
  }
}

// -------------------------------------------------------------------
// Server Guard
// -------------------------------------------------------------------

bool UFPMCharacterCreationSubsystem::IsDedicatedServerContext() const {
  if (IsRunningDedicatedServer()) {
    return true;
  }

#if WITH_EDITOR
  if (const UGameInstance *GI = GetGameInstance()) {
    if (const UWorld *World = GI->GetWorld()) {
      const ENetMode NetMode = World->GetNetMode();
      return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
    }
  }
#endif

  return false;
}
