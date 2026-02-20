// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Core/FPMGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMGameInstance, Log, All);

void UFPMGameInstance::Init() {
  Super::Init();

  // On dedicated servers, tolerate missing World Partition level packages.
  // PIE/editor clients send /Memory/ prefixed streaming levels that don't
  // exist on the cooked server — without this, the server disconnects them.
  if (IsRunningDedicatedServer()) {
    IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
        TEXT("net.SkipMissingLevelDisconnect"));
    if (CVar) {
      CVar->Set(1);
      UE_LOG(
          LogFPMGameInstance, Log,
          TEXT("FPM: Set net.SkipMissingLevelDisconnect=1 (dedicated server)"));
    }
  }

  // --- Read server connection settings ---
  // Priority: Environment variables > ServerSecrets.ini > DefaultGame.ini

  // Priority 1: Environment variables
  FString EnvIP = FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_SERVER_IP"));
  FString EnvPort =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FPM_SERVER_PORT"));

  if (!EnvIP.IsEmpty()) {
    ServerIP = EnvIP;
  }
  if (!EnvPort.IsEmpty()) {
    ServerPort = FCString::Atoi(*EnvPort);
  }

  // Priority 2: ServerSecrets.ini (gitignored)
  FString SecretsPath =
      FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("ServerSecrets.ini"));
  if (FPaths::FileExists(SecretsPath)) {
    FConfigFile SecretsConfig;
    SecretsConfig.Read(SecretsPath);

    FString Val;
    if (EnvIP.IsEmpty()) {
      if (SecretsConfig.GetString(TEXT("FPM.Server"), TEXT("ServerIP"), Val))
        ServerIP = Val;
    }
    int32 IniPort;
    if (EnvPort.IsEmpty()) {
      if (SecretsConfig.GetString(TEXT("FPM.Server"), TEXT("ServerPort"),
                                  Val)) {
        IniPort = FCString::Atoi(*Val);
        if (IniPort > 0)
          ServerPort = IniPort;
      }
    }
    bool IniAutoConnect = bAutoConnect;
    if (SecretsConfig.GetString(TEXT("FPM.Server"), TEXT("bAutoConnect"),
                                Val)) {
      bAutoConnect = Val.ToBool();
    }
  }

  // Priority 3: DefaultGame.ini via UE config layering (GGameIni)
  if (GConfig) {
    FString IniIP;
    if (EnvIP.IsEmpty() && ServerIP == TEXT("127.0.0.1")) {
      if (GConfig->GetString(TEXT("FPM.Server"), TEXT("ServerIP"), IniIP,
                             GGameIni)) {
        ServerIP = IniIP;
      }
    }

    int32 IniPort = ServerPort;
    if (EnvPort.IsEmpty()) {
      if (GConfig->GetInt(TEXT("FPM.Server"), TEXT("ServerPort"), IniPort,
                          GGameIni)) {
        ServerPort = IniPort;
      }
    }

    bool IniAutoConnect = bAutoConnect;
    if (GConfig->GetBool(TEXT("FPM.Server"), TEXT("bAutoConnect"),
                         IniAutoConnect, GGameIni)) {
      bAutoConnect = IniAutoConnect;
    }
  }

  UE_LOG(LogFPMGameInstance, Log,
         TEXT("FPM: GameInstance initialized  |  Server=%s:%d  AutoConnect=%s"),
         *ServerIP, ServerPort, bAutoConnect ? TEXT("YES") : TEXT("NO"));

  // Auto-connect if configured (typically for packaged client builds)
  if (bAutoConnect) {
    // Defer the connect slightly so the world is fully initialized
    FTimerHandle TimerHandle;
    GetTimerManager().SetTimer(
        TimerHandle,
        FTimerDelegate::CreateUObject(
            this, &UFPMGameInstance::ConnectToDedicatedServer),
        1.0f, false);
  }
}

void UFPMGameInstance::ConnectToDedicatedServer() {
  const FString TravelURL =
      FString::Printf(TEXT("%s:%d"), *ServerIP, ServerPort);

  UE_LOG(LogFPMGameInstance, Warning,
         TEXT("FPM: Connecting to dedicated server at %s ..."), *TravelURL);

  // Get the first local player controller and travel to the server
  APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0);
  if (PC) {
    PC->ClientTravel(TravelURL, TRAVEL_Absolute);
  } else {
    UE_LOG(LogFPMGameInstance, Error,
           TEXT("FPM: Cannot connect — no local PlayerController found!"));
  }
}
