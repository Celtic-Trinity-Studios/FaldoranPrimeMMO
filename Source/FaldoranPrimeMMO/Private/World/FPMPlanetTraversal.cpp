// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMPlanetTraversal.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"
#include "World/FPMTerrainShell.h"


// ===================================================================
//  Constructor / BeginPlay
// ===================================================================
UFPMPlanetTraversal::UFPMPlanetTraversal() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFPMPlanetTraversal::BeginPlay() { Super::BeginPlay(); }

// ===================================================================
//  Helpers
// ===================================================================
float UFPMPlanetTraversal::GetCurrentSpeed() const {
  switch (CurrentSpeedTier) {
  case 0:
    return 0.f;
  case 1:
    return SpeedGlide;
  case 2:
    return SpeedBoost;
  case 3:
    return SpeedHypersonic;
  case 4:
    return SpeedRift;
  default:
    return SpeedGlide;
  }
}

FString UFPMPlanetTraversal::GetTierName() const {
  switch (CurrentSpeedTier) {
  case 0:
    return TEXT("Hover");
  case 1:
    return TEXT("Glide");
  case 2:
    return TEXT("Boost");
  case 3:
    return TEXT("Hypersonic");
  case 4:
    return TEXT("Rift Speed");
  default:
    return TEXT("???");
  }
}

// ===================================================================
//  Shell management
// ===================================================================
void UFPMPlanetTraversal::SpawnShellIfNeeded(ACharacter *Char) {
  if (CurrentSpeedTier < 2) {
    DestroyShell();
    return;
  }
  if (TerrainShell.IsValid())
    return;

  if (UWorld *W = Char->GetWorld()) {
    if (AFPMTerrainShell *Shell = W->SpawnActor<AFPMTerrainShell>(
            AFPMTerrainShell::StaticClass(), Char->GetActorLocation(),
            FRotator::ZeroRotator)) {
      Shell->Initialize(WorldSeed);
      TerrainShell = Shell;
    }
  }
}

void UFPMPlanetTraversal::DestroyShell() {
  if (AFPMTerrainShell *Shell = TerrainShell.Get())
    Shell->Destroy();
  TerrainShell = nullptr;
}

// ===================================================================
//  ToggleFlight  (G key)
// ===================================================================
void UFPMPlanetTraversal::ToggleFlight() {
  ACharacter *Character = Cast<ACharacter>(GetOwner());
  if (!Character)
    return;
  UCharacterMovementComponent *CMC = Character->GetCharacterMovement();
  if (!CMC)
    return;

  bIsFlying = !bIsFlying;

  if (bIsFlying) {
    StartPosition = Character->GetActorLocation();
    HoverLockedZ = StartPosition.Z;   // Hover holds this altitude
    PreviousPosition = StartPosition;
    TotalDistanceCm = 0.f;
    FurthestFromStartCm = 0.f;
    bCompletedLoop = false;
    bPassedQuarter = false;

    SavedMovementMode = CMC->MovementMode;
    CMC->SetMovementMode(MOVE_Flying);

    // Set fly speed — use SpeedGlide as minimum so Hover tier still lets
    // the player nudge with inputs without feeling sluggish
    CMC->MaxFlySpeed = (CurrentSpeedTier == 0) ? SpeedGlide : GetCurrentSpeed();

    CMC->BrakingDecelerationFlying = 2048.f; // snappy stops in flight
    CMC->StopMovementImmediately();

    if (UCapsuleComponent *Cap = Character->GetCapsuleComponent())
      Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SetComponentTickEnabled(true);
    SpawnShellIfNeeded(Character);

    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 4.f, FColor::Cyan,
          FString::Printf(TEXT("FLIGHT ON  |  %s  |  [H] speed  [G] land"),
                          *GetTierName()));
    UE_LOG(LogTemp, Log, TEXT("FPM Flight ON  Tier%d %s"), CurrentSpeedTier,
           *GetTierName());

  } else {
    CMC->SetMovementMode(SavedMovementMode);
    CMC->StopMovementImmediately();

    if (UCapsuleComponent *Cap = Character->GetCapsuleComponent())
      Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    SetComponentTickEnabled(false);
    DestroyShell();

    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 4.f, FColor::Yellow,
          FString::Printf(TEXT("FLIGHT OFF  |  %.1f km traveled"),
                          TotalDistanceCm / 100000.f));
    UE_LOG(LogTemp, Log, TEXT("FPM Flight OFF  %.1f km"),
           TotalDistanceCm / 100000.f);
  }
}

// ===================================================================
//  CycleSpeedTier  (H key)
// ===================================================================
void UFPMPlanetTraversal::CycleSpeedTier() {
  CurrentSpeedTier = (CurrentSpeedTier + 1) % 5;

  if (bIsFlying) {
    if (ACharacter *Char = Cast<ACharacter>(GetOwner())) {
      if (UCharacterMovementComponent *CMC = Char->GetCharacterMovement()) {
        CMC->MaxFlySpeed =
            (CurrentSpeedTier == 0) ? SpeedGlide : GetCurrentSpeed();
      }
      SpawnShellIfNeeded(Char);
    }
  }

  const float SpeedKmH = GetCurrentSpeed() / 100000.f * 3600.f;
  const FString Msg =
      (CurrentSpeedTier == 0)
          ? TEXT("Hover  —  use WASD to nudge")
          : FString::Printf(TEXT("Tier %d: %s  —  %.0f km/h"), CurrentSpeedTier,
                            *GetTierName(), SpeedKmH);
  if (GEngine)
    GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
}

