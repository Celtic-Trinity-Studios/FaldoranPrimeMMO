// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/FPMCharacterCreationTestCommands.h"
#include "Character/FPMCharacterCreationDataContract.h"
#include "Character/FPMCharacterCreationSubsystem.h"
#include "Database/FPMDatabaseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharCreateTest, Log, All);

TArray<IConsoleObject *> UFPMCharacterCreationTestCommands::RegisteredCommands;

// -------------------------------------------------------------------
// Helper: Find the server world in PIE
// -------------------------------------------------------------------
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
// Helper: Get a test account ID from the database
// -------------------------------------------------------------------
// For console testing, we need a valid account ID.
// This queries the first account in the database.
static FGuid GetTestAccountId(UWorld *World) {
  UWorld *ServerWorld = FindServerWorld(World);
  if (!ServerWorld) {
    return FGuid();
  }

  UGameInstance *GI = ServerWorld->GetGameInstance();
  if (!GI) {
    return FGuid();
  }

  UFPMDatabaseSubsystem *DB = GI->GetSubsystem<UFPMDatabaseSubsystem>();
  if (!DB || !DB->IsConnected()) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: Database not connected."));
    return FGuid();
  }

  // Grab the first account for testing purposes
  const FString SQL = TEXT("SELECT account_id FROM accounts LIMIT 1");
  FFPMDatabaseQueryResult Result = DB->ExecuteQuery(SQL);

  if (!Result.bSuccess || Result.Rows.Num() == 0) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: No accounts found. Create one first "
                "with FPM.CreateAccount <username> <password>"));
    return FGuid();
  }

  const FString *AccountIdStr = Result.Rows[0].Find(TEXT("account_id"));
  if (!AccountIdStr) {
    return FGuid();
  }

  FGuid AccountId;
  FGuid::Parse(*AccountIdStr, AccountId);
  return AccountId;
}

// -------------------------------------------------------------------
// Helper: Get the character creation subsystem from a World
// -------------------------------------------------------------------
static UFPMCharacterCreationSubsystem *GetCreationSubsystem(UWorld *World) {
  UWorld *ServerWorld = FindServerWorld(World);
  if (!ServerWorld) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: No world available."));
    return nullptr;
  }

  UGameInstance *GI = ServerWorld->GetGameInstance();
  if (!GI) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: No GameInstance found."));
    return nullptr;
  }

  UFPMCharacterCreationSubsystem *CreationSys =
      GI->GetSubsystem<UFPMCharacterCreationSubsystem>();
  if (!CreationSys) {
    UE_LOG(
        LogFPMCharCreateTest, Error,
        TEXT("FPM CharCreateTest: UFPMCharacterCreationSubsystem not found."));
    return nullptr;
  }

  return CreationSys;
}

// -------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------

void UFPMCharacterCreationTestCommands::RegisterCommands() {
  if (RegisteredCommands.Num() > 0) {
    return;
  }

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestCreateCharacter"),
      TEXT("Create a test character. Usage: FPM.TestCreateCharacter <name>"),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleTestCreateCharacter),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.TestCreateCharacterWithAffinities"),
      TEXT("Create a test character with affinity distributions."),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleTestCreateCharacterWithAffinities),
      ECVF_Default));

  UE_LOG(LogFPMCharCreateTest, Log,
         TEXT("FPM CharCreateTest: Console commands registered."));
}

void UFPMCharacterCreationTestCommands::UnregisterCommands() {
  for (IConsoleObject *Cmd : RegisteredCommands) {
    if (Cmd) {
      IConsoleManager::Get().UnregisterConsoleObject(Cmd);
    }
  }
  RegisteredCommands.Empty();
  UE_LOG(LogFPMCharCreateTest, Log,
         TEXT("FPM CharCreateTest: Console commands unregistered."));
}

// -------------------------------------------------------------------
// Command Handlers
// -------------------------------------------------------------------

