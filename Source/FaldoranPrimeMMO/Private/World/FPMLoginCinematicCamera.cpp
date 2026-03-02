// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMLoginCinematicCamera.h"
#include "CineCameraComponent.h" // CinematicCamera module
#include "Components/SplineComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMLoginCamera, Log, All);

// =====================================================================
//  Constructor
// =====================================================================

AFPMLoginCinematicCamera::AFPMLoginCinematicCamera() {
  PrimaryActorTick.bCanEverTick = true;
  // Run every frame — dolly movement needs per-frame update
  PrimaryActorTick.TickGroup = TG_PostUpdateWork;

  // Root component — keep as default scene root
  USceneComponent *Root =
      CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
  SetRootComponent(Root);

  // Spline defines the dolly track. Author it in-editor via Edit Spline.
  DollySpline = CreateDefaultSubobject<USplineComponent>(TEXT("DollySpline"));
  DollySpline->SetupAttachment(Root);
  DollySpline->SetClosedLoop(false); // We handle looping manually

  // CineCamera — attached to scene root; we move it along the spline in Tick
  CineCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("CineCamera"));
  CineCamera->SetupAttachment(Root);
}

// =====================================================================
//  BeginPlay
// =====================================================================

void AFPMLoginCinematicCamera::BeginPlay() {
  Super::BeginPlay();

  // Place the camera at the start of the spline immediately
  SplineDistance = 0.0f;
  DollyDirection = 1.0f;

  ConfigureLens();

  UE_LOG(LogFPMLoginCamera, Log,
         TEXT("FPM Login Camera: Cinematic dolly started. SplineLength=%.0f cm "
              "Speed=%.0f cm/s Lens=%.0fmm f/%.1f"),
         DollySpline->GetSplineLength(), DollySpeed, FocalLengthMM, Aperture);
}

// =====================================================================
//  Tick — advance camera along spline
// =====================================================================

void AFPMLoginCinematicCamera::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  const float SplineLength = DollySpline->GetSplineLength();
  if (SplineLength < SMALL_NUMBER || DollySpeed <= 0.0f) {
    return;
  }

  // Advance distance
  SplineDistance += DollySpeed * DollyDirection * DeltaTime;

  if (bLoopSpline) {
    // Seamless loop: wrap around
    if (SplineDistance > SplineLength) {
      SplineDistance -= SplineLength;
    } else if (SplineDistance < 0.0f) {
      SplineDistance += SplineLength;
    }
  } else {
    // Ping-pong: reverse at endpoints
    if (SplineDistance >= SplineLength) {
      SplineDistance = SplineLength;
      DollyDirection = -1.0f;
    } else if (SplineDistance <= 0.0f) {
      SplineDistance = 0.0f;
      DollyDirection = 1.0f;
    }
  }

  // Sample spline position and tangent (world space)
  const FVector NewPos =
      DollySpline->GetWorldLocationAtDistanceAlongSpline(SplineDistance);
  const FVector Tangent =
      DollySpline->GetWorldDirectionAtDistanceAlongSpline(SplineDistance);

  // Compute desired rotation — toward FocalTarget or along tangent
  const FRotator NewRot = ComputeDesiredRotation(NewPos, Tangent);

  // Apply to camera (not the entire actor, so the spline gizmo stays in place)
  CineCamera->SetWorldLocationAndRotation(NewPos, NewRot);
}

// =====================================================================
//  ComputeDesiredRotation
// =====================================================================

FRotator AFPMLoginCinematicCamera::ComputeDesiredRotation(
    const FVector &CamPos, const FVector &SplineTangent) const {
  if (!FocalTarget.IsNearlyZero()) {
    // Look toward the focal target
    const FVector ToTarget = (FocalTarget - CamPos).GetSafeNormal();
    // Smoothly blend: 80% focal target, 20% spline tangent for slight motion
    const FVector BlendedDir =
        (ToTarget * 0.8f + SplineTangent * 0.2f).GetSafeNormal();
    return BlendedDir.Rotation();
  }
  // Default: look along spline tangent
  return SplineTangent.Rotation();
}

// =====================================================================
//  ConfigureLens
// =====================================================================

void AFPMLoginCinematicCamera::ConfigureLens() {
  if (!CineCamera) {
    return;
  }

  // Filmback: 2.39:1 anamorphic for a wide cinematic establishing shot feel.
  // These values match the Panavision Anamorphic 35mm format.
  CineCamera->Filmback.SensorWidth = 54.12f; // mm
  CineCamera->Filmback.SensorHeight =
      25.416f; // mm  (≈ 2.13:1 native, crops to 2.39)

  // Lens
  CineCamera->CurrentFocalLength = FocalLengthMM;
  CineCamera->CurrentAperture = Aperture;

  // Focus distance: if a focal target is set, derive distance from it.
  // Otherwise default to 5000 cm (50 m) — good for mid-ground terrain.
  if (!FocalTarget.IsNearlyZero() &&
      CineCamera->GetComponentLocation() != FVector::ZeroVector) {
    const float Dist =
        FVector::Dist(CineCamera->GetComponentLocation(), FocalTarget);
    CineCamera->FocusSettings.ManualFocusDistance = FMath::Max(Dist, 100.0f);
  } else {
    CineCamera->FocusSettings.ManualFocusDistance = 5000.0f;
  }

  CineCamera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;

  UE_LOG(
      LogFPMLoginCamera, Verbose,
      TEXT("FPM Login Camera: Lens configured — %.0fmm f/%.1f, focus=%.0fcm"),
      FocalLengthMM, Aperture, CineCamera->FocusSettings.ManualFocusDistance);
}
