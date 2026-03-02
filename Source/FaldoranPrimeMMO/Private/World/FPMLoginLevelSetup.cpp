// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMLoginLevelSetup.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMLoginSetup, Log, All);

// =====================================================================
//  Constructor
// =====================================================================

AFPMLoginLevelSetup::AFPMLoginLevelSetup() {
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickInterval = 0.1f; // 10 Hz — sun drift is very slow

  USceneComponent *Root =
      CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
  SetRootComponent(Root);

  // Directional light — the Sun
  SunLight =
      CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
  SunLight->SetupAttachment(Root);
  SunLight->SetAtmosphereSunLight(true); // Required for SkyAtmosphere

  // Sky atmosphere — physically-based sky
  SkyAtmosphere =
      CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
  SkyAtmosphere->SetupAttachment(Root);

  // Sky light — indirect/ambient fill from the sky
  SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
  SkyLight->SetupAttachment(Root);
  SkyLight->SetMobility(EComponentMobility::Movable);
  SkyLight->bRealTimeCapture = true; // Keeps sky light in sync as sun moves

  // Exponential height fog — atmospheric depth
  HeightFog =
      CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
  HeightFog->SetupAttachment(Root);
}

// =====================================================================
//  BeginPlay
// =====================================================================

void AFPMLoginLevelSetup::BeginPlay() {
  Super::BeginPlay();

  ApplyAtmosphereSettings();

  UE_LOG(LogFPMLoginSetup, Log,
         TEXT("FPM Login Setup: Golden Hour atmosphere applied. "
              "Sun pitch=%.1f° yaw=%.1f° temp=%.0fK intensity=%.0f lux"),
         SunPitchDegrees, SunYawDegrees, SunColorTemperatureK, SunIntensityLux);
}

// =====================================================================
//  Tick — drift the sun very slowly
// =====================================================================

void AFPMLoginLevelSetup::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  if (SunDriftDegreesPerSecond <= 0.0f || !SunLight) {
    return;
  }

  SunDriftAccumulated += SunDriftDegreesPerSecond * DeltaTime;
  // Drift the yaw (azimuth) — keeps pitch constant for a stable golden hour
  const float NewYaw = FMath::Fmod(SunYawDegrees + SunDriftAccumulated, 360.0f);
  SunLight->SetWorldRotation(FRotator(SunPitchDegrees, NewYaw, 0.0f));
}

// =====================================================================
//  ApplyAtmosphereSettings
// =====================================================================

void AFPMLoginLevelSetup::ApplyAtmosphereSettings() {
  // ---- Directional Light (Sun) ----
  if (SunLight) {
    SunLight->SetWorldRotation(FRotator(SunPitchDegrees, SunYawDegrees, 0.0f));
    SunLight->SetIntensity(SunIntensityLux);

    // Use temperature mode for physically-correct warm light
    SunLight->bUseTemperature = true;
    SunLight->Temperature = SunColorTemperatureK;
    SunLight->SetLightColor(FLinearColor::White); // Temperature drives the hue

    // Cascade shadow quality
    SunLight->DynamicShadowDistanceMovableLight =
        20000.0f; // 200 m shadow range
    SunLight->CascadeDistributionExponent = 3.0f;
    // Soft shadow for an outdoor feel
    SunLight->LightSourceAngle = 0.5357f; // matches real sun angular diameter
  }

  // ---- Sky Atmosphere ----
  if (SkyAtmosphere) {
    SkyAtmosphere->RayleighScatteringScale = RayleighScatteringScale;
    SkyAtmosphere->MieScatteringScale = MieScatteringScale;

    // Aerosol absorption — a touch of haze for that dusty golden-hour look
    SkyAtmosphere->MieAbsorptionScale = 0.000896f;

    // Ground radius matches UE's Earth default (6371 km)
    SkyAtmosphere->BottomRadius = 6360.0f;
    SkyAtmosphere->AtmosphereHeight = 60.0f; // km — standard

    // Ozone layer contribution for the blue upper sky gradient
    SkyAtmosphere->OtherAbsorptionScale = 1.0f;
  }

  // ---- Sky Light ----
  if (SkyLight) {
    SkyLight->SetIntensity(SkyLightIntensity);
    // Lower bound colour — warm fill from below (ground reflects warm light)
    SkyLight->LowerHemisphereColor = FLinearColor(0.15f, 0.08f, 0.04f);
  }

  // ---- Exponential Height Fog ----
  if (HeightFog) {
    HeightFog->SetFogDensity(FogDensity);
    HeightFog->SetFogInscatteringColor(FogInscatteringColor);
    HeightFog->SetFogHeightFalloff(FogHeightFalloff);
    // Start distance: don't fog up close-range detail
    HeightFog->SetStartDistance(1000.0f); // 10 m
    // Directional glow toward the sun via inscattering exponent
    HeightFog->DirectionalInscatteringExponent = 4.0f;
    HeightFog->DirectionalInscatteringStartDistance = 10000.0f;
  }
}