// ===================================================================
//  TickComponent
//
//  ALL direction control is handled by UE's CharacterMovementComponent
//  (MOVE_Flying + player input). We only:
//    - HOVER tier: cancel velocity and hold altitude via gentle Z lerp
//    - ALL tiers:  track distance traveled for the world-loop counter
// ===================================================================
void UFPMPlanetTraversal::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
  if (!bIsFlying)
    return;

  ACharacter *Character = Cast<ACharacter>(GetOwner());
  if (!Character)
    return;

  UCharacterMovementComponent *CMC = Character->GetCharacterMovement();
  const FVector Pos = Character->GetActorLocation();

  // --- Hover: zero velocity + hold altitude ---
  if (CurrentSpeedTier == 0) {
    if (CMC)
      CMC->StopMovementImmediately();

    // Lock to the altitude captured when flight was activated
    if (FMath::Abs(Pos.Z - HoverLockedZ) > 5.f) {
      FVector HoverPos = Pos;
      HoverPos.Z = FMath::FInterpTo(Pos.Z, HoverLockedZ, DeltaTime, 8.f);
      Character->SetActorLocation(HoverPos, false, nullptr,
                                  ETeleportType::TeleportPhysics);
    }

    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          100, 0.f, FColor::Cyan, TEXT("HOVER  |  [H] speed  [G] land"));
    return;
  }

  // --- Glide / Boost / Hypersonic / Rift: CMC handles ALL movement ---
  // Player uses WASD + mouse look to fly in any direction at MaxFlySpeed.
  // Nothing to override here — just track distance and show HUD.

  // Distance tracking
  const float WDX = FPMChunkConstants::WrappedDelta(PreviousPosition.X, Pos.X);
  const float WDY = FPMChunkConstants::WrappedDelta(PreviousPosition.Y, Pos.Y);
  TotalDistanceCm += FMath::Sqrt(WDX * WDX + WDY * WDY);
  PreviousPosition = Pos;

  const float DistFromStart =
      FPMChunkConstants::WrappedDistance2D(Pos, StartPosition);
  if (DistFromStart > FurthestFromStartCm)
    FurthestFromStartCm = DistFromStart;

  // Loop milestones
  constexpr float QuarterCirc = FPMChunkConstants::HalfCircumferenceCm * 0.5f;
  if (!bPassedQuarter && FurthestFromStartCm > QuarterCirc) {
    bPassedQuarter = true;
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
                                       TEXT("25% of world circumference!"));
  }
  if (bPassedQuarter && !bCompletedLoop && DistFromStart < 200000.f &&
      TotalDistanceCm > FPMChunkConstants::PlanetCircumferenceCm * 0.5f) {
    bCompletedLoop = true;
    UE_LOG(LogTemp, Warning, TEXT("FPM: *** PLANET LOOP COMPLETE! ***"));
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 10.f, FColor::Green,
          FString::Printf(TEXT("*** PLANET LOOP! *** %.0f km"),
                          TotalDistanceCm / 100000.f));
  }

  // Persistent HUD
  if (GEngine) {
    // Show actual velocity, not MaxFlySpeed, so player sees real movement speed
    const float ActualKmH =
        CMC ? CMC->Velocity.Size() / 100000.f * 3600.f : 0.f;
    const float CircPct =
        (TotalDistanceCm / FPMChunkConstants::PlanetCircumferenceCm) * 100.f;
    const FColor Col = bCompletedLoop ? FColor::Green : FColor::Cyan;

    GEngine->AddOnScreenDebugMessage(
        100, 0.f, Col,
        FString::Printf(TEXT("FLIGHT  |  %s  |  %.0f km/h"), *GetTierName(),
                        ActualKmH));
    GEngine->AddOnScreenDebugMessage(
        101, 0.f, Col,
        FString::Printf(TEXT("%.1f km  (%.2f%% of world)"),
                        TotalDistanceCm / 100000.f, CircPct));
  }
}

// ===================================================================
//  CalcCompassHeading
// ===================================================================
FString UFPMPlanetTraversal::CalcCompassHeading(const FVector &Dir) const {
  if (Dir.Size2D() < 0.01f)
    return TEXT("--");
  const float A = FMath::Atan2(Dir.X, -Dir.Y) * (180.f / PI);
  const float H = FMath::Fmod(A + 360.f, 360.f);
  if (H < 22.5f || H >= 337.5f)
    return TEXT("N");
  if (H < 67.5f)
    return TEXT("NE");
  if (H < 112.5f)
    return TEXT("E");
  if (H < 157.5f)
    return TEXT("SE");
  if (H < 202.5f)
    return TEXT("S");
  if (H < 247.5f)
    return TEXT("SW");
  if (H < 292.5f)
    return TEXT("W");
  return TEXT("NW");
}
