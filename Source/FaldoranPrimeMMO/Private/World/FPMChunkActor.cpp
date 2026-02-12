// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMChunkActor.h"

// =====================================================================
//  Constructor
// =====================================================================

AFPMChunkActor::AFPMChunkActor() {
  PrimaryActorTick.bCanEverTick = false;

  TerrainMesh =
      CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
  RootComponent = TerrainMesh;

  // Per-triangle collision (not convex hull).
  TerrainMesh->bUseComplexAsSimpleCollision = true;

  // Synchronous cooking so collision is ready immediately.
  TerrainMesh->bUseAsyncCooking = false;

  TerrainMesh->SetCastShadow(true);
  TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
  TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// =====================================================================
//  Height / Color helpers
// =====================================================================

float AFPMChunkActor::HeightToWorldZ(float NormalizedHeight) {
  // Reduced range: 50m total height (was 120m).
  // This makes all slopes ~2.4x gentler, eliminating sawtooth artifacts.
  return -400.0f + NormalizedHeight * 5000.0f;
}

FLinearColor AFPMChunkActor::BiomeToVertexColor(EFPMBiome Biome) {
  switch (Biome) {
  case EFPMBiome::Meadows:
    return FLinearColor(1.f, 0.f, 0.f, 0.f);
  case EFPMBiome::Forest:
    return FLinearColor(0.f, 1.f, 0.f, 0.f);
  case EFPMBiome::Mountain:
    return FLinearColor(0.f, 0.f, 1.f, 0.f);
  case EFPMBiome::Coast:
    return FLinearColor(.2f, 0.f, 0.f, 0.f);
  case EFPMBiome::Swamp:
    return FLinearColor(.5f, 0.f, .5f, 0.f);
  case EFPMBiome::Snow:
    return FLinearColor(0.f, 0.f, 0.f, 1.f);
  case EFPMBiome::Ocean:
    return FLinearColor(0.f, 0.f, 0.f, 0.f);
  default:
    return FLinearColor(.5f, .5f, .5f, 0.f);
  }
}

// =====================================================================
//  InitializeChunk  (simplified — no mobility dance)
// =====================================================================

void AFPMChunkActor::InitializeChunk(const FFPMChunkHeightmapData &InData,
                                     EFPMChunkLOD InLOD) {
  ChunkData = InData;
  CurrentLOD = InLOD;

  SetActorLocation(FPMChunkGenerator::ChunkToWorldOrigin(InData.Coord));

  switch (InLOD) {
  case EFPMChunkLOD::Full:
    BuildMesh(1, true);
    break;
  case EFPMChunkLOD::Medium:
    BuildMesh(2, true);
    break;
  case EFPMChunkLOD::Low:
    BuildMesh(4, false);
    break;
  case EFPMChunkLOD::Unloaded:
    TerrainMesh->ClearAllMeshSections();
    break;
  }
}

// =====================================================================
//  SetChunkLOD
// =====================================================================

void AFPMChunkActor::SetChunkLOD(EFPMChunkLOD NewLOD) {
  if (NewLOD == CurrentLOD)
    return;
  CurrentLOD = NewLOD;

  switch (NewLOD) {
  case EFPMChunkLOD::Full:
    BuildMesh(1, true);
    break;
  case EFPMChunkLOD::Medium:
    BuildMesh(2, true);
    break;
  case EFPMChunkLOD::Low:
    BuildMesh(4, false);
    break;
  case EFPMChunkLOD::Unloaded:
    TerrainMesh->ClearAllMeshSections();
    break;
  }
}

// =====================================================================
//  BuildMesh
// =====================================================================

void AFPMChunkActor::BuildMesh(int32 LODStep, bool bCollision) {
  if (!ChunkData.bIsValid) {
    return;
  }

  TerrainMesh->ClearAllMeshSections();

  constexpr int32 FullRes = FPMChunkConstants::ChunkResolution;
  const int32 Res = (FullRes - 1) / LODStep + 1;
  const float Size = FPMChunkConstants::ChunkWorldSize;

  // ---- 1. Vertices ----
  const int32 NumVerts = Res * Res;

  TArray<FVector> Verts;
  TArray<FVector2D> UVs;
  TArray<FLinearColor> VColors;

  Verts.Reserve(NumVerts);
  UVs.Reserve(NumVerts);
  VColors.Reserve(NumVerts);

  for (int32 Y = 0; Y < Res; ++Y) {
    for (int32 X = 0; X < Res; ++X) {
      const int32 SrcX = FMath::Min(X * LODStep, FullRes - 1);
      const int32 SrcY = FMath::Min(Y * LODStep, FullRes - 1);
      const int32 SrcIdx = SrcY * FullRes + SrcX;

      const float U = static_cast<float>(X) / (Res - 1);
      const float V = static_cast<float>(Y) / (Res - 1);

      Verts.Emplace(U * Size, V * Size,
                    HeightToWorldZ(ChunkData.HeightValues[SrcIdx]));
      UVs.Emplace(U, V);
      VColors.Add(BiomeToVertexColor(ChunkData.BiomeValues[SrcIdx]));
    }
  }

  // ---- 2. Triangles (alternating diagonal to prevent sawtooth) ----
  //
  // Checkerboard pattern: even quads split BL→TR, odd quads split TL→BR.
  // This eliminates the directional bias that causes visible zigzag
  // patterns on slopes.
  const int32 NumQuads = (Res - 1) * (Res - 1);
  TArray<int32> Tris;
  Tris.Reserve(NumQuads * 6);

  for (int32 Y = 0; Y < Res - 1; ++Y) {
    for (int32 X = 0; X < Res - 1; ++X) {
      const int32 BL = Y * Res + X;
      const int32 BR = BL + 1;
      const int32 TL = BL + Res;
      const int32 TR = TL + 1;

      if ((X + Y) % 2 == 0) {
        // Even: split along BL→TR diagonal
        Tris.Add(BL);
        Tris.Add(TL);
        Tris.Add(TR);

        Tris.Add(BL);
        Tris.Add(TR);
        Tris.Add(BR);
      } else {
        // Odd: split along TL→BR diagonal
        Tris.Add(BL);
        Tris.Add(TL);
        Tris.Add(BR);

        Tris.Add(BR);
        Tris.Add(TL);
        Tris.Add(TR);
      }
    }
  }

  // ---- 3. Normals (flipped to +Z for lighting) ----
  TArray<FVector> Normals;
  Normals.SetNumZeroed(NumVerts);

  for (int32 i = 0; i < Tris.Num(); i += 3) {
    const FVector &A = Verts[Tris[i]];
    const FVector &B = Verts[Tris[i + 1]];
    const FVector &C = Verts[Tris[i + 2]];

    FVector FaceN = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
    if (FaceN.Z < 0.f)
      FaceN = -FaceN;

    Normals[Tris[i]] += FaceN;
    Normals[Tris[i + 1]] += FaceN;
    Normals[Tris[i + 2]] += FaceN;
  }

  for (FVector &N : Normals) {
    N = N.GetSafeNormal();
    if (N.IsNearlyZero())
      N = FVector::UpVector;
  }

  // ---- 4. Skirts ----
  constexpr float SkirtDrop = 200.f; // Reduced for flatter terrain

  auto AddSkirtQuad = [&](int32 IdxA, int32 IdxB) {
    const FVector PA = Verts[IdxA];
    const FVector PB = Verts[IdxB];
    const FVector2D UvA = UVs[IdxA];
    const FVector2D UvB = UVs[IdxB];
    const FLinearColor ColA = VColors[IdxA];
    const FLinearColor ColB = VColors[IdxB];

    const int32 BotA = Verts.Num();
    Verts.Emplace(PA.X, PA.Y, PA.Z - SkirtDrop);
    UVs.Add(UvA);
    VColors.Add(ColA);
    Normals.Add(FVector::DownVector);

    const int32 BotB = Verts.Num();
    Verts.Emplace(PB.X, PB.Y, PB.Z - SkirtDrop);
    UVs.Add(UvB);
    VColors.Add(ColB);
    Normals.Add(FVector::DownVector);

    Tris.Add(IdxA);
    Tris.Add(IdxB);
    Tris.Add(BotA);

    Tris.Add(IdxB);
    Tris.Add(BotB);
    Tris.Add(BotA);
  };

  for (int32 X = 0; X < Res - 1; ++X)
    AddSkirtQuad(X + 1, X);
  for (int32 X = 0; X < Res - 1; ++X) {
    const int32 Row = (Res - 1) * Res;
    AddSkirtQuad(Row + X, Row + X + 1);
  }
  for (int32 Y = 0; Y < Res - 1; ++Y)
    AddSkirtQuad(Y * Res, (Y + 1) * Res);
  for (int32 Y = 0; Y < Res - 1; ++Y) {
    const int32 Col = Res - 1;
    AddSkirtQuad((Y + 1) * Res + Col, Y * Res + Col);
  }

  // ---- 5. Create section ----
  TArray<FColor> Colors;
  Colors.Reserve(VColors.Num());
  for (const FLinearColor &LC : VColors) {
    Colors.Add(LC.ToFColor(true));
  }

  TArray<FProcMeshTangent> Tangents;

  // CreateMeshSection with bCreateCollision=true internally:
  //   1. Creates a BodySetup with bDoubleSidedGeometry=true (PMC default)
  //   2. Sets CollisionTraceFlag = CTF_UseComplexAsSimple (from our flag)
  //   3. Cooks the trimesh collision
  //   4. Calls RecreatePhysicsState()
  // So collision is fully ready when this returns. No extra calls needed.
  TerrainMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents,
                                 bCollision);

  TerrainMesh->SetCollisionEnabled(bCollision
                                       ? ECollisionEnabled::QueryAndPhysics
                                       : ECollisionEnabled::NoCollision);
}
