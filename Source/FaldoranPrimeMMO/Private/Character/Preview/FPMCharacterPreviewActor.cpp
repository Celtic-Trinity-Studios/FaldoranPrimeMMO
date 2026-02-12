// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/Preview/FPMCharacterPreviewActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterPreview, Log, All);

// Material parameter names — must match the MID parameter names
const FName AFPMCharacterPreviewActor::SkinToneParamName(TEXT("SkinTone"));
const FName AFPMCharacterPreviewActor::HairColorParamName(TEXT("HairColor"));

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

AFPMCharacterPreviewActor::AFPMCharacterPreviewActor() {
  PrimaryActorTick.bCanEverTick = false;

  // Client-only — never replicate
  bReplicates = false;

  // Root component for rotation pivot
  PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
  SetRootComponent(PreviewRoot);

  // Skeletal mesh — UE5 mannequin placeholder
  PreviewMesh =
      CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
  PreviewMesh->SetupAttachment(PreviewRoot);
  PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));

  // UE 5.7.1 Third Person content pack uses SKM_Manny_Simple
  static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(
      TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
  if (MannyMesh.Succeeded()) {
    PreviewMesh->SetSkeletalMeshAsset(MannyMesh.Object);
  } else {
    UE_LOG(
        LogFPMCharacterPreview, Warning,
        TEXT("FPM: Failed to load SKM_Manny_Simple. Preview will be empty."));
  }

  // --- Lighting ---
  // High intensity required because point lights compete with no ambient.
  // Preview is isolated far from the world, so these only affect our mesh.

  // Key light: front-right, warm, strong primary illumination
  KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
  KeyLight->SetupAttachment(PreviewRoot);
  KeyLight->SetRelativeLocation(FVector(120.0f, 80.0f, 100.0f));
  KeyLight->SetIntensity(80000.0f);
  KeyLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f));
  KeyLight->SetAttenuationRadius(500.0f);
  KeyLight->SetCastShadows(false);

  // Fill light: front-left, cool, softer shadow fill
  FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
  FillLight->SetupAttachment(PreviewRoot);
  FillLight->SetRelativeLocation(FVector(100.0f, -100.0f, 60.0f));
  FillLight->SetIntensity(40000.0f);
  FillLight->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));
  FillLight->SetAttenuationRadius(500.0f);
  FillLight->SetCastShadows(false);

  // Rim light: behind, for edge definition / silhouette pop
  RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RimLight"));
  RimLight->SetupAttachment(PreviewRoot);
  RimLight->SetRelativeLocation(FVector(-120.0f, 0.0f, 80.0f));
  RimLight->SetIntensity(50000.0f);
  RimLight->SetLightColor(FLinearColor(0.9f, 0.95f, 1.0f));
  RimLight->SetAttenuationRadius(500.0f);
  RimLight->SetCastShadows(false);

  // --- Camera / Scene Capture ---

  CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
  CameraBoom->SetupAttachment(PreviewRoot);
  CameraBoom->TargetArmLength = DefaultZoomDistance;
  // Position at chest height, aim slightly downward to frame the character
  CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
  CameraBoom->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
  CameraBoom->bDoCollisionTest = false;
  CameraBoom->bUsePawnControlRotation = false;

  SceneCapture =
      CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
  SceneCapture->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
  SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

  // Disable sky, atmosphere, and fog so we get a clean dark background.
  // The mannequin is lit only by our three point lights.
  SceneCapture->ShowFlags.SetAtmosphere(false);
  SceneCapture->ShowFlags.SetFog(false);
  SceneCapture->ShowFlags.SetVolumetricFog(false);
  SceneCapture->ShowFlags.SetSkyLighting(false);
  SceneCapture->ShowFlags.SetReflectionEnvironment(false);
}

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::BeginPlay() {
  Super::BeginPlay();

  // Create a render target for the scene capture
  UTextureRenderTarget2D *RT = NewObject<UTextureRenderTarget2D>(this);
  if (RT) {
    static constexpr int32 RenderTargetWidth = 512;
    static constexpr int32 RenderTargetHeight = 768;
    RT->InitAutoFormat(RenderTargetWidth, RenderTargetHeight);
    RT->ClearColor = FLinearColor(0.05f, 0.05f, 0.08f, 1.0f);
    SceneCapture->TextureTarget = RT;
  }

  CreateDynamicMaterial();

  UE_LOG(LogFPMCharacterPreview, Log,
         TEXT("FPM: CharacterPreviewActor spawned."));
}

