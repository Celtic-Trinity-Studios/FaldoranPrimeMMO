// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMTerrainShell.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "ProceduralMeshComponent.h"
#include "World/FPMChunkData.h"
#include "World/FPMNoise.h"
#include "World/FPMVoxelChunk.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------
static constexpr int32 NearGridN = 128;
static constexpr float NearHalfExtent = 5000000.f; // 50 km
static constexpr int32 FarGridN = 64;
static constexpr float FarHalfExtent = 40000000.f; // 400 km

// ---------------------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------------------
AFPMTerrainShell::AFPMTerrainShell() {
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.bStartWithTickEnabled = true;
  SetReplicates(false);

  NearMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NearMesh"));
  NearMesh->bUseAsyncCooking = false; // we don't need collision
  NearMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  NearMesh->SetCastShadow(false);
  NearMesh->bRenderInMainPass = true;
  SetRootComponent(NearMesh);

  FarMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FarMesh"));
  FarMesh->bUseAsyncCooking = false;
  FarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  FarMesh->SetCastShadow(false);
  FarMesh->bRenderInMainPass = true;
  FarMesh->SetupAttachment(NearMesh);

  // Load and apply the unlit vertex-color material (set up in Content/Materials/M_TerrainShell)
  static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShellMat(
      TEXT("/Game/Materials/M_TerrainShell.M_TerrainShell"));
  if (ShellMat.Succeeded()) {
    NearMesh->SetMaterial(0, ShellMat.Object);
    FarMesh->SetMaterial(0, ShellMat.Object);
  }
}

// ---------------------------------------------------------------------------
//  Initialize
// ---------------------------------------------------------------------------
void AFPMTerrainShell::Initialize(int32 InWorldSeed) {
  WorldSeed = InWorldSeed;
  TimeSinceRebake = 9999.f; // fire immediately first tick
}

// ---------------------------------------------------------------------------
//  Tick — re-centre on player and re-bake periodically
// ---------------------------------------------------------------------------
void AFPMTerrainShell::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // Find the local player pawn
  FVector PlayerPos = FVector::ZeroVector;
  if (UWorld *W = GetWorld()) {
    if (APlayerController *PC = W->GetFirstPlayerController()) {
      if (APawn *P = PC->GetPawn()) {
        PlayerPos = P->GetActorLocation();
      }
    }
  }

  // Re-centre the shell on the player each frame (camera-relative trick)
  SetActorLocation(PlayerPos);

  // Periodic re-bake
  TimeSinceRebake += DeltaTime;
  if (!bBakeInFlight && TimeSinceRebake >= RebakeIntervalSec) {
    TimeSinceRebake = 0.f;
    TriggerAsyncBake(PlayerPos);
  }
}

// ---------------------------------------------------------------------------
//  TriggerAsyncBake
// ---------------------------------------------------------------------------
void AFPMTerrainShell::TriggerAsyncBake(FVector CentreWorld) {
  bBakeInFlight = true;

  const int32 Seed = WorldSeed;
  TWeakObjectPtr<AFPMTerrainShell> WeakSelf = this;

  Async(EAsyncExecution::ThreadPool, [CentreWorld, Seed, WeakSelf]() {
    // Build both rings on a worker thread — pure math, no UObject access
    FShellMeshData Near =
        BuildRing(CentreWorld, NearGridN, NearHalfExtent, Seed);
    FShellMeshData Far = BuildRing(CentreWorld, FarGridN, FarHalfExtent, Seed);

    // Marshal back to game thread
    AsyncTask(ENamedThreads::GameThread, [WeakSelf, NearData = MoveTemp(Near),
                                          FarData = MoveTemp(Far)]() mutable {
      AFPMTerrainShell *Self = WeakSelf.Get();
      if (!Self)
        return;

      // Apply near ring (section 0)
      Self->NearMesh->CreateMeshSection(
          0, NearData.Vertices, NearData.Triangles, NearData.Normals,
          TArray<FVector2D>{}, // UVs
          NearData.Colors, TArray<FProcMeshTangent>{},
          /*bCreateCollision=*/false);

      // Apply far ring (section 0 on separate component)
      Self->FarMesh->CreateMeshSection(
          0, FarData.Vertices, FarData.Triangles, FarData.Normals,
          TArray<FVector2D>{}, FarData.Colors, TArray<FProcMeshTangent>{},
          /*bCreateCollision=*/false);

      Self->bBakeInFlight = false;
    });
  });
}

