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
    Result.ErrorCode = EFPMCharacterCreationError::CharacterAlreadyExists;
    Result.ErrorMessage = TEXT(
        "Your account already has a character. One character per account.");
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

  // --- Insert affinities (if provided) ---
  if (Request.PlaystyleAffinities.Num() > 0 ||
      Request.MagicalAffinities.Num() > 0) {
    FString AffinityError;
    if (!InsertAffinities(NewCharacterId, Request, AffinityError)) {
      // Character was already inserted — log the error.
      // Production would use a DB transaction; prototype logs and continues.
      UE_LOG(LogFPMCharacterCreation, Error,
             TEXT("FPM CharacterCreation: Affinity insert failed — %s"),
             *AffinityError);
    }
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
           "(character_id, account_id, character_name, species, body_type, "
           "skin_color_r, skin_color_g, skin_color_b, "
           "eye_color_r, eye_color_g, eye_color_b, "
           "hair_style, hair_color_r, hair_color_g, hair_color_b, "
           "morph_jaw, morph_nose, morph_brow, morph_lips, "
           "created_at, last_played) "
           "VALUES (gen_random_uuid(), $1, $2, $3, $4, "
           "$5, $6, $7, $8, $9, $10, $11, $12, $13, $14, "
           "$15, $16, $17, $18, NOW(), NOW()) "
           "RETURNING character_id");

  const FString AccountIdStr =
      AccountId.ToString(EGuidFormats::DigitsWithHyphensLower);

  // Extract morph values (default to 0.5 if not provided)
  const float MorphJaw =
      Request.FacialMorphs.Num() > 0 ? Request.FacialMorphs[0] : 0.5f;
  const float MorphNose =
      Request.FacialMorphs.Num() > 1 ? Request.FacialMorphs[1] : 0.5f;
  const float MorphBrow =
      Request.FacialMorphs.Num() > 2 ? Request.FacialMorphs[2] : 0.5f;
  const float MorphLips =
      Request.FacialMorphs.Num() > 3 ? Request.FacialMorphs[3] : 0.5f;

  TArray<FString> Params;
  Params.Add(AccountIdStr);          // $1
  Params.Add(Request.CharacterName); // $2
  Params.Add(FString::Printf(TEXT("%d"),
                             static_cast<int32>(Request.Species))); // $3
  Params.Add(FString::Printf(TEXT("%d"), Request.BodyType));        // $4
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.R));    // $5
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.G));    // $6
  Params.Add(FString::Printf(TEXT("%.6f"), Request.SkinTone.B));    // $7
  Params.Add(FString::Printf(TEXT("%.6f"), Request.EyeColor.R));    // $8
  Params.Add(FString::Printf(TEXT("%.6f"), Request.EyeColor.G));    // $9
  Params.Add(FString::Printf(TEXT("%.6f"), Request.EyeColor.B));    // $10
  Params.Add(FString::Printf(TEXT("%d"), Request.HairStyle));       // $11
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.R));   // $12
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.G));   // $13
  Params.Add(FString::Printf(TEXT("%.6f"), Request.HairColor.B));   // $14
  Params.Add(FString::Printf(TEXT("%.6f"), MorphJaw));              // $15
  Params.Add(FString::Printf(TEXT("%.6f"), MorphNose));             // $16
  Params.Add(FString::Printf(TEXT("%.6f"), MorphBrow));             // $17
  Params.Add(FString::Printf(TEXT("%.6f"), MorphLips));             // $18

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
         TEXT("FPM CharacterCreation: Character inserted — name='%s', "
              "id=%s, account=%s"),
         *Request.CharacterName, *OutCharacterId.ToString(), *AccountIdStr);

  return true;
}

// -------------------------------------------------------------------
// Insert Affinities
// -------------------------------------------------------------------

bool UFPMCharacterCreationSubsystem::InsertAffinities(
    const FGuid &CharacterId, const FFPMCharacterCreationRequest &Request,
    FString &OutError) {
  UGameInstance *GI = GetGameInstance();
  if (!GI) {
    OutError = TEXT("No GameInstance.");
    return false;
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    OutError = TEXT("Database not available.");
    return false;
  }

  const FString CharIdStr =
      CharacterId.ToString(EGuidFormats::DigitsWithHyphensLower);

  const FString SQL =
      TEXT("INSERT INTO character_affinities "
           "(character_id, affinity_pool, affinity_type, points) "
           "VALUES ($1, $2, $3, $4)");

  // Pool 0 = Playstyle affinities
  static constexpr int32 PlaystylePoolId = 0;
  for (const FFPMPlaystyleAffinityEntry &Entry : Request.PlaystyleAffinities) {
    const int32 TypeIndex = static_cast<int32>(Entry.Affinity);
    TArray<FString> Params;
    Params.Add(CharIdStr);
    Params.Add(FString::Printf(TEXT("%d"), PlaystylePoolId));
    Params.Add(FString::Printf(TEXT("%d"), TypeIndex));
    Params.Add(FString::Printf(TEXT("%d"), Entry.Points));

    FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(SQL, Params);
    if (!DBResult.bSuccess) {
      OutError =
          FString::Printf(TEXT("Playstyle affinity %d insert failed — %s"),
                          TypeIndex, *DBResult.ErrorMessage);
      return false;
    }
  }

  // Pool 1 = Magical affinities
  static constexpr int32 MagicalPoolId = 1;
  for (const FFPMMagicalAffinityEntry &Entry : Request.MagicalAffinities) {
    const int32 TypeIndex = static_cast<int32>(Entry.Affinity);
    TArray<FString> Params;
    Params.Add(CharIdStr);
    Params.Add(FString::Printf(TEXT("%d"), MagicalPoolId));
    Params.Add(FString::Printf(TEXT("%d"), TypeIndex));
    Params.Add(FString::Printf(TEXT("%d"), Entry.Points));

    FFPMDatabaseQueryResult DBResult = DB->ExecuteQuery(SQL, Params);
    if (!DBResult.bSuccess) {
      OutError = FString::Printf(TEXT("Magical affinity %d insert failed — %s"),
                                 TypeIndex, *DBResult.ErrorMessage);
      return false;
    }
  }

  UE_LOG(LogFPMCharacterCreation, Log,
         TEXT("FPM CharacterCreation: Affinities inserted — character=%s, "
              "playstyle=%d, magical=%d"),
         *CharIdStr, Request.PlaystyleAffinities.Num(),
         Request.MagicalAffinities.Num());

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