// -------------------------------------------------------------------
// Appearance Setters
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetSkinTone(const FLinearColor &NewColor) {
  if (DynamicMaterial) {
    DynamicMaterial->SetVectorParameterValue(SkinToneParamName, NewColor);
  }
  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview SkinTone=(%.2f,%.2f,%.2f)"), NewColor.R, NewColor.G,
         NewColor.B);
}

void AFPMCharacterPreviewActor::SetHairStyle(uint8 StyleIndex) {
  // PROTOTYPE: Log the change. Full mesh-swap pipeline deferred until
  // Mutable/CC5 integration. The mannequin doesn't have hair meshes.
  CurrentHairStyle = StyleIndex;
  UE_LOG(LogFPMCharacterPreview, Verbose, TEXT("FPM: Preview HairStyle=%d"),
         StyleIndex);
}

void AFPMCharacterPreviewActor::SetHairColor(const FLinearColor &NewColor) {
  if (DynamicMaterial) {
    DynamicMaterial->SetVectorParameterValue(HairColorParamName, NewColor);
  }
  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview HairColor=(%.2f,%.2f,%.2f)"), NewColor.R,
         NewColor.G, NewColor.B);
}

void AFPMCharacterPreviewActor::SetBodyType(uint8 TypeIndex) {
  // PROTOTYPE: Log the change. Full body-type morph-target pipeline
  // deferred until Mutable/CC5 integration.
  CurrentBodyType = TypeIndex;
  UE_LOG(LogFPMCharacterPreview, Verbose, TEXT("FPM: Preview BodyType=%d"),
         TypeIndex);
}

// -------------------------------------------------------------------
// Camera Orbit Controls
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::AddYawRotation(float DeltaYaw) {
  if (CameraBoom) {
    FRotator Current = CameraBoom->GetRelativeRotation();
    Current.Yaw += DeltaYaw;
    CameraBoom->SetRelativeRotation(Current);
    UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: CameraBoom Yaw = %.2f"),
           Current.Yaw);
  }
}

void AFPMCharacterPreviewActor::AddZoom(float DeltaZoom) {
  if (!CameraBoom) {
    return;
  }
  float NewLength = CameraBoom->TargetArmLength + (DeltaZoom * ZoomSpeed);
  CameraBoom->TargetArmLength =
      FMath::Clamp(NewLength, MinZoomDistance, MaxZoomDistance);
}

// -------------------------------------------------------------------
// Render Target Access
// -------------------------------------------------------------------

UTextureRenderTarget2D *AFPMCharacterPreviewActor::GetRenderTarget() const {
  if (SceneCapture) {
    return SceneCapture->TextureTarget;
  }
  return nullptr;
}

// -------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::CreateDynamicMaterial() {
  if (!PreviewMesh) {
    return;
  }

  // Create a dynamic material instance from the mesh's first material slot
  UMaterialInterface *BaseMat = PreviewMesh->GetMaterial(0);
  if (BaseMat) {
    DynamicMaterial =
        UMaterialInstanceDynamic::Create(BaseMat, this, TEXT("PreviewMID"));
    if (DynamicMaterial) {
      PreviewMesh->SetMaterial(0, DynamicMaterial);
      UE_LOG(LogFPMCharacterPreview, Log,
             TEXT("FPM: Preview dynamic material created."));
    }
  } else {
    UE_LOG(LogFPMCharacterPreview, Warning,
           TEXT("FPM: No base material on preview mesh slot 0."));
  }
}
