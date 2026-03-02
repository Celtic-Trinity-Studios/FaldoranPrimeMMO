// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Player/FPMPlayerCharacter.h"
#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Character/FPMSpeciesDataAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/FPMInteractionComponent.h"
#include "Gameplay/FPMInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "World/FPMChunkData.h"
#include "World/FPMPlanetTraversal.h"
#include "World/FPMVoxelChunk.h"
#include "World/FPMWorldChunkManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMPlayerCharacter, Log, All);

// Morph target names (must match CC5 morph target names)
const FName AFPMPlayerCharacter::MorphName_Jaw(TEXT("Jaw_Width"));
const FName AFPMPlayerCharacter::MorphName_Nose(TEXT("Nose_Bridge"));
const FName AFPMPlayerCharacter::MorphName_Brow(TEXT("Brow_Ridge"));
const FName AFPMPlayerCharacter::MorphName_Lips(TEXT("Lip_Fullness"));

// -------------------------------------------------------------------
// Species Scaling Data
// -------------------------------------------------------------------

// -------------------------------------------------------------------
// Species Scaling — Fallback Table
// Used when no UFPMSpeciesRegistry data asset is assigned.
// All values match UFPMSpeciesDataAsset field semantics.
// Walk/Run speeds are absolute cm/s (NOT multipliers).
// Indexed by EFPMSpecies cast to int32.
// -------------------------------------------------------------------

struct FFallbackSpeciesData {
  float MeshScale;         // Uniform scale for the skeletal mesh
  float CapsuleHalfHeight; // Capsule half-height (cm)
  float CapsuleRadius;     // Capsule radius (cm)
  float BaseWalkSpeed;     // Normal walk speed (cm/s) — avg human = 150
  float BaseRunSpeed;      // Run speed (cm/s, hold Shift) — default 500
  float BoomLength;        // Camera spring arm length (cm)
  float JumpMultiplier;    // Multiplier on JumpZVelocity (420 base)
};

// clang-format off
//                                               MeshSc  CapsHH CapsR  Walk  Run   Boom   Jump
static const FFallbackSpeciesData FallbackData[] = {
  // Reference: walk = 1.3 m/s (3 mph), run = 2.6 m/s (6 mph, 2x walk)
  // Jump base 300 cm/s gives ~46cm height (UE default gravity 980 cm/s²)
  // h = v²/(2g)
  /* Human    — reference                    */ {0.50f,  45.0f, 17.0f, 130.f, 260.f, 200.f, 1.00f},
  /* HalfElf  — slightly taller stride       */ {0.525f, 47.0f, 17.0f, 133.f, 266.f, 210.f, 1.00f},
  /* Elf      — tall, elegant stride         */ {0.54f,  48.5f, 16.0f, 137.f, 274.f, 215.f, 1.02f},
  /* Dwarf    — short legs, slower           */ {0.375f, 34.0f, 19.0f, 120.f, 240.f, 160.f, 0.90f},
  /* Halfling — quick scurry, light jump     */ {0.275f, 25.0f, 14.0f, 115.f, 230.f, 140.f, 1.15f},
  /* HalfOrc  — heavy, slightly slower walk  */ {0.575f, 51.5f, 20.0f, 127.f, 254.f, 230.f, 0.95f},
  /* Gnome    — tiny, scurrying pace         */ {0.275f, 25.0f, 13.0f, 118.f, 236.f, 135.f, 1.12f},
  /* Kethari  — cat-race, agile stride       */ {0.475f, 43.0f, 16.0f, 135.f, 270.f, 190.f, 1.05f},
  /* Rauken   — dog-race, steady             */ {0.525f, 47.0f, 18.0f, 130.f, 260.f, 210.f, 1.00f},
};
// clang-format on
static constexpr int32 FallbackDataCount =
    sizeof(FallbackData) / sizeof(FallbackData[0]);

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