// ---------------------------------------------------------------------------
//  BuildRing — pure static, called from worker thread
// ---------------------------------------------------------------------------
AFPMTerrainShell::FShellMeshData
AFPMTerrainShell::BuildRing(FVector CentreWorld, int32 GridN,
                            float HalfExtentCm, int32 WorldSeed) {

  FShellMeshData Out;
  const int32 Verts = GridN + 1;
  Out.Vertices.Reserve(Verts * Verts);
  Out.Normals.Reserve(Verts * Verts);
  Out.Colors.Reserve(Verts * Verts);

  const float Step = (HalfExtentCm * 2.f) / static_cast<float>(GridN);

  // --- Sample heights ---
  TArray<float> Heights;
  Heights.Reserve(Verts * Verts);

  for (int32 Row = 0; Row <= GridN; ++Row) {
    for (int32 Col = 0; Col <= GridN; ++Col) {
      const float LocalX = -HalfExtentCm + Col * Step;
      const float LocalY = -HalfExtentCm + Row * Step;
      const float WorldX = CentreWorld.X + LocalX;
      const float WorldY = CentreWorld.Y + LocalY;

      // Wrap coordinates into planet bounds
      const float WX = FPMChunkConstants::WrapWorldCoord(WorldX);
      const float WY = FPMChunkConstants::WrapWorldCoord(WorldY);

      const float SurfZ = FPMNoise::TerrainSurfaceZ(WX, WY, WorldSeed);
      Heights.Add(SurfZ);

      // Vertex position is relative to CentreWorld (avoids float precision)
      Out.Vertices.Add(FVector(LocalX, LocalY, SurfZ - CentreWorld.Z));

      // Biome color
      const float NormH = FMath::Clamp(SurfZ / 2000000.f, 0.f, 1.f);
      const EFPMBiome Biome =
          FPMVoxelGenerator::BiomeAtWorldXY(WX, WY, WorldSeed, NormH);
      FColor C = FPMVoxelGenerator::BiomeToVertexColor(Biome);

      // Below sea level → deep blue tint
      if (SurfZ <= 0.f) {
        const float Depth = FMath::Clamp(-SurfZ / 1100000.f, 0.f, 1.f);
        C = FColor(FMath::Lerp(C.R, 20, Depth), FMath::Lerp(C.G, 60, Depth),
                   FMath::Lerp(C.B, 140, Depth), 255);
      }
      // Altitude brightening for peaks
      else if (SurfZ > 500000.f) {
        const float Peak =
            FMath::Clamp((SurfZ - 500000.f) / 800000.f, 0.f, 1.f);
        C = FColor(FMath::Clamp(FMath::Lerp((int32)C.R, 240, Peak), 0, 255),
                   FMath::Clamp(FMath::Lerp((int32)C.G, 240, Peak), 0, 255),
                   FMath::Clamp(FMath::Lerp((int32)C.B, 255, Peak), 0, 255),
                   255);
      }

      Out.Colors.Add(C);
      Out.Normals.Add(FVector::UpVector); // flat normals (refined below)
    }
  }

  // --- Smooth normals from height samples ---
  for (int32 Row = 0; Row <= GridN; ++Row) {
    for (int32 Col = 0; Col <= GridN; ++Col) {
      const int32 L = FMath::Max(Col - 1, 0);
      const int32 R = FMath::Min(Col + 1, GridN);
      const int32 Bu = FMath::Max(Row - 1, 0);
      const int32 T = FMath::Min(Row + 1, GridN);
      const float dX = Heights[Row * Verts + R] - Heights[Row * Verts + L];
      const float dY = Heights[T * Verts + Col] - Heights[Bu * Verts + Col];
      FVector N(-dX / (Step * 2.f), -dY / (Step * 2.f), 1.f);
      N.Normalize();
      Out.Normals[Row * Verts + Col] = N;
    }
  }

  // --- Generate triangle indices ---
  Out.Triangles.Reserve(GridN * GridN * 6);
  for (int32 Row = 0; Row < GridN; ++Row) {
    for (int32 Col = 0; Col < GridN; ++Col) {
      const int32 TL = Row * Verts + Col;
      const int32 TR = TL + 1;
      const int32 BL = TL + Verts;
      const int32 BR = BL + 1;
      Out.Triangles.Add(TL);
      Out.Triangles.Add(TR);
      Out.Triangles.Add(BL);
      Out.Triangles.Add(TR);
      Out.Triangles.Add(BR);
      Out.Triangles.Add(BL);
    }
  }

  return Out;
}
