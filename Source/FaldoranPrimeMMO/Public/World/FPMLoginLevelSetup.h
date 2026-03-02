// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPMLoginLevelSetup.generated.h"


class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

/**
 * AFPMLoginLevelSetup
 *
 * Placed in the Login Level. On BeginPlay it configures the scene lighting
 * to a dramatic "Golden Hour" atmosphere:
 *
 *   - Sun angle near the horizon (warm, raking light)
 *   - Warm orange/amber sky tint
 *   - Light exponential fog for depth / atmospheric perspective
 *   - Sky light intensity boosted for a bright, hopeful ambience
 *
 * All lighting values are UPROPERTY-exposed so you can tweak them in the
 * Details panel without recompiling. This actor owns its own light
 * components so no pre-existing level actors are required — just drop it in.
 *
 * NOTE: The actor sets itself to tick so the sun can slowly crawl across
 * the horizon during the login screen (a subtle 0.01°/s drift).
 */
UCLASS(Blueprintable, BlueprintType)
class FALDORANPRIMEMMO_API AFPMLoginLevelSetup : public AActor {
  GENERATED_BODY()

public:
  AFPMLoginLevelSetup();

  virtual void BeginPlay() override;
  virtual void Tick(float DeltaTime) override;

  // ---- Sun / Directional Light ----

  /** Sun pitch (negative = above horizon). -8° ≈ golden hour sweet spot. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sun",
            meta = (ClampMin = "-90.0", ClampMax = "0.0"))
  float SunPitchDegrees = -8.0f;

  /** Sun yaw (compass direction the light comes FROM). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sun",
            meta = (ClampMin = "0.0", ClampMax = "360.0"))
  float SunYawDegrees = 225.0f;

  /** Sun colour temperature in Kelvin. 3200 K = warm golden/amber. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sun",
            meta = (ClampMin = "1700.0", ClampMax = "12000.0"))
  float SunColorTemperatureK = 3800.0f;

  /** Sun intensity in lux. 100000 is noon sun; ~25000 for golden hour. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sun",
            meta = (ClampMin = "0.0", ClampMax = "200000.0"))
  float SunIntensityLux = 22000.0f;

  /**
   * Degrees per second the sun drifts during the login screen.
   * 0.005 °/s ≈ subtle, barely perceptible.  0 to disable.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sun",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float SunDriftDegreesPerSecond = 0.005f;

  // ---- Sky Atmosphere ----

  /** Rayleigh scattering scale — higher = more blue spread. Default 0.0331. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sky",
            meta = (ClampMin = "0.001", ClampMax = "0.1"))
  float RayleighScatteringScale = 0.0331f;

  /** Mie scattering — controls haze/glow around the sun. 0.003 = dramatic. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Sky",
            meta = (ClampMin = "0.0", ClampMax = "0.1"))
  float MieScatteringScale = 0.003991f;

  // ---- Sky Light ----

  /** Sky light intensity. 1.5 = bright, hopeful sky. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|SkyLight",
            meta = (ClampMin = "0.0", ClampMax = "10.0"))
  float SkyLightIntensity = 1.5f;

  // ---- Fog ----

  /** Exponential height fog density. 0.00005 = very light haze. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Fog",
            meta = (ClampMin = "0.0", ClampMax = "0.01"))
  float FogDensity = 0.00005f;

  /** Fog inscattering colour (the tint of the haze). Warm amber. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Fog")
  FLinearColor FogInscatteringColor = FLinearColor(0.85f, 0.55f, 0.25f, 1.0f);

  /** Height falloff — how quickly fog thins with altitude. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere|Fog",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float FogHeightFalloff = 0.2f;

  // ---- Components ----

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<UDirectionalLightComponent> SunLight;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<USkyLightComponent> SkyLight;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<UExponentialHeightFogComponent> HeightFog;

private:
  /** Apply all current property values to the light components. */
  void ApplyAtmosphereSettings();

  /** Accumulated sun drift (degrees). */
  float SunDriftAccumulated = 0.0f;
};
