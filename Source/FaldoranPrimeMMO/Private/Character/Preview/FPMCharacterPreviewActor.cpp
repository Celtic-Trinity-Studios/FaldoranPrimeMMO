// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "Character/Preview/FPMCharacterPreviewActor.h"
#include "Animation/AnimationAsset.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMCharacterPreview, Log, All);

// Material parameter names — must match the CC5 HQ shader parameter names
const FName AFPMCharacterPreviewActor::SkinColorParamName(TEXT("SkinColor"));
const FName AFPMCharacterPreviewActor::IrisColorParamName(TEXT("IrisColor"));
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

  // --- CC5 Skeletal Mesh ---
  PreviewMesh =
      CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
  PreviewMesh->SetupAttachment(PreviewRoot);
  // Offset down so the character's feet are near the pivot origin
  PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));

  // Force animation to always tick even when off-screen.
  // The preview actor is spawned far off-world (-50000,-50000,-5000) where
  // the player camera can't see it. UE5's visibility-based anim tick
  // optimization would suppress animation since SceneCaptureComponent2D
  // does NOT count as a "camera" for visibility checks.
  PreviewMesh->VisibilityBasedAnimTickOption =
      EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

  // --- Load both male and female CC5 meshes ---
  static ConstructorHelpers::FObjectFinder<USkeletalMesh> CC5MaleMeshObj(
      TEXT("SkeletalMesh'/Game/Characters/CC5/CC5_Base_Male.CC5_Base_Male'"));
  if (CC5MaleMeshObj.Succeeded()) {
    MaleMesh = CC5MaleMeshObj.Object;
    UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: CC5_Base_Male loaded."));
  }

  static ConstructorHelpers::FObjectFinder<USkeletalMesh> CC5FemaleMeshObj(TEXT(
      "SkeletalMesh'/Game/Characters/CC5/CC5_Base_Female.CC5_Base_Female'"));
  if (CC5FemaleMeshObj.Succeeded()) {
    FemaleMesh = CC5FemaleMeshObj.Object;
    UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: CC5_Base_Female loaded."));
  }

  // Default to male mesh
  if (MaleMesh) {
    PreviewMesh->SetSkeletalMeshAsset(MaleMesh);
  } else {
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> FallbackMesh(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    if (FallbackMesh.Succeeded())
      PreviewMesh->SetSkeletalMeshAsset(FallbackMesh.Object);
    UE_LOG(LogFPMCharacterPreview, Warning,
           TEXT("FPM: CC5_Base_Male not found, using fallback."));
  }

  // Load idle animation for direct looping playback
  static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleAnimObj(
      TEXT("/Game/Characters/CC5/Motion/idle-378963_idle-378963"));
  if (IdleAnimObj.Succeeded()) {
    IdleAnimation = IdleAnimObj.Object;
    UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: Idle animation loaded."));
  }

  // Use single-animation mode for seamless loop control
  PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

  // --- Hair Mesh Component ---
  HairMeshComp =
      CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
  HairMeshComp->SetupAttachment(PreviewMesh);
  // Hair attaches to head bone; relative transform handled by the mesh

  // --- Three-Point Lighting ---
  // High intensity because the preview scene is isolated with no global
  // lighting. These point lights only affect our mesh.

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

  // Ambient light: soft global fill to prevent pitch-black shadows
  AmbientLight =
      CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientLight"));
  AmbientLight->SetupAttachment(PreviewRoot);
  AmbientLight->SetIntensity(0.8f);
  AmbientLight->SetLightColor(FLinearColor(0.15f, 0.15f, 0.2f));

  // --- Camera / Scene Capture ---

  CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
  CameraBoom->SetupAttachment(PreviewRoot);
  CameraBoom->TargetArmLength = DefaultZoomDistance;
  // Position at chest height, aim slightly downward to frame the character
  CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
  CameraBoom->SetRelativeRotation(FRotator(DefaultCameraPitch, 0.0f, 0.0f));
  CameraBoom->bDoCollisionTest = false;
  CameraBoom->bUsePawnControlRotation = false;

  SceneCapture =
      CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
  SceneCapture->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
  SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

  // Dark background: disable sky/atmosphere so only our lights are visible
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
    static constexpr int32 RenderTargetWidth = 768;
    static constexpr int32 RenderTargetHeight = 1024;
    RT->InitAutoFormat(RenderTargetWidth, RenderTargetHeight);
    RT->ClearColor = FLinearColor(0.05f, 0.05f, 0.08f, 1.0f);
    SceneCapture->TextureTarget = RT;
  }

  CreateDynamicMaterials();

  // Start the idle animation with looping enabled
  if (IdleAnimation && PreviewMesh) {
    PreviewMesh->PlayAnimation(IdleAnimation, true);
  }

  UE_LOG(LogFPMCharacterPreview, Log,
         TEXT("FPM: CharacterPreviewActor spawned with CC5 mesh."));
}

