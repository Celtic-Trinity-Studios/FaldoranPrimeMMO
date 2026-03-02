// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include <atomic>
#include "FPMHUD.generated.h"

UCLASS()
class FALDORANPRIMEMMO_API AFPMHUD : public AHUD {
  GENERATED_BODY()

public:
  AFPMHUD();
  virtual void DrawHUD() override;

  UFUNCTION(BlueprintCallable, Category = "Debug")
  void RefreshBiomeCache(int32 WorldSeed);

  /** Toggle the world map overlay on/off (M key). */
  void ToggleWorldMap(int32 WorldSeed);

private:
  float DrawHUDLine(float X, float Y, const FString &Text,
                    const FLinearColor &Color, UFont *Font, float Scale) const;

  void DrawBiomeTeleportPanel(APlayerController *PC,
                              class AFPMPlayerCharacter *Char, float TopY);

  void DrawRiftRunnerPanel(APlayerController *PC,
                           class AFPMPlayerCharacter *Char);

  void DrawWorldMapOverlay(APlayerController *PC,
                           class AFPMPlayerCharacter *Char);

  void TeleportToBiome(uint8 BiomeIndex);

  // --- World Map ---
  void BeginWorldMapGeneration(int32 WorldSeed);

  bool bWorldMapOpen    = false;
  bool bWorldMapPending = false; // background gen in flight
  bool bWorldMapReady   = false;

  // ---------------------------------------------------------------
  //  Biome location cache
  // ---------------------------------------------------------------
  struct FBiomeLocation {
    FVector2D WorldXY;
    bool bFound = false;
  };

  /** Live map read by DrawHUD every frame (game thread only). */
  TMap<uint8, FBiomeLocation> BiomeLocations;
  /** Staging map written by the worker thread. */
  TMap<uint8, FBiomeLocation> StagingLocations;

  bool bCacheBuilt = false;
  bool bCacheBuilding = false; // worker in flight
  std::atomic<bool> bStagingReady{false};
  int32 CachedWorldSeed = 0;

  /** World map render texture (1024x1024, RGBA8). */
  UPROPERTY(Transient)
  TObjectPtr<UTexture2D> WorldMapTexture;

  /** Staging pixel buffer written by background thread. */
  TArray<FColor> StagingMapPixels;
  std::atomic<bool> bMapStagingReady{false};
  int32 WorldMapSeed = 0;
};