AFPMPlayerCharacter::AFPMPlayerCharacter() {
  // Replicate this actor to all clients
  bReplicates = true;

  // Configure the inherited skeletal mesh
  if (GetMesh()) {
    GetMesh()->SetRelativeLocation(
        FVector(0.0f, 0.0f, -45.0f)); // half of original 90
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    // Force animation to always tick (needed for server + remote clients)
    GetMesh()->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    // --- Load CC5 meshes ---
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CC5MaleObj(
        TEXT("SkeletalMesh'/Game/Characters/CC5/CC5_Base_Male.CC5_Base_Male'"));
    if (CC5MaleObj.Succeeded()) {
      CC5MaleMesh = CC5MaleObj.Object;
    }

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CC5FemaleObj(TEXT(
        "SkeletalMesh'/Game/Characters/CC5/CC5_Base_Female.CC5_Base_Female'"));
    if (CC5FemaleObj.Succeeded()) {
      CC5FemaleMesh = CC5FemaleObj.Object;
    }

    // Load idle animation
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleAnimObj(
        TEXT("/Game/Characters/CC5/Motion/idle-378963_idle-378963"));
    if (IdleAnimObj.Succeeded()) {
      CC5IdleAnimation = IdleAnimObj.Object;
    }

    // Default to male CC5 mesh, fall back to Manny if CC5 not available
    if (CC5MaleMesh) {
      GetMesh()->SetSkeletalMeshAsset(CC5MaleMesh);
      GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
      UE_LOG(LogFPMPlayerCharacter, Log,
             TEXT("FPM: CC5_Base_Male loaded for in-world character."));
    } else {
      static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(
          TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
      if (MannyMesh.Succeeded()) {
        GetMesh()->SetSkeletalMeshAsset(MannyMesh.Object);
      }
      UE_LOG(LogFPMPlayerCharacter, Warning,
             TEXT("FPM: CC5 mesh not found, using Manny fallback."));
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

  // Character movement defaults — ApplySpeciesScaling() overrides these
  // once a species is selected; these are sane construction-time values.
  if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
    CMC->bOrientRotationToMovement = true;
    CMC->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    CMC->MaxWalkSpeed = CachedWalkSpeed; // 130 cm/s (1.3 m/s, avg human walk)
    // Base 300 cm/s → h = 300²/(2×980) ≈ 45.9cm (average adult vertical jump)
    CMC->JumpZVelocity = 300.0f;

    // Reduce "ServerMove TimeStamp expired" warnings in PIE listen-server.
    CMC->NetworkMaxSmoothUpdateDistance = 256.0f;
    CMC->NetworkSimulatedSmoothLocationTime = 0.2f;
  }

  // Don't rotate character with controller pitch/roll
  bUseControllerRotationPitch = false;
  bUseControllerRotationYaw = false;
  bUseControllerRotationRoll = false;

  // --- Gameplay Components ---
  InventoryComponent = CreateDefaultSubobject<UFPMInventoryComponent>(
      TEXT("InventoryComponent"));

  InteractionComponent = CreateDefaultSubobject<UFPMInteractionComponent>(
      TEXT("InteractionComponent"));

  PlanetTraversalComponent = CreateDefaultSubobject<UFPMPlanetTraversal>(
      TEXT("PlanetTraversalComponent"));
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

    // --- Draw debug spheres at river head (spring) locations ---
    for (TActorIterator<AFPMWorldChunkManager> It(GetWorld()); It; ++It) {
      const TArray<FVector> &Heads = It->GetRiverHeads();
      for (const FVector &H : Heads) {
        // Only draw if within 50,000 cm (500m)
        if (FVector::Dist(GetActorLocation(), H) < 50000.0f) {
          DrawDebugSphere(GetWorld(), H, 150.0f, 12, FColor::Cyan, false, 0.0f,
                          0, 3.0f);
          DrawDebugSphere(GetWorld(), H + FVector(0, 0, 200.0f), 50.0f, 8,
                          FColor::Blue, false, 0.0f, 0, 2.0f);
        }
      }
      break;
    }
  }

  // --- Velocity clamp (prevent collision ejection launches) ---
  // Suppressed when Rift Runner is active (it uses SetActorLocation, not CMC
  // velocity, but residual CMC velocity could falsely trigger this).
  if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
    const UFPMPlanetTraversal *Rift =
        FindComponentByClass<UFPMPlanetTraversal>();
    const bool bRiftActive = Rift && Rift->IsRiftRunnerActive();
    const FVector Vel = CMC->Velocity;
    constexpr float MaxSafeSpeed = 60000.0f; // 600 m/s (turbo flight)
    if (!bRiftActive && Vel.Size() > MaxSafeSpeed) {
      CMC->StopMovementImmediately();
      UE_LOG(LogFPMPlayerCharacter, Warning,
             TEXT("FPM: Velocity clamp — killed extreme velocity (%.0f cm/s)"),
             Vel.Size());
    }
  }

  // --- Fall-through recovery ---
  // Runs on locally controlled character for immediate correction.
  // Uses TeleportTo (not SetActorLocation) so the CMC doesn't override it.
  // Suppressed when Rift Runner is active (it manages its own altitude).
  {
    const UFPMPlanetTraversal *Rift =
        FindComponentByClass<UFPMPlanetTraversal>();
    const bool bRiftActive = Rift && Rift->IsRiftRunnerActive();
    const FVector Loc = GetActorLocation();
    // Only trigger if below sea level minus a generous buffer.
    // Legitimate terrain can be at negative Z (ocean floor), but if the
    // character is falling with high negative velocity, something went wrong.
    constexpr float FallThroughThreshold = -50000.0f; // -500m

    if (!bRiftActive && Loc.Z < FallThroughThreshold) {
      // Cooldown: avoid repeated triggers each frame
      const double Now = FPlatformTime::Seconds();
      if (Now - LastFallRecoveryTime < 5.0) {
        return; // Already recovered recently, wait for terrain to load
      }
      LastFallRecoveryTime = Now;

      // Use the terrain generator to compute the actual surface Z at our XY.
      // This is deterministic and doesn't depend on collision cooking.
      int32 RecoverySeed = 42;
      UWorld *W = GetWorld();
      if (W) {
        for (TActorIterator<AFPMWorldChunkManager> It(W); It; ++It) {
          RecoverySeed = It->WorldSeed;
          break;
        }
      }

      const float SurfaceZ =
          FPMVoxelGenerator::TerrainSurfaceZ(Loc.X, Loc.Y, RecoverySeed);

      const float CapsuleHalf =
          GetCapsuleComponent()
              ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
              : 90.0f;

      // If terrain is above sea level here, use it. Otherwise search for
      // a safe Meadows location using the same search as spawn.
      FVector SafePos = FVector::ZeroVector;
      if (SurfaceZ > 0.0f) {
        SafePos = FVector(Loc.X, Loc.Y, SurfaceZ + CapsuleHalf + 50.0f);
      } else {
        // Terrain is underwater at our XY — find nearest safe land
        // Use a simple spiral search for above-sea-level terrain
        bool bFoundSafe = false;
        const float Step = 128000.0f; // 1.28km
        for (int32 Ring = 1; Ring <= 20 && !bFoundSafe; ++Ring) {
          const int32 Samples = Ring * 6;
          for (int32 Si = 0; Si < Samples && !bFoundSafe; ++Si) {
            const float Ang = (static_cast<float>(Si) / Samples) * 2.0f * PI;
            const float TX = Loc.X + Ring * Step * FMath::Cos(Ang);
            const float TY = Loc.Y + Ring * Step * FMath::Sin(Ang);
            const float TZ =
                FPMVoxelGenerator::TerrainSurfaceZ(TX, TY, RecoverySeed);
            if (TZ > 0.0f) {
              SafePos = FVector(TX, TY, TZ + CapsuleHalf + 50.0f);
              bFoundSafe = true;
            }
          }
        }
        if (!bFoundSafe) {
          // Last resort: use origin with computed terrain Z
          const float OriginZ =
              FPMVoxelGenerator::TerrainSurfaceZ(0.0f, 0.0f, RecoverySeed);
          SafePos = FVector(0.0f, 0.0f,
                            FMath::Max(OriginZ, 0.0f) + CapsuleHalf + 500.0f);
        }
      }

      // Force-load chunks at the recovery destination
      if (W) {
        for (TActorIterator<AFPMWorldChunkManager> It(W); It; ++It) {
          It->EnsureChunkLoadedAtWorldPos(SafePos);
          It->ForceChunkUpdate();
          break;
        }
      }

      // Stop all movement BEFORE teleporting
      if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
        CMC->StopMovementImmediately();
        CMC->SetMovementMode(MOVE_Flying);
        CMC->GravityScale = 0.0f;
      }

      // TeleportTo resets physics state so CMC won't override the position
      TeleportTo(SafePos, GetActorRotation());

      // Re-enable gravity after a short delay to let collision cook
      TWeakObjectPtr<AFPMPlayerCharacter> WeakSelf = this;
      if (W) {
        FTimerHandle TH;
        W->GetTimerManager().SetTimer(
            TH,
            [WeakSelf]() {
              if (AFPMPlayerCharacter *C = WeakSelf.Get()) {
                if (UCharacterMovementComponent *MC =
                        C->GetCharacterMovement()) {
                  MC->GravityScale = 1.0f;
                  MC->SetMovementMode(MOVE_Walking);
                }
              }
            },
            2.0f, false); // 2s delay for collision to be ready
      }

      UE_LOG(LogFPMPlayerCharacter, Warning,
             TEXT("FPM: Fall-through recovery -- teleported from Z=%.0f to "
                  "Z=%.0f at XY=(%.0f, %.0f) [SurfaceZ=%.0f]"),
             Loc.Z, SafePos.Z, SafePos.X, SafePos.Y, SurfaceZ);
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
  DOREPLIFETIME(AFPMPlayerCharacter, Species);
  DOREPLIFETIME(AFPMPlayerCharacter, SkinTone);
  DOREPLIFETIME(AFPMPlayerCharacter, EyeColor);
  DOREPLIFETIME(AFPMPlayerCharacter, HairColor);
  DOREPLIFETIME(AFPMPlayerCharacter, MorphJaw);
  DOREPLIFETIME(AFPMPlayerCharacter, MorphNose);
  DOREPLIFETIME(AFPMPlayerCharacter, MorphBrow);
  DOREPLIFETIME(AFPMPlayerCharacter, MorphLips);
}

// -------------------------------------------------------------------
// Server-Side Initialization
// -------------------------------------------------------------------

void AFPMPlayerCharacter::InitializeAppearance(
    const FString &InName, uint8 InSpecies, uint8 InBodyType,
    const FLinearColor &InSkinTone, const FLinearColor &InEyeColor,
    const FLinearColor &InHairColor, float InMorphJaw, float InMorphNose,
    float InMorphBrow, float InMorphLips) {
  CharacterName = InName;
  Species = InSpecies;
  BodyType = InBodyType;
  SkinTone = InSkinTone;
  EyeColor = InEyeColor;
  HairColor = InHairColor;
  MorphJaw = InMorphJaw;
  MorphNose = InMorphNose;
  MorphBrow = InMorphBrow;
  MorphLips = InMorphLips;

  // Apply immediately on the server; clients get it via OnRep
  ApplyAppearance();

  UE_LOG(LogFPMPlayerCharacter, Log,
         TEXT("FPM: Appearance initialized — Name='%s', Species=%d, Body=%d, "
              "Skin=(%.2f,%.2f,%.2f), Eye=(%.2f,%.2f,%.2f), "
              "Hair=(%.2f,%.2f,%.2f), Morphs=(%.2f,%.2f,%.2f,%.2f)"),
         *CharacterName, Species, BodyType, SkinTone.R, SkinTone.G, SkinTone.B,
         EyeColor.R, EyeColor.G, EyeColor.B, HairColor.R, HairColor.G,
         HairColor.B, MorphJaw, MorphNose, MorphBrow, MorphLips);
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

void AFPMPlayerCharacter::OnRep_Species() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_SkinTone() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_EyeColor() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_HairColor() { ApplyAppearance(); }

void AFPMPlayerCharacter::OnRep_FacialMorphs() { ApplyAppearance(); }

// -------------------------------------------------------------------
// Appearance Application
// -------------------------------------------------------------------

void AFPMPlayerCharacter::ApplyAppearance() {
  USkeletalMeshComponent *SkelMesh = GetMesh();
  if (!SkelMesh)
    return;

  // --- Switch mesh based on BodyType (0=Male, 1=Female) ---
  USkeletalMesh *DesiredMesh = (BodyType == 1 && CC5FemaleMesh)
                                   ? CC5FemaleMesh.Get()
                               : CC5MaleMesh ? CC5MaleMesh.Get()
                                             : nullptr;

  if (DesiredMesh && SkelMesh->GetSkeletalMeshAsset() != DesiredMesh) {
    SkelMesh->SetSkeletalMeshAsset(DesiredMesh);
    SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: Switched to %s mesh."),
           BodyType == 1 ? TEXT("Female") : TEXT("Male"));
  }

  // --- Play idle animation ---
  if (CC5IdleAnimation &&
      SkelMesh->GetAnimationMode() == EAnimationMode::AnimationSingleNode) {
    SkelMesh->PlayAnimation(CC5IdleAnimation, true);
  }

  // --- Apply facial morph targets ---
  if (SkelMesh->GetSkeletalMeshAsset()) {
    SkelMesh->SetMorphTarget(MorphName_Jaw, MorphJaw);
    SkelMesh->SetMorphTarget(MorphName_Nose, MorphNose);
    SkelMesh->SetMorphTarget(MorphName_Brow, MorphBrow);
    SkelMesh->SetMorphTarget(MorphName_Lips, MorphLips);
  }

  UE_LOG(LogFPMPlayerCharacter, Verbose,
         TEXT("FPM: ApplyAppearance — Species=%d, Body=%d, "
              "Skin=(%.2f,%.2f,%.2f), Eye=(%.2f,%.2f,%.2f), "
              "Hair=(%.2f,%.2f,%.2f), Morphs=(%.2f,%.2f,%.2f,%.2f)"),
         Species, BodyType, SkinTone.R, SkinTone.G, SkinTone.B, EyeColor.R,
         EyeColor.G, EyeColor.B, HairColor.R, HairColor.G, HairColor.B,
         MorphJaw, MorphNose, MorphBrow, MorphLips);

  // --- Apply species-specific scaling ---
  ApplySpeciesScaling();
}

void AFPMPlayerCharacter::ApplySpeciesScaling() {
  // ----------------------------------------------------------------
  // Resolve species values — prefer UFPMSpeciesRegistry if assigned,
  // fall back to the hardcoded FallbackData table.
  // ----------------------------------------------------------------
  float MeshScale = 0.50f;
  float CapsHH = 45.0f;
  float CapsR = 17.0f;
  float WalkSpeed = 150.0f; // cm/s
  float RunSpeed = 500.0f;  // cm/s
  float Boom = 200.0f;
  float JumpMult = 1.0f;
  TMap<FName, float> MorphDefaults; // species default morph overrides

  UFPMSpeciesDataAsset *DA = nullptr;
  if (SpeciesRegistry) {
    DA = SpeciesRegistry->FindSpecies(static_cast<EFPMSpecies>(Species));
  }

  if (DA) {
    // --- Data Asset path (designer-controlled) ---
    MeshScale = DA->MeshScale;
    CapsHH = DA->CapsuleHalfHeight;
    CapsR = DA->CapsuleRadius;
    WalkSpeed = DA->BaseWalkSpeed;
    RunSpeed = DA->BaseRunSpeed;
    Boom = DA->CameraBoomLength;
    JumpMult = DA->JumpMultiplier;
    MorphDefaults = DA->DefaultMorphTargets;

    UE_LOG(
        LogFPMPlayerCharacter, Log,
        TEXT("FPM: Species scaling from DA '%s' — Scale=%.2f, "
             "Capsule=(%.0f/%.0f), Walk=%.0f, Run=%.0f, Boom=%.0f, Jump=%.2fx"),
        *DA->GetName(), MeshScale, CapsHH, CapsR, WalkSpeed, RunSpeed, Boom,
        JumpMult);
  } else {
    // --- Fallback table (no registry assigned) ---
    const int32 Idx =
        FMath::Clamp(static_cast<int32>(Species), 0, FallbackDataCount - 1);
    const FFallbackSpeciesData &F = FallbackData[Idx];
    MeshScale = F.MeshScale;
    CapsHH = F.CapsuleHalfHeight;
    CapsR = F.CapsuleRadius;
    WalkSpeed = F.BaseWalkSpeed;
    RunSpeed = F.BaseRunSpeed;
    Boom = F.BoomLength;
    JumpMult = F.JumpMultiplier;

    UE_LOG(LogFPMPlayerCharacter, Log,
           TEXT("FPM: Species scaling from fallback table (no registry) — "
                "Idx=%d, Scale=%.2f, Capsule=(%.0f/%.0f), "
                "Walk=%.0f, Run=%.0f, Boom=%.0f, Jump=%.2fx"),
           Idx, MeshScale, CapsHH, CapsR, WalkSpeed, RunSpeed, Boom, JumpMult);
  }

  // ----------------------------------------------------------------
  // Apply to components
  // ----------------------------------------------------------------

  // Capsule dimensions
  if (UCapsuleComponent *Cap = GetCapsuleComponent()) {
    Cap->SetCapsuleHalfHeight(CapsHH);
    Cap->SetCapsuleRadius(CapsR);
  }

  // Mesh scale + Z offset: feet must align with capsule bottom
  if (USkeletalMeshComponent *SM = GetMesh()) {
    SM->SetRelativeScale3D(FVector(MeshScale));
    SM->SetRelativeLocation(FVector(0.0f, 0.0f, -CapsHH));
  }

  // Cache walk/run speeds — HandleMovementInput reads these every frame
  CachedWalkSpeed = WalkSpeed;
  CachedRunSpeed = RunSpeed;

  // Set CMC to current run state (could be called mid-game if species changes)
  if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
    CMC->MaxWalkSpeed = bIsRunning ? CachedRunSpeed : CachedWalkSpeed;
    // Base 300 cm/s * JumpMult. h = v²/(2g): 300→46cm, 345→61cm, 270→37cm
    CMC->JumpZVelocity = 300.0f * JumpMult;
  }

  // Camera boom
  if (CameraBoom)
    CameraBoom->TargetArmLength = Boom;

  // Species default morph targets (applied on top of player customization)
  if (!MorphDefaults.IsEmpty()) {
    if (USkeletalMeshComponent *SM = GetMesh()) {
      for (const auto &Pair : MorphDefaults) {
        SM->SetMorphTarget(Pair.Key, Pair.Value);
      }
      UE_LOG(LogFPMPlayerCharacter, Log,
             TEXT("FPM: Applied %d species default morph targets."),
             MorphDefaults.Num());
    }
  }
}