// -------------------------------------------------------------------
// Morph Target Control
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetMorphValue(FName MorphName, float Value) {
  if (!PreviewMesh) {
    return;
  }

  const float ClampedValue = FMath::Clamp(Value, 0.0f, 1.0f);
  PreviewMesh->SetMorphTarget(MorphName, ClampedValue);

  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview Morph '%s' = %.3f"), *MorphName.ToString(),
         ClampedValue);
}

// -------------------------------------------------------------------
// Material Color Setters
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetSkinTone(const FLinearColor &NewColor) {
  if (SkinMID) {
    SkinMID->SetVectorParameterValue(SkinColorParamName, NewColor);
  }
  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview SkinTone=(%.2f,%.2f,%.2f)"), NewColor.R, NewColor.G,
         NewColor.B);
}

void AFPMCharacterPreviewActor::SetEyeColor(const FLinearColor &NewColor) {
  if (EyeMID) {
    EyeMID->SetVectorParameterValue(IrisColorParamName, NewColor);
  }
  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview EyeColor=(%.2f,%.2f,%.2f)"), NewColor.R, NewColor.G,
         NewColor.B);
}

void AFPMCharacterPreviewActor::SetHairColor(const FLinearColor &NewColor) {
  if (HairMID) {
    HairMID->SetVectorParameterValue(HairColorParamName, NewColor);
  }
  UE_LOG(LogFPMCharacterPreview, Verbose,
         TEXT("FPM: Preview HairColor=(%.2f,%.2f,%.2f)"), NewColor.R,
         NewColor.G, NewColor.B);
}

// -------------------------------------------------------------------
// Hair Style
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetHairStyle(int32 HairIndex) {
  if (!HairMeshComp) {
    return;
  }

  CurrentHairIndex = HairIndex;

  // Index 0 = bald (clear the mesh)
  if (HairIndex <= 0 || HairMeshes.Num() == 0) {
    HairMeshComp->SetSkeletalMesh(nullptr);
    UE_LOG(LogFPMCharacterPreview, Verbose,
           TEXT("FPM: Preview HairStyle = Bald"));
    return;
  }

  // Valid index check (offset by 1 since index 0 is bald)
  const int32 MeshIndex = HairIndex - 1;
  if (MeshIndex < HairMeshes.Num() && HairMeshes[MeshIndex]) {
    HairMeshComp->SetSkeletalMesh(HairMeshes[MeshIndex]);
    UE_LOG(LogFPMCharacterPreview, Verbose, TEXT("FPM: Preview HairStyle=%d"),
           HairIndex);
  } else {
    UE_LOG(LogFPMCharacterPreview, Warning,
           TEXT("FPM: HairStyle index %d out of range (max %d)."), HairIndex,
           HairMeshes.Num());
  }
}

// -------------------------------------------------------------------
// Camera Orbit Controls
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::AddYawRotation(float DeltaYaw) {
  if (!CameraBoom) {
    return;
  }
  FRotator Current = CameraBoom->GetRelativeRotation();
  Current.Yaw += DeltaYaw;
  CameraBoom->SetRelativeRotation(Current);
}

void AFPMCharacterPreviewActor::AddZoom(float DeltaZoom) {
  if (!CameraBoom) {
    return;
  }
  float NewLength = CameraBoom->TargetArmLength + (DeltaZoom * ZoomSpeed);
  CameraBoom->TargetArmLength =
      FMath::Clamp(NewLength, MinZoomDistance, MaxZoomDistance);
}

void AFPMCharacterPreviewActor::ResetCameraToFront() {
  if (!CameraBoom) {
    return;
  }
  CameraBoom->SetRelativeRotation(FRotator(DefaultCameraPitch, 0.0f, 0.0f));
  CameraBoom->TargetArmLength = DefaultZoomDistance;
  UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: Camera reset to front view."));
}

// -------------------------------------------------------------------
// Gender Swap
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetIsFemale(bool bFemale) {
  if (bIsFemale == bFemale)
    return;
  bIsFemale = bFemale;
  ApplyCurrentBody();
}

