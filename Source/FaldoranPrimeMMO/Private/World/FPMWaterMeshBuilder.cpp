// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMWaterMeshBuilder.h"
#include "World/FPMWaterSimulation.h"

bool FPMWaterMeshBuilder::BuildWaterMesh(
    const FFPMChunkWaterData &Water, const FFPMChunkHeightmapData &Terrain,
    const FFPMChunkCoord &ChunkCoord, TArray<FVector> &OutVertices,
    TArray<int32> &OutTriangles, TArray<FVector> &OutNormals,
    TArray<FVector2D> &OutUVs, TArray<FColor> &OutColors) {

  OutVertices.Empty();
  OutTriangles.Empty();
  OutNormals.Empty();
  OutUVs.Empty();
  OutColors.Empty();

  if (!Water.bAllocated || !Water.bHasWater) {
    return false;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;
  const float MinDepth = FPMWaterConstants::MinRenderDepth;
  const float MaxDepth = FPMWaterConstants::MaxWaterDepth;
  const float ChunkSize = FPMChunkConstants::ChunkWorldSize;

  // Build a 2D grid of water vertex indices.
  // -1 means no water at this cell (skip geometry).
  TArray<int32> VertexMap;
  VertexMap.Init(-1, FPMWaterConstants::WaterCellCount);

  // ================================================================
  //  Pass 1: Create vertices where water exists
  // ================================================================
  int32 VertCount = 0;
  for (int32 Y = 0; Y < WRes; ++Y) {
    for (int32 X = 0; X < WRes; ++X) {
      const int32 Idx = Y * WRes + X;

      if (Water.WaterDepth[Idx] < MinDepth) {
        continue;
      }

      // Check if ANY adjacent cell also has water (avoid single-cell quads)
      bool bHasAdjacentWater = false;
      for (int32 Dir = 0; Dir < 4; ++Dir) {
        const int32 NIdx = FPMWaterSimulation::GetNeighborIdx(X, Y, Dir);
        if (NIdx >= 0 && Water.WaterDepth[NIdx] >= MinDepth * 0.5f) {
          bHasAdjacentWater = true;
          break;
        }
      }
      // Single-cell islands are okay — they represent springs or small pools
      // So we don't skip them, but we could if desired

      const float U = static_cast<float>(X) / (WRes - 1);
      const float V = static_cast<float>(Y) / (WRes - 1);

      // Water surface position = terrain + water depth
      const float TerrainZ =
          FPMWaterSimulation::SampleTerrainHeight(Terrain, X, Y);
      const float WaterSurfaceZ = TerrainZ + Water.WaterDepth[Idx];

      // Vertex in chunk-local space
      OutVertices.Emplace(U * ChunkSize, V * ChunkSize, WaterSurfaceZ);

      // UV for water texture
      OutUVs.Emplace(U * 4.0f, V * 4.0f); // Tile 4x per chunk

      // Normal (pointing up — water surface is mostly flat)
      OutNormals.Emplace(0.0f, 0.0f, 1.0f);

      // Vertex color encoding:
      //   R = depth (0-255, clamped to 0-MaxDepth)
      //   G = flow speed (0-255)
      //   B = source distance (0-255, clamped to 0-MaxFlowHops)
      //   A = foam hint (255 near shore, 0 in deep water)
      const float DepthNorm =
          FMath::Clamp(Water.WaterDepth[Idx] / MaxDepth, 0.0f, 1.0f);
      const float SpeedNorm =
          FMath::Clamp(Water.FlowSpeed[Idx] / 100.0f, 0.0f, 1.0f);
      const float DistNorm =
          Water.SourceDistance[Idx] < INT32_MAX
              ? FMath::Clamp(static_cast<float>(Water.SourceDistance[Idx]) /
                                 FPMWaterConstants::MaxFlowHops,
                             0.0f, 1.0f)
              : 1.0f;

      // Foam: stronger at shallow water edges and high flow
      const float FoamAmount = FMath::Clamp(
          (1.0f - DepthNorm) * 0.5f + SpeedNorm * 0.5f, 0.0f, 1.0f);

      OutColors.Add(FColor(static_cast<uint8>(DepthNorm * 255.0f),
                           static_cast<uint8>(SpeedNorm * 255.0f),
                           static_cast<uint8>(DistNorm * 255.0f),
                           static_cast<uint8>(FoamAmount * 255.0f)));

      VertexMap[Idx] = VertCount++;
    }
  }

  if (VertCount == 0) {
    return false;
  }

  // ================================================================
  //  Pass 2: Create triangles between adjacent wet cells
  // ================================================================
  OutTriangles.Reserve(VertCount * 6);

  for (int32 Y = 0; Y < WRes - 1; ++Y) {
    for (int32 X = 0; X < WRes - 1; ++X) {
      const int32 BL = VertexMap[Y * WRes + X];
      const int32 BR = VertexMap[Y * WRes + X + 1];
      const int32 TL = VertexMap[(Y + 1) * WRes + X];
      const int32 TR = VertexMap[(Y + 1) * WRes + X + 1];

      // Need at least 3 vertices to make a triangle
      const int32 Count = (BL >= 0 ? 1 : 0) + (BR >= 0 ? 1 : 0) +
                          (TL >= 0 ? 1 : 0) + (TR >= 0 ? 1 : 0);

      if (Count < 3) {
        continue;
      }

      // Full quad (all 4 vertices have water)
      if (Count == 4) {
        // Two triangles with alternating diagonal
        if ((X + Y) % 2 == 0) {
          OutTriangles.Add(BL);
          OutTriangles.Add(TR);
          OutTriangles.Add(TL);
          OutTriangles.Add(BL);
          OutTriangles.Add(BR);
          OutTriangles.Add(TR);
        } else {
          OutTriangles.Add(BL);
          OutTriangles.Add(BR);
          OutTriangles.Add(TL);
          OutTriangles.Add(BR);
          OutTriangles.Add(TR);
          OutTriangles.Add(TL);
        }
      } else {
        // Partial quad — make one triangle with the available 3 vertices
        TArray<int32, TInlineAllocator<4>> ValidVerts;
        if (BL >= 0)
          ValidVerts.Add(BL);
        if (BR >= 0)
          ValidVerts.Add(BR);
        if (TR >= 0)
          ValidVerts.Add(TR);
        if (TL >= 0)
          ValidVerts.Add(TL);

        if (ValidVerts.Num() >= 3) {
          OutTriangles.Add(ValidVerts[0]);
          OutTriangles.Add(ValidVerts[1]);
          OutTriangles.Add(ValidVerts[2]);
        }
      }
    }
  }

  return OutTriangles.Num() > 0;
}

bool FPMWaterMeshBuilder::UpdateWaterMeshComponent(
    UProceduralMeshComponent *MeshComp, const FFPMChunkWaterData &Water,
    const FFPMChunkHeightmapData &Terrain, const FFPMChunkCoord &Coord) {

  if (!MeshComp) {
    return false;
  }

  TArray<FVector> Vertices;
  TArray<int32> Triangles;
  TArray<FVector> Normals;
  TArray<FVector2D> UVs;
  TArray<FColor> Colors;

  const bool bHasMesh = BuildWaterMesh(Water, Terrain, Coord, Vertices,
                                       Triangles, Normals, UVs, Colors);

  if (!bHasMesh) {
    MeshComp->ClearAllMeshSections();
    MeshComp->SetVisibility(false);
    return false;
  }

  TArray<FProcMeshTangent> Tangents; // Empty — computed by engine

  MeshComp->ClearAllMeshSections();
  MeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors,
                              Tangents, false); // No collision for water

  MeshComp->SetVisibility(true);

  // Water should NOT have collision (players swim through it)
  MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  MeshComp->SetCastShadow(false);

  return true;
}
