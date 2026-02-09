// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMPlayerCharacter, Log, All);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

AFPMPlayerCharacter::AFPMPlayerCharacter() {
  // Replicate this actor to all clients
  bReplicates = true;

  // Visible body mesh — simple sphere for prototype visibility
  BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
  BodyMesh->SetupAttachment(GetCapsuleComponent());
  BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
  BodyMesh->SetWorldScale3D(FVector(0.75f, 0.75f, 1.5f)); // Capsule-ish shape
  BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

  static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
      TEXT("/Engine/BasicShapes/Sphere"));
  if (SphereMesh.Succeeded()) {
    BodyMesh->SetStaticMesh(SphereMesh.Object);
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

void AFPMPlayerCharacter::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  if (IsLocallyControlled()) {
    HandleMovementInput(DeltaTime);
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
