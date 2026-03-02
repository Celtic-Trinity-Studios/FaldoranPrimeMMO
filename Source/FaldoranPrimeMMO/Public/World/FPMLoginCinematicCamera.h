// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPMLoginCinematicCamera.generated.h"


class UCineCameraComponent;
class USplineComponent;

/**
 * AFPMLoginCinematicCamera
 *
 * Drop this actor into the Login Level. It slowly pans the camera along a
 * Spline path, looking toward a configurable focal target while the
 * procedural terrain generates behind the login widget.
 *
 * How to use (Editor setup):
 *   1. Place this actor in the Login Level.
 *   2. Add spline points (Edit Spline in Viewport) to define the dolly path.
 *   3. Set FocalTarget to a point in the level you want the camera to gaze at,
 *      or leave zero for auto-gaze-forward-along-spline.
 *   4. In your LoginLevel GameMode/PlayerController, call
 *      AttachPlayerCameraToLoginCamera() to view through this camera.
 *
 * The camera uses a CineCamera component with:
 *   - Filmback: 2.39:1 anamorphic (cinematic widescreen look)
 *   - Focal length: 35mm (moderate wide — great for terrain establishing shots)
 *   - DoF: subtle, focused at FocalTarget distance
 */
UCLASS(Blueprintable, BlueprintType)
class FALDORANPRIMEMMO_API AFPMLoginCinematicCamera : public AActor {
  GENERATED_BODY()

public:
  AFPMLoginCinematicCamera();

  virtual void BeginPlay() override;
  virtual void Tick(float DeltaTime) override;

  // ---- Configuration ----

  /** Dolly speed along the spline in cm/s. Default 40 cm/s = ~1.4 km/hour. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic|Motion",
            meta = (ClampMin = "0.0", ClampMax = "500.0"))
  float DollySpeed = 40.0f;

  /**
   * World-space focal point the camera gazes toward.
   * If left at zero the camera simply looks forward along the spline tangent.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic|Focus")
  FVector FocalTarget = FVector::ZeroVector;

  /**
   * When true the camera loops seamlessly back to the start of the spline
   * when it reaches the end. When false it ping-pongs (reverses direction).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic|Motion")
  bool bLoopSpline = true;

  /** Focal length in mm (35 = wide, 50 = natural, 85 = mild telephoto). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic|Lens",
            meta = (ClampMin = "10.0", ClampMax = "200.0"))
  float FocalLengthMM = 35.0f;

  /**
   * Depth-of-field f-stop. Lower = shallower DoF (more blur).
   * Default 5.6 gives a gentle background separation without overdoing it.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic|Lens",
            meta = (ClampMin = "1.0", ClampMax = "22.0"))
  float Aperture = 5.6f;

  // ---- Components ----

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<UCineCameraComponent> CineCamera;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<USplineComponent> DollySpline;

private:
  /** Current normalised distance along spline [0, SplineLength]. */
  float SplineDistance = 0.0f;

  /** Dolly direction: +1 forward, -1 backward (for ping-pong mode). */
  float DollyDirection = 1.0f;

  /** Configure the CineCamera filmback, lens, and DoF settings. */
  void ConfigureLens();

  /** Smoothly interpolate camera rotation toward target. */
  FRotator ComputeDesiredRotation(const FVector &CamPos,
                                  const FVector &SplineTangent) const;
};
