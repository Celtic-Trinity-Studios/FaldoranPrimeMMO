// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "World/FPMChunkData.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMPlayerCharacter, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

AFPMPlayerCharacter::AFPMPlayerCharacter() {
  // Replicate this actor to all clients
  bReplicates = true;

  // Configure the inherited skeletal mesh
  if (GetMesh()) {
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    if (MannyMesh.Succeeded()) {
      GetMesh()->SetSkeletalMeshAsset(MannyMesh.Object);
    }
  }

  // Spring arm for third-person camera
  CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
  CameraBoom->SetupAttachment(GetCapsuleComponent());
  CameraBoom->TargetArmLength = 400.0f;
  CameraBoom->bUsePawnControlRotation = true;

  // Camera attached to the boom
  FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
  FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
  FollowCamera->bUsePawnControlRotation = false;

  // Character movement defaults
  if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
    CMC->bOrientRotationToMovement = true;
    CMC->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    CMC->MaxWalkSpeed = 600.0f;

    // Reduce "ServerMove TimeStamp expired" warnings in PIE listen-server.
    // These are cosmetic — timestamps desync when client+server share a
    // process.
    CMC->NetworkMaxSmoothUpdateDistance = 256.0f;
    CMC->NetworkSimulatedSmoothLocationTime = 0.2f;
  }

  // Don't rotate character with controller pitch/roll
  bUseControllerRotationPitch = false;
  bUseControllerRotationYaw = false;
  bUseControllerRotationRoll = false;
}

void AFPMPlayerCharacter::BeginPlay() {
  Super::BeginPlay();

  UE_LOG(LogFPMPlayerCharacter, Log,
         TEXT("FPM: PlayerCharacter spawned — Name='%s', Local=%s"),
         *CharacterName, IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

// --- Compass helper ---
static FString YawToCompass(float Yaw) {
  // Normalize to 0-360
  while (Yaw < 0.f)
    Yaw += 360.f;
  while (Yaw >= 360.f)
    Yaw -= 360.f;

  // UE: 0=X+ (forward), 90=Y+ (right)
  // Map to compass: N=+X, E=+Y, S=-X, W=-Y
  FString Dir;
  if (Yaw >= 337.5f || Yaw < 22.5f)
    Dir = TEXT("N");
  else if (Yaw < 67.5f)
    Dir = TEXT("NE");
  else if (Yaw < 112.5f)
    Dir = TEXT("E");
  else if (Yaw < 157.5f)
    Dir = TEXT("SE");
  else if (Yaw < 202.5f)
    Dir = TEXT("S");
  else if (Yaw < 247.5f)
    Dir = TEXT("SW");
  else if (Yaw < 292.5f)
    Dir = TEXT("W");
  else
    Dir = TEXT("NW");

  return FString::Printf(TEXT("%s (%.0f°)"), *Dir, Yaw);
}

void AFPMPlayerCharacter::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  if (IsLocallyControlled()) {
    HandleMovementInput(DeltaTime);

    // --- Debug HUD: Compass + Position ---
    if (GEngine) {
      const FVector Pos = GetActorLocation();
      const APlayerController *PC = Cast<APlayerController>(GetController());
      const float Yaw =
          PC ? PC->GetControlRotation().Yaw : GetActorRotation().Yaw;

      // Chunk coordinate
      const FFPMChunkCoord Chunk = FPMChunkGenerator::WorldToChunkCoord(Pos);

      // Line 1: Compass
      GEngine->AddOnScreenDebugMessage(
          -1, 0.f, FColor::Cyan,
          FString::Printf(TEXT("Compass: %s"), *YawToCompass(Yaw)));

      // Line 2: World position
      GEngine->AddOnScreenDebugMessage(
          -2, 0.f, FColor::Yellow,
          FString::Printf(TEXT("Pos: X=%.0f  Y=%.0f  Z=%.0f"), Pos.X, Pos.Y,
                          Pos.Z));

      // Line 3: Chunk coordinate
      GEngine->AddOnScreenDebugMessage(
          -3, 0.f, FColor::Green,
          FString::Printf(TEXT("Chunk: (%d, %d)"), Chunk.X, Chunk.Y));
    }
  }

  // --- Fall-through recovery (server-authoritative) ---
  // If the character falls below a safe threshold, trace from high above
  // to find the terrain surface and teleport back up.
  if (HasAuthority()) {
    const FVector Loc = GetActorLocation();

    // Only recover if significantly below sea level (allow normal falling)
    constexpr float FallThroughThreshold = -3000.0f;

    if (Loc.Z < FallThroughThreshold) {
      // Trace from far above the character's XY to find terrain
      const FVector TraceStart(Loc.X, Loc.Y, 50000.0f);
      const FVector TraceEnd(Loc.X, Loc.Y, -10000.0f);

      FHitResult Hit;
      FCollisionQueryParams Params;
      Params.AddIgnoredActor(this);

      if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd,
                                               ECC_WorldStatic, Params)) {
        // Place character on top of terrain + half capsule height
        const float CapsuleHalf =
            GetCapsuleComponent()
                ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
                : 90.0f;

        const FVector SafePos(Hit.ImpactPoint.X, Hit.ImpactPoint.Y,
                              Hit.ImpactPoint.Z + CapsuleHalf + 10.0f);

        SetActorLocation(SafePos);

        // Reset velocity to prevent immediately falling through again
        if (GetCharacterMovement()) {
          GetCharacterMovement()->StopMovementImmediately();
        }

        UE_LOG(LogFPMPlayerCharacter, Warning,
               TEXT("FPM: Fall-through recovery — teleported from Z=%.0f to "
                    "Z=%.0f"),
               Loc.Z, SafePos.Z);
      } else {
        // No terrain found — emergency teleport to world origin
        SetActorLocation(FVector(0.0f, 0.0f, 5000.0f));

        if (GetCharacterMovement()) {
          GetCharacterMovement()->StopMovementImmediately();
        }

        UE_LOG(LogFPMPlayerCharacter, Error,
               TEXT("FPM: Fall-through recovery — no terrain found, "
                    "teleported to world origin"));
      }
    }
  }
}

