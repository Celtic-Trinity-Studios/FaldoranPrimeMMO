// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMWaterSource.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AFPMWaterSource::AFPMWaterSource() {
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickInterval = 0.0f; // Managed by water simulation
  bReplicates = false;

  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

  // --- Visible spring pool mesh ---
  PoolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoolMesh"));
  PoolMesh->SetupAttachment(RootComponent);

  // Use engine default cylinder
  static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
      TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  if (CylinderFinder.Succeeded()) {
    PoolMesh->SetStaticMesh(CylinderFinder.Object);
  }

  // Scale: 3m radius disc (X,Y = 3.0) barely raised (Z = 0.02)
  PoolMesh->SetWorldScale3D(FVector(3.0f, 3.0f, 0.02f));
  // Slight offset up so it sits on terrain surface
  PoolMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
  PoolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  PoolMesh->CastShadow = false;
}

void AFPMWaterSource::BeginPlay() {
  Super::BeginPlay();
  RemainingVolume = TotalVolume;

  // Apply translucent blue water material at runtime
  if (PoolMesh) {
    UMaterialInterface *BaseMat = PoolMesh->GetMaterial(0);
    if (BaseMat) {
      UMaterialInstanceDynamic *DynMat =
          UMaterialInstanceDynamic::Create(BaseMat, this);
      if (DynMat) {
        // Deep blue-green water color with transparency
        DynMat->SetVectorParameterValue(
            TEXT("BaseColor"), FLinearColor(0.05f, 0.25f, 0.45f, 1.0f));
        DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.7f);
        PoolMesh->SetMaterial(0, DynMat);
      }
    }
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM Water: Source spawned at %s — Type=%d, FlowRate=%.1f, "
              "Infinite=%d"),
         *GetActorLocation().ToString(), static_cast<int32>(SourceType),
         FlowRate, bInfiniteSource ? 1 : 0);
}

void AFPMWaterSource::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // Deplete finite sources
  if (bActive && !bInfiniteSource) {
    RemainingVolume -= FlowRate * DeltaTime;
    if (RemainingVolume <= 0.0f) {
      RemainingVolume = 0.0f;
      bActive = false;
      UE_LOG(LogTemp, Warning,
             TEXT("FPM Water: Source at %s depleted — deactivating"),
             *GetActorLocation().ToString());
    }
  }
}

float AFPMWaterSource::GetEffectiveFlowRate() const {
  if (!bActive)
    return 0.0f;
  if (bInfiniteSource)
    return FlowRate;
  return (RemainingVolume > 0.0f) ? FlowRate : 0.0f;
}

void AFPMWaterSource::SetActive(bool bNewActive) {
  bActive = bNewActive;
  UE_LOG(LogTemp, Log, TEXT("FPM Water: Source at %s %s"),
         *GetActorLocation().ToString(),
         bActive ? TEXT("activated") : TEXT("deactivated"));
}