void UFPMCharacterCreationTestCommands::HandleTestCreateCharacter(
    const TArray<FString> &Args, UWorld *World) {
  if (Args.Num() < 1) {
    UE_LOG(LogFPMCharCreateTest, Warning,
           TEXT("FPM CharCreateTest: Usage: FPM.TestCreateCharacter <name>"));
    return;
  }

  // Build the character name from all args (supports names with spaces)
  FString CharacterName = Args[0];
  for (int32 i = 1; i < Args.Num(); ++i) {
    CharacterName += TEXT(" ") + Args[i];
  }

  UFPMCharacterCreationSubsystem *CreationSys = GetCreationSubsystem(World);
  if (!CreationSys) {
    return;
  }

  // Get a test account ID from the database
  const FGuid TestAccountId = GetTestAccountId(World);
  if (!TestAccountId.IsValid()) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: Could not find a test account. "
                "Create one first: FPM.CreateAccount <user> <pass>"));
    return;
  }

  UE_LOG(LogFPMCharCreateTest, Log,
         TEXT("FPM CharCreateTest: Creating character '%s' for account %s..."),
         *CharacterName, *TestAccountId.ToString());

  // Build request with default appearance values
  FFPMCharacterCreationRequest Request;
  Request.CharacterName = CharacterName;
  // BodyType, SkinTone, HairStyle, HairColor use struct defaults

  const FFPMCharacterCreationResult Result =
      CreationSys->SubmitCharacterCreation(TestAccountId, Request);

  if (Result.bSuccess) {
    UE_LOG(
        LogFPMCharCreateTest, Log,
        TEXT("FPM CharCreateTest: === CHARACTER CREATED === id=%s, name='%s'"),
        *Result.CharacterId.ToString(), *CharacterName);
  } else {
    UE_LOG(LogFPMCharCreateTest, Warning,
           TEXT("FPM CharCreateTest: === CREATION FAILED === error=%s"),
           *Result.ErrorMessage);
  }
}

// -------------------------------------------------------------------
// FPM.TestCreateCharacterWithAffinities
// -------------------------------------------------------------------

void UFPMCharacterCreationTestCommands::HandleTestCreateCharacterWithAffinities(
    const TArray<FString> &Args, UWorld *World) {
  UFPMCharacterCreationSubsystem *CreationSys = GetCreationSubsystem(World);
  if (!CreationSys) {
    return;
  }

  const FGuid TestAccountId = GetTestAccountId(World);
  if (!TestAccountId.IsValid()) {
    UE_LOG(LogFPMCharCreateTest, Error,
           TEXT("FPM CharCreateTest: No test account. "
                "Create one first: FPM.CreateAccount <user> <pass>"));
    return;
  }

  // Build character name from args, or use a default
  FString CharacterName = TEXT("AffinityTestChar");
  if (Args.Num() > 0) {
    CharacterName = Args[0];
    for (int32 i = 1; i < Args.Num(); ++i) {
      CharacterName += TEXT(" ") + Args[i];
    }
  }

  UE_LOG(LogFPMCharCreateTest, Log,
         TEXT("FPM CharCreateTest: Creating '%s' with affinities..."),
         *CharacterName);

  // Build request with default appearance
  FFPMCharacterCreationRequest Request;
  Request.CharacterName = CharacterName;

  // --- Playstyle distribution (total = 600) ---
  // Martial boosted to 150, others adjusted to balance
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Martial, 150});
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Ranged, 90});
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Magic, 90});
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Crafting, 90});
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Social, 90});
  Request.PlaystyleAffinities.Add({EFPMPlaystyleAffinity::Survival, 90});
  // Total: 150 + 90*5 = 600 ✓

  // --- Magical distribution (total = 800) ---
  // Fire boosted to 150, others adjusted to balance
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Fire, 150});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Water, 90});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Earth, 90});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Air, 95});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Light, 95});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Shadow, 95});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Nature, 95});
  Request.MagicalAffinities.Add({EFPMMagicalAffinity::Arcane, 90});
  // Total: 150 + 90 + 90 + 95*4 + 90 = 800 ✓

  const FFPMCharacterCreationResult Result =
      CreationSys->SubmitCharacterCreation(TestAccountId, Request);

  if (Result.bSuccess) {
    UE_LOG(LogFPMCharCreateTest, Log,
           TEXT("FPM CharCreateTest: === CHARACTER WITH AFFINITIES CREATED "
                "=== id=%s, name='%s'"),
           *Result.CharacterId.ToString(), *CharacterName);
  } else {
    UE_LOG(LogFPMCharCreateTest, Warning,
           TEXT("FPM CharCreateTest: === CREATION FAILED === error=%s"),
           *Result.ErrorMessage);
  }
}