// -------------------------------------------------------------------
// Flight Toggle
// -------------------------------------------------------------------

void AFPMPlayerCharacter::ToggleFlight() {
  UCharacterMovementComponent *CMC = GetCharacterMovement();
  if (!CMC)
    return;

  bIsFlying = !bIsFlying;

  if (bIsFlying) {
    CMC->SetMovementMode(MOVE_Flying);
    CMC->MaxFlySpeed = 2000.0f;
    CMC->MaxAcceleration = 20000.0f; // Instant acceleration
    CMC->BrakingDecelerationFlying = 8000.0f;
    CMC->GravityScale = 0.0f;
    UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: Flight mode ENABLED"));
  } else {
    CMC->SetMovementMode(MOVE_Falling);
    CMC->MaxAcceleration = 2048.0f;
    CMC->GravityScale = 1.0f;
    // Restore correct walk/run speed for current run state
    CMC->MaxWalkSpeed = bIsRunning ? CachedRunSpeed : CachedWalkSpeed;
    UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: Flight mode DISABLED"));
  }
}

// -------------------------------------------------------------------
// Interaction (E key)
// -------------------------------------------------------------------

void AFPMPlayerCharacter::TryInteract() {
  if (!InteractionComponent)
    return;
  InteractionComponent->TryInteract();
}

