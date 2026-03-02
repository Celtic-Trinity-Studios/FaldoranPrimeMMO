// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPMTerrainShell.generated.h"


class UProceduralMeshComponent;

/**
 * AFPMTerrainShell
 *
 * A camera-relative low-resolution terrain preview mesh displayed while the
 * Rift Runner is active. Avoids UE float-precision issues by keeping all
 * vertex positions relative to the player's current world location.
 *
 * Two rings are generated on a background thread:
 *   Near  : 128x128 grid, ±50 km  →  ~780 m per vertex
 *   Far   :  64x64  grid, ±400 km →  ~12.5 km per vertex (silhouette only)
 *
 * The shell re-bakes every RebakeIntervalSec seconds while Rift Runner is on.
 * On deactivation the shell actor is destroyed.
 */
UCLASS(NotBlueprintable)
class FALDORANPRIMEMMO_API AFPMTerrainShell : public AActor {
  GENERATED_BODY()

public:
  AFPMTerrainShell();

  virtual void Tick(float DeltaTime) override;

  /** Call once after spawning to supply the world seed. */
  void Initialize(int32 InWorldSeed);

  /** How often (seconds) to re-bake the mesh while Rift Runner is active. */
  UPROPERTY(EditAnywhere, Category = "FPM|TerrainShell")
  float RebakeIntervalSec = 3.0f;

private:
  UPROPERTY(VisibleAnywhere)
  TObjectPtr<UProceduralMeshComponent> NearMesh;

  UPROPERTY(VisibleAnywhere)
  TObjectPtr<UProceduralMeshComponent> FarMesh;

  int32 WorldSeed = 0;
  float TimeSinceRebake = 9999.f; // trigger immediately on first tick
  bool bBakeInFlight = false;

  /** Bake both rings on a background thread, then apply on game thread. */
  void TriggerAsyncBake(FVector CentreWorld);

  struct FShellMeshData {
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FColor> Colors;
  };

  /** Build a grid ring mesh. CentreWorld is used for terrain height sampling;
   *  vertices are returned relative to CentreWorld (so the shell sits at
   *  origin and avoids float-precision drift). */
  static FShellMeshData BuildRing(FVector CentreWorld, int32 GridN,
                                  float HalfExtentCm, int32 WorldSeed);
};