void AFPMCharacterPreviewActor::ApplyCurrentBody() {
  if (!PreviewMesh)
    return;

  USkeletalMesh *NewMesh = bIsFemale ? FemaleMesh.Get() : MaleMesh.Get();
  if (!NewMesh) {
    UE_LOG(LogFPMCharacterPreview, Warning, TEXT("FPM: %s mesh not available."),
           bIsFemale ? TEXT("Female") : TEXT("Male"));
    return;
  }

  PreviewMesh->SetSkeletalMeshAsset(NewMesh);

  // Direct playback with looping for seamless idle animation
  PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
  if (IdleAnimation) {
    PreviewMesh->PlayAnimation(IdleAnimation, true);
  }
  CreateDynamicMaterials();

  UE_LOG(LogFPMCharacterPreview, Log, TEXT("FPM: Switched to %s body."),
         bIsFemale ? TEXT("Female") : TEXT("Male"));
}

// -------------------------------------------------------------------
// Species Scaling (preview mesh)
// -------------------------------------------------------------------

void AFPMCharacterPreviewActor::SetSpeciesScale(float UniformScale) {
  if (!PreviewMesh)
    return;

  PreviewMesh->SetRelativeScale3D(FVector(UniformScale));

  // Adjust camera target Y offset to keep the character framed.
  // Shorter races (scale < 1) should shift the camera down, taller up.
  if (CameraBoom) {
    const float BaseHeight = 10.0f;
    CameraBoom->SetRelativeLocation(
        FVector(0.0f, 0.0f, BaseHeight * UniformScale));
  }

  UE_LOG(LogFPMCharacterPreview, Log,
         TEXT("FPM: Preview species scale set to %.2f"), UniformScale);
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

void AFPMCharacterPreviewActor::CreateDynamicMaterials() {
  if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
    return;

  AllBodyMIDs.Empty();
  SkinMID = nullptr;
  EyeMID = nullptr;

  const int32 NumMats = PreviewMesh->GetNumMaterials();
  AllBodyMIDs.Reserve(NumMats);

  for (int32 i = 0; i < NumMats; ++i) {
    UMaterialInterface *BaseMat = PreviewMesh->GetMaterial(i);
    if (!BaseMat)
      continue;

    UMaterialInstanceDynamic *MID =
        UMaterialInstanceDynamic::Create(BaseMat, this);
    PreviewMesh->SetMaterial(i, MID);
    AllBodyMIDs.Add(MID);

    // Identify skin and eye slots by name
    const FName SlotName =
        PreviewMesh->GetSkeletalMeshAsset()->GetMaterials()[i].MaterialSlotName;
    const FString SlotStr = SlotName.ToString().ToLower();
    if (SlotStr.Contains(TEXT("skin")) || SlotStr.Contains(TEXT("body"))) {
      SkinMID = MID;
    } else if (SlotStr.Contains(TEXT("eye")) ||
               SlotStr.Contains(TEXT("iris"))) {
      EyeMID = MID;
    }
  }

  // Hair MID from hair mesh comp
  if (HairMeshComp && HairMeshComp->GetSkeletalMeshAsset()) {
    UMaterialInterface *HairMat = HairMeshComp->GetMaterial(0);
    if (HairMat) {
      HairMID = UMaterialInstanceDynamic::Create(HairMat, this);
      HairMeshComp->SetMaterial(0, HairMID);
    }
  }

  UE_LOG(LogFPMCharacterPreview, Log,
         TEXT("FPM: CreateDynamicMaterials — %d body MIDs created. "
              "Skin=%s, Eye=%s, Hair=%s"),
         AllBodyMIDs.Num(), SkinMID ? TEXT("found") : TEXT("none"),
         EyeMID ? TEXT("found") : TEXT("none"),
         HairMID ? TEXT("found") : TEXT("none"));
}

int32 AFPMCharacterPreviewActor::FindMaterialSlotByKeyword(
    const FString &Keyword) const {
  if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset()) {
    return INDEX_NONE;
  }

  const TArray<FSkeletalMaterial> &Materials =
      PreviewMesh->GetSkeletalMeshAsset()->GetMaterials();
  for (int32 Index = 0; Index < Materials.Num(); ++Index) {
    const FName SlotName = Materials[Index].MaterialSlotName;
    if (SlotName.ToString().Contains(Keyword)) {
      return Index;
    }
  }
  return INDEX_NONE;
}