// -------------------------------------------------------------------
// Inventory Toggle (I key)
// -------------------------------------------------------------------

void AFPMPlayerCharacter::ToggleInventory() {
  bInventoryOpen = !bInventoryOpen;

  if (bInventoryOpen) {
    UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: Inventory OPENED"));
    // TODO: Show inventory widget
    if (InventoryComponent) {
      const auto &Slots = InventoryComponent->GetSlots();
      int32 UsedSlots = 0;
      for (const auto &Slot : Slots) {
        if (!Slot.IsEmpty())
          UsedSlots++;
      }
      UE_LOG(LogFPMPlayerCharacter, Log,
             TEXT("FPM: Inventory has %d/%d slots in use"), UsedSlots,
             Slots.Num());
    }
  } else {
    UE_LOG(LogFPMPlayerCharacter, Log, TEXT("FPM: Inventory CLOSED"));
    // TODO: Hide inventory widget
  }
}

// -------------------------------------------------------------------
// Input Handling
// -------------------------------------------------------------------

void AFPMPlayerCharacter::HandleMovementInput(float DeltaTime) {
  APlayerController *PC = Cast<APlayerController>(GetController());
  if (!PC) {
    return;
  }

  // --- Middle Mouse Button: toggle first-person / third-person camera ---
  {
    static bool bMMBWasDown = false;
    const bool bMMBIsDown = PC->IsInputKeyDown(EKeys::MiddleMouseButton);
    if (bMMBIsDown && !bMMBWasDown) {
      if (CameraBoom) {
        const bool bIsFirstPerson = CameraBoom->TargetArmLength < 10.f;
        CameraBoom->TargetArmLength = bIsFirstPerson ? 400.f : 0.f;
        CameraBoom->bEnableCameraLag = !bIsFirstPerson;
      }
    }
    bMMBWasDown = bMMBIsDown;
  }

  // --- HUD cursor mode (Tab key with debounce) ---
  // Tab toggles between:
  //   Game-only mode  — mouse locked, camera control active
  //   Game+UI mode    — cursor visible, can click HUD buttons (biome teleport
  //   etc)
  {
    static bool bTabWasDown = false;
    const bool bTabIsDown = PC->IsInputKeyDown(EKeys::Tab);
    if (bTabIsDown && !bTabWasDown) {
      bHUDMouseMode = !bHUDMouseMode;
      PC->SetShowMouseCursor(bHUDMouseMode);
      if (bHUDMouseMode) {
        // Show cursor — still allow WASD/movement but mouse clicks hit the HUD
        FInputModeGameAndUI UIMode;
        UIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        UIMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(UIMode);
      } else {
        // Lock mouse back for camera
        PC->SetInputMode(FInputModeGameOnly());
      }
    }
    bTabWasDown = bTabIsDown;
  }

  // --- Run / flight-boost (Left Shift) ---
  // Sync bIsFlying from PlanetTraversal so pitch-movement logic stays correct.
  {
    if (const UFPMPlanetTraversal *Rift = FindComponentByClass<UFPMPlanetTraversal>())
      bIsFlying = Rift->IsFlying();

    const bool bShiftHeld = PC->IsInputKeyDown(EKeys::LeftShift);
    if (bIsFlying) {
      // PlanetTraversal owns MaxFlySpeed -- don't override it here
    } else {
      // Ground run: toggle CMC walk speed
      if (bShiftHeld != bIsRunning) {
        bIsRunning = bShiftHeld;
        if (UCharacterMovementComponent *CMC = GetCharacterMovement()) {
          CMC->MaxWalkSpeed = bIsRunning ? CachedRunSpeed : CachedWalkSpeed;
        }
        UE_LOG(LogFPMPlayerCharacter, Verbose, TEXT("FPM: %s (%.0f cm/s)"),
               bIsRunning ? TEXT("Running") : TEXT("Walking"),
               bIsRunning ? CachedRunSpeed : CachedWalkSpeed);
      }
    }
  }

  // --- Interact (E key with debounce) ---
  {
    static bool bEKeyWasDown = false;
    const bool bEKeyIsDown = PC->IsInputKeyDown(EKeys::E);
    if (bEKeyIsDown && !bEKeyWasDown) {
      TryInteract();
    }
    bEKeyWasDown = bEKeyIsDown;
  }

  // --- Inventory toggle (I key with debounce) ---
  {
    static bool bIKeyWasDown = false;
    const bool bIKeyIsDown = PC->IsInputKeyDown(EKeys::I);
    if (bIKeyIsDown && !bIKeyWasDown) {
      ToggleInventory();
    }
    bIKeyWasDown = bIKeyIsDown;
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

    if (bIsFlying) {
      // In flight: use full camera rotation (including pitch)
      // so WASD moves in the direction you're looking
      const FVector ForwardDir =
          FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
      const FVector RightDir =
          FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);

      // Turbo boost: hold Shift for 5x speed
      const float SpeedMult =
          PC->IsInputKeyDown(EKeys::LeftShift) ? 5.0f : 1.0f;
      AddMovementInput(ForwardDir, MoveInput.X * SpeedMult);
      AddMovementInput(RightDir, MoveInput.Y * SpeedMult);
    } else {
      // On ground: yaw only, no pitch
      const FRotator YawOnlyRot(0.0f, ControlRot.Yaw, 0.0f);
      const FVector ForwardDir =
          FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::X);
      const FVector RightDir =
          FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::Y);

      AddMovementInput(ForwardDir, MoveInput.X);
      AddMovementInput(RightDir, MoveInput.Y);
    }
  }

  // --- Space Bar: Jump on ground, fly up in the air ---
  if (bIsFlying) {
    const float VertMult = PC->IsInputKeyDown(EKeys::LeftShift) ? 5.0f : 1.0f;
    if (PC->IsInputKeyDown(EKeys::SpaceBar))
      AddMovementInput(FVector::UpVector, 1.0f * VertMult);
    if (PC->IsInputKeyDown(EKeys::C))
      AddMovementInput(FVector::UpVector, -1.0f * VertMult);
  } else {
    // ACharacter::Jump() checks IsGrounded internally — safe to call every
    // frame. StopJumping() clears the jump flag so the character doesn't
    // chain-jump.
    if (PC->IsInputKeyDown(EKeys::SpaceBar))
      Jump();
    else
      StopJumping();
  }

  // Mouse look — suppressed while HUD cursor is visible
  if (!bHUDMouseMode) {
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    PC->GetInputMouseDelta(MouseX, MouseY);
    AddControllerYawInput(MouseX);
    AddControllerPitchInput(-MouseY);
  }
}
