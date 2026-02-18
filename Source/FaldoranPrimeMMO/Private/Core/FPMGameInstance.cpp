// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Core/FPMGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

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
          LogTemp, Log,
          TEXT("FPM: Set net.SkipMissingLevelDisconnect=1 (dedicated server)"));
    }
  }

  // --- Read server connection settings from DefaultGame.ini ---
  FString IniPath =
      FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
  FConfigCacheIni::NormalizeConfigIniPath(IniPath);

  if (GConfig) {
    FString IniIP;
    if (GConfig->GetString(TEXT("FPM.Server"), TEXT("ServerIP"), IniIP,
                           IniPath)) {
      ServerIP = IniIP;
    }

    int32 IniPort = ServerPort;
    if (GConfig->GetInt(TEXT("FPM.Server"), TEXT("ServerPort"), IniPort,
                        IniPath)) {
      ServerPort = IniPort;
    }

    bool IniAutoConnect = bAutoConnect;
    if (GConfig->GetBool(TEXT("FPM.Server"), TEXT("bAutoConnect"),
                         IniAutoConnect, IniPath)) {
      bAutoConnect = IniAutoConnect;
    }
  }

  UE_LOG(LogTemp, Log,
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

  UE_LOG(LogTemp, Warning,
         TEXT("FPM: Connecting to dedicated server at %s ..."), *TravelURL);

  // Get the first local player controller and travel to the server
  APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0);
  if (PC) {
    PC->ClientTravel(TravelURL, TRAVEL_Absolute);
  } else {
    UE_LOG(LogTemp, Error,
           TEXT("FPM: Cannot connect — no local PlayerController found!"));
  }
}
