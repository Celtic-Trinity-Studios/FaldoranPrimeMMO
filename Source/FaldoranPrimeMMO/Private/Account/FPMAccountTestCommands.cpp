// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Account/FPMAccountTestCommands.h"
#include "Account/FPMAccountSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMAccountTest, Log, All);

TArray<IConsoleObject *> UFPMAccountTestCommands::RegisteredCommands;

// -------------------------------------------------------------------
// Helper: Find the server world in PIE
// -------------------------------------------------------------------
// In PIE the Output Log's console runs commands in the CLIENT world.
// We need the SERVER world to access server-only subsystems.
// Wrapped in anonymous namespace to avoid Unity Build collisions.
namespace {
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
// Helper: Get the account subsystem from a World pointer
// -------------------------------------------------------------------
static UFPMAccountSubsystem *GetAccountSubsystem(UWorld *World) {
  UWorld *ServerWorld = FindServerWorld(World);
  if (!ServerWorld) {
    UE_LOG(LogFPMAccountTest, Error,
           TEXT("FPM AccountTest: No world available."));
    return nullptr;
  }

  UGameInstance *GI = ServerWorld->GetGameInstance();
  if (!GI) {
    UE_LOG(LogFPMAccountTest, Error,
           TEXT("FPM AccountTest: No GameInstance found."));
    return nullptr;
  }

  UFPMAccountSubsystem *AccountSys = GI->GetSubsystem<UFPMAccountSubsystem>();
  if (!AccountSys) {
    UE_LOG(LogFPMAccountTest, Error,
           TEXT("FPM AccountTest: UFPMAccountSubsystem not found."));
    return nullptr;
  }

  return AccountSys;
}
} // anonymous namespace

// -------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------

void UFPMAccountTestCommands::RegisterCommands() {
  if (RegisteredCommands.Num() > 0) {
    return;
  }

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.CreateAccount"),
      TEXT("Create a new account. Usage: FPM.CreateAccount <username> "
           "<password>"),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
          &HandleCreateAccount),
      ECVF_Default));

  RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
      TEXT("FPM.Login"),
      TEXT("Log in to an existing account. Usage: FPM.Login <username> "
           "<password>"),
      FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleLogin),
      ECVF_Default));

  UE_LOG(LogFPMAccountTest, Log,
         TEXT("FPM AccountTest: Console commands registered."));
}

void UFPMAccountTestCommands::UnregisterCommands() {
  for (IConsoleObject *Cmd : RegisteredCommands) {
    if (Cmd) {
      IConsoleManager::Get().UnregisterConsoleObject(Cmd);
    }
  }
  RegisteredCommands.Empty();
  UE_LOG(LogFPMAccountTest, Log,
         TEXT("FPM AccountTest: Console commands unregistered."));
}

// -------------------------------------------------------------------
// Command Handlers
// -------------------------------------------------------------------

void UFPMAccountTestCommands::HandleCreateAccount(const TArray<FString> &Args,
                                                  UWorld *World) {
  if (Args.Num() < 2) {
    UE_LOG(LogFPMAccountTest, Warning,
           TEXT("FPM AccountTest: Usage: FPM.CreateAccount <username> "
                "<password>"));
    return;
  }

  UFPMAccountSubsystem *AccountSys = GetAccountSubsystem(World);
  if (!AccountSys) {
    return;
  }

  const FString &Username = Args[0];
  const FString &Password = Args[1];

  UE_LOG(LogFPMAccountTest, Log,
         TEXT("FPM AccountTest: Creating account for '%s'..."), *Username);

  const FFPMLoginResult Result = AccountSys->CreateAccount(Username, Password);

  if (Result.bSuccess) {
    UE_LOG(LogFPMAccountTest, Log,
           TEXT("FPM AccountTest: === ACCOUNT CREATED === id=%s"),
           *Result.AccountId.ToString());
  } else {
    UE_LOG(LogFPMAccountTest, Warning,
           TEXT("FPM AccountTest: === ACCOUNT CREATION FAILED === %s"),
           *Result.ErrorMessage);
  }
}

void UFPMAccountTestCommands::HandleLogin(const TArray<FString> &Args,
                                          UWorld *World) {
  if (Args.Num() < 2) {
    UE_LOG(LogFPMAccountTest, Warning,
           TEXT("FPM AccountTest: Usage: FPM.Login <username> <password>"));
    return;
  }

  UFPMAccountSubsystem *AccountSys = GetAccountSubsystem(World);
  if (!AccountSys) {
    return;
  }

  const FString &Username = Args[0];
  const FString &Password = Args[1];

  UE_LOG(LogFPMAccountTest, Log, TEXT("FPM AccountTest: Logging in as '%s'..."),
         *Username);

  const FFPMLoginResult Result = AccountSys->Login(Username, Password);

  if (Result.bSuccess) {
    UE_LOG(LogFPMAccountTest, Log,
           TEXT("FPM AccountTest: === LOGIN SUCCESSFUL === id=%s"),
           *Result.AccountId.ToString());
  } else {
    UE_LOG(LogFPMAccountTest, Warning,
           TEXT("FPM AccountTest: === LOGIN FAILED === %s"),
           *Result.ErrorMessage);
  }
}