// -------------------------------------------------------------------
// Replication
// -------------------------------------------------------------------

void AFPMPlayerCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  DOREPLIFETIME(AFPMPlayerCharacter, CharacterName);
  DOREPLIFETIME(AFPMPlayerCharacter, BodyType);
  DOREPLIFETIME(AFPMPlayerCharacter, SkinTone);
  DOREPLIFETIME(AFPMPlayerCharacter, HairColor);
}

// -------------------------------------------------------------------
// Server-Side Initialization
// -------------------------------------------------------------------

void AFPMPlayerCharacter::InitializeAppearance(
    const FString &InName, uint8 InBodyType, const FLinearColor &InSkinTone,
    const FLinearColor &InHairColor) {
  CharacterName = InName;
  BodyType = InBodyType;
  SkinTone = InSkinTone;
  HairColor = InHairColor;

  // Apply immediately on the server; clients get it via OnRep
  ApplyAppearance();

  UE_LOG(LogFPMPlayerCharacter, Log,
         TEXT("FPM: Appearance initialized — Name='%s', Body=%d, "
              "Skin=(%.2f,%.2f,%.2f), Hair=(%.2f,%.2f,%.2f)"),
         *CharacterName, BodyType, SkinTone.R, SkinTone.G, SkinTone.B,
         HairColor.R, HairColor.G, HairColor.B);
}

// -------------------------------------------------------------------
// OnRep Callbacks
// -------------------------------------------------------------------

void AFPMPlayerCharacter::OnRep_CharacterName() {
  UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: OnRep_CharacterName — '%s'"),
         *CharacterName);
  // Future: update nameplate widget
}

void AFPMPlayerCharacter::OnRep_BodyType() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_SkinTone() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_HairColor() { ApplyAppearance(); }

// -------------------------------------------------------------------
// Appearance Application
// -------------------------------------------------------------------

void AFPMPlayerCharacter::ApplyAppearance() {
  // PROTOTYPE: Log appearance. Full material-instance approach deferred
  // until Mutable/CC5 integration.
  UE_LOG(LogFPMPlayerCharacter, Verbose,
         TEXT("FPM: ApplyAppearance — Body=%d, Skin=(%.2f,%.2f,%.2f), "
              "Hair=(%.2f,%.2f,%.2f)"),
         BodyType, SkinTone.R, SkinTone.G, SkinTone.B, HairColor.R, HairColor.G,
         HairColor.B);
}

// -------------------------------------------------------------------
// Input Handling
// -------------------------------------------------------------------

void AFPMPlayerCharacter::HandleMovementInput(float DeltaTime) {
  APlayerController *PC = Cast<APlayerController>(GetController());
  if (!PC) {
    return;
  }

  FVector MoveInput = FVector::ZeroVector;
  if (PC->IsInputKeyDown(EKeys::W))
    MoveInput.X += 1.0f;
  if (PC->IsInputKeyDown(EKeys::S))
    MoveInput.X -= 1.0f;
  if (PC->IsInputKeyDown(EKeys::D))
    MoveInput.Y += 1.0f;
  if (PC->IsInputKeyDown(EKeys::A))
    MoveInput.Y -= 1.0f;

  if (!MoveInput.IsZero()) {
    const FRotator ControlRot = PC->GetControlRotation();
    const FRotator YawOnlyRot(0.0f, ControlRot.Yaw, 0.0f);

    const FVector ForwardDir =
        FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDir, MoveInput.X);
    AddMovementInput(RightDir, MoveInput.Y);
  }

  // Mouse look
  float MouseX = 0.0f;
  float MouseY = 0.0f;
  PC->GetInputMouseDelta(MouseX, MouseY);
  AddControllerYawInput(MouseX);
  AddControllerPitchInput(-MouseY);
}
