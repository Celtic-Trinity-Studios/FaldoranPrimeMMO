// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
//
// Pipe-model water flow simulation with anti-flood safeguards.
// Reference: Mei, Decaudin, Hu (2007) — GPU Hydraulic Erosion

#include "World/FPMWaterSimulation.h"
#include "World/FPMChunkData.h"

// Direction offsets: N=0, E=1, S=2, W=3
static constexpr int32 DirDX[4] = {0, 1, 0, -1};
static constexpr int32 DirDY[4] = {-1, 0, 1, 0};

// Opposite direction mapping (for cross-chunk flow):
// N↔S, E↔W
static constexpr int32 OppositeDir[4] = {2, 3, 0, 1};

// ===================================================================
//  Settings Loading from INI
// ===================================================================

void FPMWaterSimulation::LoadSettingsFromINI(const FString &IniPath) {
  if (!FPaths::FileExists(IniPath)) {
    UE_LOG(LogTemp, Warning,
           TEXT("FPM Water: No WorldGen.ini found, using default water "
                "settings"));
    return;
  }

  // [WaterSources] section
  int32 Tmp;
  float FTmp;

  if (GConfig->GetInt(TEXT("WaterSources"), TEXT("SourcesPerMountainChunk"),
                      Tmp, IniPath)) {
    FPMWaterConstants::SourcesPerMountainChunk = Tmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSources"), TEXT("SpringFlowRate"), FTmp,
                        IniPath)) {
    FPMWaterConstants::SpringFlowRate = FTmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSources"), TEXT("MinSpringElevation"), FTmp,
                        IniPath)) {
    FPMWaterConstants::MinSpringElevation = FTmp;
  }

  // [WaterSimulation] section
  if (GConfig->GetFloat(TEXT("WaterSimulation"), TEXT("SimulationRate"), FTmp,
                        IniPath)) {
    FPMWaterConstants::SimulationRate = FTmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSimulation"), TEXT("FlowGravity"), FTmp,
                        IniPath)) {
    FPMWaterConstants::FlowGravity = FTmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSimulation"), TEXT("MinRenderDepth"), FTmp,
                        IniPath)) {
    FPMWaterConstants::MinRenderDepth = FTmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSimulation"), TEXT("EvaporationRate"), FTmp,
                        IniPath)) {
    FPMWaterConstants::EvaporationRate = FTmp;
  }
  if (GConfig->GetFloat(TEXT("WaterSimulation"), TEXT("MaxWaterDepth"), FTmp,
                        IniPath)) {
    FPMWaterConstants::MaxWaterDepth = FTmp;
  }
  if (GConfig->GetInt(TEXT("WaterSimulation"), TEXT("MaxFlowHops"), Tmp,
                      IniPath)) {
    FPMWaterConstants::MaxFlowHops = Tmp;
  }

  UE_LOG(LogTemp, Log,
         TEXT("FPM Water: Settings loaded — SimRate=%.0fHz, "
              "Evap=%.4f, MaxDepth=%.0fcm, MaxHops=%d, "
              "Sources/Mtn=%d, SpringFlow=%.0f"),
         FPMWaterConstants::SimulationRate, FPMWaterConstants::EvaporationRate,
         FPMWaterConstants::MaxWaterDepth, FPMWaterConstants::MaxFlowHops,
         FPMWaterConstants::SourcesPerMountainChunk,
         FPMWaterConstants::SpringFlowRate);
}

// ===================================================================
//  Terrain Sampling (water grid → terrain grid interpolation)
// ===================================================================

float FPMWaterSimulation::SampleTerrainHeight(
    const FFPMChunkHeightmapData &Terrain, int32 WaterX, int32 WaterY) {

  if (!Terrain.bIsValid || Terrain.HeightValues.Num() == 0) {
    return 0.0f;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;
  const int32 TRes = FPMChunkConstants::ChunkResolution;

  // Map water grid position to terrain grid position (bilinear)
  const float TU = static_cast<float>(WaterX) / (WRes - 1) * (TRes - 1);
  const float TV = static_cast<float>(WaterY) / (WRes - 1) * (TRes - 1);

  const int32 TX0 = FMath::Clamp(FMath::FloorToInt(TU), 0, TRes - 2);
  const int32 TY0 = FMath::Clamp(FMath::FloorToInt(TV), 0, TRes - 2);
  const int32 TX1 = TX0 + 1;
  const int32 TY1 = TY0 + 1;

  const float FracX = TU - TX0;
  const float FracY = TV - TY0;

  const float H00 = Terrain.HeightValues[TY0 * TRes + TX0];
  const float H10 = Terrain.HeightValues[TY0 * TRes + TX1];
  const float H01 = Terrain.HeightValues[TY1 * TRes + TX0];
  const float H11 = Terrain.HeightValues[TY1 * TRes + TX1];

  // Bilinear interpolation
  const float H = FMath::Lerp(FMath::Lerp(H00, H10, FracX),
                              FMath::Lerp(H01, H11, FracX), FracY);

  // Convert normalized height to world Z
  return FPMChunkConstants::MinWorldZ + H * FPMChunkConstants::WorldHeightRange;
}

EFPMBiome FPMWaterSimulation::SampleBiome(const FFPMChunkHeightmapData &Terrain,
                                          int32 WaterX, int32 WaterY) {

  if (!Terrain.bIsValid || Terrain.BiomeValues.Num() == 0) {
    return EFPMBiome::Meadows;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;
  const int32 TRes = FPMChunkConstants::ChunkResolution;

  // Nearest-neighbor lookup
  const int32 TX = FMath::Clamp(
      FMath::RoundToInt(static_cast<float>(WaterX) / (WRes - 1) * (TRes - 1)),
      0, TRes - 1);
  const int32 TY = FMath::Clamp(
      FMath::RoundToInt(static_cast<float>(WaterY) / (WRes - 1) * (TRes - 1)),
      0, TRes - 1);

  return Terrain.BiomeValues[TY * TRes + TX];
}

// ===================================================================
//  Neighbor Index
// ===================================================================

int32 FPMWaterSimulation::GetNeighborIdx(int32 X, int32 Y, int32 Dir) {
  const int32 NX = X + DirDX[Dir];
  const int32 NY = Y + DirDY[Dir];
  const int32 WRes = FPMWaterConstants::WaterResolution;

  if (NX < 0 || NX >= WRes || NY < 0 || NY >= WRes) {
    return -1; // Out of chunk — cross-chunk flow
  }

  return NY * WRes + NX;
}

// ===================================================================
//  Source Injection
// ===================================================================

void FPMWaterSimulation::InjectSources(
    FFPMChunkWaterData &Water, const TArray<FFPMWaterSourceDef> &Sources,
    float DeltaTime) {
  if (!Water.bAllocated || Sources.Num() == 0) {
    return;
  }

  for (const FFPMWaterSourceDef &Src : Sources) {
    if (Src.CellIndex < 0 ||
        Src.CellIndex >= FPMWaterConstants::WaterCellCount) {
      continue;
    }

    const float FlowRate = Src.bInfinite ? Src.FlowRate : 0.0f;
    if (FlowRate <= 0.0f) {
      continue;
    }

    // Inject water volume: FlowRate * DeltaTime, distributed over cell area
    // Cell area = (ChunkWorldSize / (WaterResolution-1))² ≈ 800² = 640000 cm²
    // Depth increase = Volume / Area = FlowRate * DeltaTime / Area
    const float CellSize = FPMChunkConstants::ChunkWorldSize /
                           (FPMWaterConstants::WaterResolution - 1);
    const float CellArea = CellSize * CellSize;
    const float DepthIncrease = (FlowRate * DeltaTime) / CellArea;

    Water.WaterDepth[Src.CellIndex] += DepthIncrease;
    Water.WaterDepth[Src.CellIndex] = FMath::Min(
        Water.WaterDepth[Src.CellIndex], FPMWaterConstants::MaxWaterDepth);

    // Source cell has distance 0 from source
    Water.SourceDistance[Src.CellIndex] = 0;
    Water.bHasWater = true;
    Water.bHasSource = true;
  }
}

// ===================================================================
//  Core Pipe-Model Simulation
// ===================================================================

void FPMWaterSimulation::SimulateChunk(FFPMChunkWaterData &Water,
                                       const FFPMChunkHeightmapData &Terrain,
                                       float DeltaTime) {
  if (!Water.bAllocated || DeltaTime <= 0.0f) {
    return;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;
  const float Gravity = FPMWaterConstants::FlowGravity;
  const float Evap = FPMWaterConstants::EvaporationRate;
  const int32 MaxRange = FPMWaterConstants::MaxFlowHops;
  const float MaxDepth = FPMWaterConstants::MaxWaterDepth;
  const float MinRender = FPMWaterConstants::MinRenderDepth;
  const float Damping = FPMWaterConstants::PipeDamping;
  const float BeyondDecay = FPMWaterConstants::BeyondRangeDecayMultiplier;

  // Cell spacing in cm
  const float CellSize = FPMChunkConstants::ChunkWorldSize / (WRes - 1);
  const float CellArea = CellSize * CellSize;

  bool bAnyWater = false;
  bool bMeshChanged = false;

  // ================================================================
  //  PASS 1: Update pipe flows based on height differences
  // ================================================================
  for (int32 Y = 0; Y < WRes; ++Y) {
    for (int32 X = 0; X < WRes; ++X) {
      const int32 Idx = Y * WRes + X;

      // Skip completely dry cells with no source nearby
      if (Water.WaterDepth[Idx] <= 0.0f && Water.SourceDistance[Idx] > 0) {
        // Zero out pipes for dry cells
        for (int32 Dir = 0; Dir < 4; ++Dir) {
          Water.SetPipe(Idx, Dir, 0.0f);
        }
        continue;
      }

      // --- Anti-flood: Ocean/Coast drain ---
      const EFPMBiome CellBiome = SampleBiome(Terrain, X, Y);
      if (CellBiome == EFPMBiome::Ocean || CellBiome == EFPMBiome::Coast) {
        // Ocean absorbs all water instantly
        if (Water.WaterDepth[Idx] > 0.0f) {
          bMeshChanged = true;
        }
        Water.WaterDepth[Idx] = 0.0f;
        for (int32 Dir = 0; Dir < 4; ++Dir) {
          Water.SetPipe(Idx, Dir, 0.0f);
        }
        continue;
      }

      // --- Anti-flood: Range decay ---
      if (Water.SourceDistance[Idx] > MaxRange) {
        const float OldDepth = Water.WaterDepth[Idx];
        Water.WaterDepth[Idx] *= (1.0f - BeyondDecay * Evap * DeltaTime);
        if (Water.WaterDepth[Idx] < MinRender * 0.1f) {
          Water.WaterDepth[Idx] = 0.0f;
        }
        if (FMath::Abs(OldDepth - Water.WaterDepth[Idx]) > 0.01f) {
          bMeshChanged = true;
        }
        continue;
      }

      const float TerrainZ = SampleTerrainHeight(Terrain, X, Y);
      const float WaterZ = TerrainZ + Water.WaterDepth[Idx];

      // --- Update pipe flows for each cardinal direction ---
      float TotalOutflowRate = 0.0f;

      for (int32 Dir = 0; Dir < 4; ++Dir) {
        const int32 NIdx = GetNeighborIdx(X, Y, Dir);

        if (NIdx < 0) {
          // Edge of chunk — outflow goes to cross-chunk resolution
          // For now, apply damping (water slows at chunk edges)
          float Pipe = Water.GetPipe(Idx, Dir) * Damping * 0.5f;
          Water.SetPipe(Idx, Dir, FMath::Max(0.0f, Pipe));
          TotalOutflowRate += FMath::Max(0.0f, Pipe);
          continue;
        }

        const int32 NX = X + DirDX[Dir];
        const int32 NY = Y + DirDY[Dir];
        const float NTerrainZ = SampleTerrainHeight(Terrain, NX, NY);
        const float NWaterZ = NTerrainZ + Water.WaterDepth[NIdx];

        // Height difference drives flow
        const float DeltaH = WaterZ - NWaterZ;

        // Update pipe: accelerate flow in the direction of height gradient
        float Pipe = Water.GetPipe(Idx, Dir);
        Pipe = Pipe * Damping + DeltaH * Gravity * DeltaTime / CellSize;
        Pipe = FMath::Max(0.0f, Pipe); // No negative (backward) flow

        Water.SetPipe(Idx, Dir, Pipe);
        TotalOutflowRate += Pipe;
      }

      // --- Volume conservation: can't outflow more water than available ---
      const float AvailableVolume = Water.WaterDepth[Idx] * CellArea;
      const float TotalOutflowVolume = TotalOutflowRate * CellArea * DeltaTime;

      if (TotalOutflowVolume > AvailableVolume && TotalOutflowRate > 0.0f) {
        const float Scale = AvailableVolume / TotalOutflowVolume;
        for (int32 Dir = 0; Dir < 4; ++Dir) {
          Water.SetPipe(Idx, Dir, Water.GetPipe(Idx, Dir) * Scale);
        }
        TotalOutflowRate *= Scale;
      }
    }
  }

  // ================================================================
  //  PASS 2: Apply flow — update water depths
  // ================================================================
  // Use a temporary buffer to avoid order-dependent artifacts
  TArray<float> NewDepth;
  NewDepth.SetNum(FPMWaterConstants::WaterCellCount);
  FMemory::Memcpy(NewDepth.GetData(), Water.WaterDepth.GetData(),
                  NewDepth.Num() * sizeof(float));

  TArray<int32> NewDistance;
  NewDistance.SetNum(FPMWaterConstants::WaterCellCount);
  FMemory::Memcpy(NewDistance.GetData(), Water.SourceDistance.GetData(),
                  NewDistance.Num() * sizeof(int32));

  for (int32 Y = 0; Y < WRes; ++Y) {
    for (int32 X = 0; X < WRes; ++X) {
      const int32 Idx = Y * WRes + X;

      if (Water.WaterDepth[Idx] <= 0.0f && Water.SourceDistance[Idx] > 0) {
        continue;
      }

      float NetFlow = 0.0f;

      // Outflow from this cell
      for (int32 Dir = 0; Dir < 4; ++Dir) {
        const float OutPipe = Water.GetPipe(Idx, Dir);
        NetFlow -= OutPipe * DeltaTime;

        // Add to neighbor
        const int32 NIdx = GetNeighborIdx(X, Y, Dir);
        if (NIdx >= 0 && OutPipe > 0.0f) {
          NewDepth[NIdx] += OutPipe * DeltaTime;

          // Propagate source distance (minimum of current paths)
          NewDistance[NIdx] =
              FMath::Min(NewDistance[NIdx], Water.SourceDistance[Idx] + 1);
        }
      }

      // Inflow from neighbors (via their outgoing pipes toward us)
      for (int32 Dir = 0; Dir < 4; ++Dir) {
        const int32 NIdx = GetNeighborIdx(X, Y, Dir);
        if (NIdx >= 0) {
          const float InPipe = Water.GetPipe(NIdx, OppositeDir[Dir]);
          NetFlow += InPipe * DeltaTime;
        }
      }

      NewDepth[Idx] += NetFlow;

      // --- Evaporation (anti-flood mechanism #1) ---
      NewDepth[Idx] -= Evap * DeltaTime;

      // Clamp to valid range
      NewDepth[Idx] = FMath::Clamp(NewDepth[Idx], 0.0f, MaxDepth);

      if (NewDepth[Idx] > MinRender * 0.1f) {
        bAnyWater = true;
      }

      // Track if mesh needs updating
      if (FMath::Abs(NewDepth[Idx] - Water.WaterDepth[Idx]) > 0.01f) {
        bMeshChanged = true;
      }
    }
  }

  // Copy results back
  FMemory::Memcpy(Water.WaterDepth.GetData(), NewDepth.GetData(),
                  NewDepth.Num() * sizeof(float));
  FMemory::Memcpy(Water.SourceDistance.GetData(), NewDistance.GetData(),
                  NewDistance.Num() * sizeof(int32));

  Water.bHasWater = bAnyWater;
  Water.bMeshDirty = bMeshChanged;
}

// ===================================================================
//  Cross-Chunk Flow Resolution
// ===================================================================

void FPMWaterSimulation::ResolveCrossChunkFlow(
    TMap<FFPMChunkCoord, FFPMChunkWaterData> &ChunkWaterMap) {

  const int32 WRes = FPMWaterConstants::WaterResolution;

  // For each chunk with water, check edge cells and transfer flow
  // to the neighboring chunk's corresponding edge cells.
  //
  // Edge mapping:
  //   North edge (Y=0) → neighbor at (Q, R-1), their South edge (Y=WRes-1)
  //   South edge (Y=WRes-1) → neighbor at (Q, R+1), their North edge (Y=0)
  //   West edge (X=0) → neighbor at (Q-1, R), their East edge (X=WRes-1)
  //   East edge (X=WRes-1) → neighbor at (Q+1, R), their West edge (X=0)

  struct FEdgeTransfer {
    FFPMChunkCoord FromCoord;
    FFPMChunkCoord ToCoord;
    int32 FromCellIdx;
    int32 ToCellIdx;
    float FlowAmount;
    int32 SourceDist;
  };

  TArray<FEdgeTransfer> Transfers;

  for (auto &Pair : ChunkWaterMap) {
    const FFPMChunkCoord &Coord = Pair.Key;
    FFPMChunkWaterData &Water = Pair.Value;

    if (!Water.bAllocated || !Water.bHasWater) {
      continue;
    }

    // Check North edge (Y=0)
    {
      const FFPMChunkCoord NCoord(Coord.Q, Coord.R - 1);
      if (ChunkWaterMap.Contains(NCoord)) {
        for (int32 X = 0; X < WRes; ++X) {
          const int32 Idx = X;                      // Y=0
          const float Pipe = Water.GetPipe(Idx, 0); // North pipe
          if (Pipe > 0.001f) {
            const int32 NIdx = (WRes - 1) * WRes + X; // South edge of neighbor
            FEdgeTransfer T;
            T.FromCoord = Coord;
            T.ToCoord = NCoord;
            T.FromCellIdx = Idx;
            T.ToCellIdx = NIdx;
            T.FlowAmount = Pipe * (1.0f / FPMWaterConstants::SimulationRate);
            T.SourceDist = Water.SourceDistance[Idx] + 1;
            Transfers.Add(T);
          }
        }
      }
    }

    // Check South edge (Y=WRes-1)
    {
      const FFPMChunkCoord SCoord(Coord.Q, Coord.R + 1);
      if (ChunkWaterMap.Contains(SCoord)) {
        for (int32 X = 0; X < WRes; ++X) {
          const int32 Idx = (WRes - 1) * WRes + X;
          const float Pipe = Water.GetPipe(Idx, 2); // South pipe
          if (Pipe > 0.001f) {
            const int32 NIdx = X; // North edge of neighbor
            FEdgeTransfer T;
            T.FromCoord = Coord;
            T.ToCoord = SCoord;
            T.FromCellIdx = Idx;
            T.ToCellIdx = NIdx;
            T.FlowAmount = Pipe * (1.0f / FPMWaterConstants::SimulationRate);
            T.SourceDist = Water.SourceDistance[Idx] + 1;
            Transfers.Add(T);
          }
        }
      }
    }

    // Check West edge (X=0)
    {
      const FFPMChunkCoord WCoord(Coord.Q - 1, Coord.R);
      if (ChunkWaterMap.Contains(WCoord)) {
        for (int32 Y = 0; Y < WRes; ++Y) {
          const int32 Idx = Y * WRes;
          const float Pipe = Water.GetPipe(Idx, 3); // West pipe
          if (Pipe > 0.001f) {
            const int32 NIdx = Y * WRes + (WRes - 1); // East edge of neighbor
            FEdgeTransfer T;
            T.FromCoord = Coord;
            T.ToCoord = WCoord;
            T.FromCellIdx = Idx;
            T.ToCellIdx = NIdx;
            T.FlowAmount = Pipe * (1.0f / FPMWaterConstants::SimulationRate);
            T.SourceDist = Water.SourceDistance[Idx] + 1;
            Transfers.Add(T);
          }
        }
      }
    }

    // Check East edge (X=WRes-1)
    {
      const FFPMChunkCoord ECoord(Coord.Q + 1, Coord.R);
      if (ChunkWaterMap.Contains(ECoord)) {
        for (int32 Y = 0; Y < WRes; ++Y) {
          const int32 Idx = Y * WRes + (WRes - 1);
          const float Pipe = Water.GetPipe(Idx, 1); // East pipe
          if (Pipe > 0.001f) {
            const int32 NIdx = Y * WRes; // West edge of neighbor
            FEdgeTransfer T;
            T.FromCoord = Coord;
            T.ToCoord = ECoord;
            T.FromCellIdx = Idx;
            T.ToCellIdx = NIdx;
            T.FlowAmount = Pipe * (1.0f / FPMWaterConstants::SimulationRate);
            T.SourceDist = Water.SourceDistance[Idx] + 1;
            Transfers.Add(T);
          }
        }
      }
    }
  }

  // Apply all transfers
  for (const FEdgeTransfer &T : Transfers) {
    FFPMChunkWaterData *ToWater = ChunkWaterMap.Find(T.ToCoord);
    FFPMChunkWaterData *FromWater = ChunkWaterMap.Find(T.FromCoord);

    if (ToWater && ToWater->bAllocated && FromWater && FromWater->bAllocated) {
      // Transfer water
      const float Amount =
          FMath::Min(T.FlowAmount, FromWater->WaterDepth[T.FromCellIdx]);
      FromWater->WaterDepth[T.FromCellIdx] -= Amount;
      ToWater->WaterDepth[T.ToCellIdx] += Amount;

      // Clamp
      FromWater->WaterDepth[T.FromCellIdx] =
          FMath::Max(0.0f, FromWater->WaterDepth[T.FromCellIdx]);
      ToWater->WaterDepth[T.ToCellIdx] = FMath::Min(
          ToWater->WaterDepth[T.ToCellIdx], FPMWaterConstants::MaxWaterDepth);

      // Propagate source distance
      ToWater->SourceDistance[T.ToCellIdx] =
          FMath::Min(ToWater->SourceDistance[T.ToCellIdx], T.SourceDist);

      ToWater->bHasWater = true;
      ToWater->bMeshDirty = true;
      FromWater->bMeshDirty = true;
    }
  }
}

// ===================================================================
//  Flow Vector Computation (for rendering UV animation)
// ===================================================================

void FPMWaterSimulation::ComputeFlowVectors(FFPMChunkWaterData &Water) {
  if (!Water.bAllocated) {
    return;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;

  for (int32 Y = 0; Y < WRes; ++Y) {
    for (int32 X = 0; X < WRes; ++X) {
      const int32 Idx = Y * WRes + X;

      if (Water.WaterDepth[Idx] <= FPMWaterConstants::MinRenderDepth * 0.5f) {
        Water.FlowDirection[Idx] = FVector2D::ZeroVector;
        Water.FlowSpeed[Idx] = 0.0f;
        continue;
      }

      // Compute flow vector from pipe values
      // East-West component from E and W pipes
      const float FlowX = Water.GetPipe(Idx, 1) - Water.GetPipe(Idx, 3);
      // North-South component from S and N pipes
      const float FlowY = Water.GetPipe(Idx, 2) - Water.GetPipe(Idx, 0);

      const FVector2D FlowVec(FlowX, FlowY);
      const float Speed = FlowVec.Size();

      Water.FlowSpeed[Idx] = Speed;
      Water.FlowDirection[Idx] =
          Speed > 0.001f ? FlowVec / Speed : FVector2D::ZeroVector;
    }
  }
}

// ===================================================================
//  Procedural Source Placement
// ===================================================================

void FPMWaterSimulation::PlaceProceduralSources(
    const FFPMChunkCoord &Coord, int32 WorldSeed,
    const FFPMChunkHeightmapData &Terrain,
    TArray<FFPMWaterSourceDef> &OutSources) {

  OutSources.Empty();

  if (!Terrain.bIsValid || Terrain.HeightValues.Num() == 0) {
    return;
  }

  const int32 WRes = FPMWaterConstants::WaterResolution;
  const int32 MaxSources = FPMWaterConstants::SourcesPerMountainChunk;
  const float MinElev = FPMWaterConstants::MinSpringElevation;
  const float BaseFlow = FPMWaterConstants::SpringFlowRate;

  // Count highland biome cells in this chunk
  int32 HighlandCells = 0;
  for (int32 i = 0; i < Terrain.BiomeValues.Num(); ++i) {
    const EFPMBiome B = Terrain.BiomeValues[i];
    if (B == EFPMBiome::Mountain || B == EFPMBiome::Snow ||
        B == EFPMBiome::Alpine) {
      HighlandCells++;
    }
  }

  // Only place sources in chunks with significant highland area
  if (HighlandCells < Terrain.BiomeValues.Num() / 8) {
    return;
  }

  // Use deterministic noise to select source positions
  // The seed combines WorldSeed + chunk coord for uniqueness
  const int32 ChunkHash =
      FPMChunkGenerator::Hash(Coord.Q * 31, Coord.R * 37, WorldSeed + 77777) *
      100000;

  int32 SourcesPlaced = 0;

  for (int32 Attempt = 0;
       Attempt < MaxSources * 4 && SourcesPlaced < MaxSources; ++Attempt) {

    // Deterministic random position within water grid
    const float RandX =
        FPMChunkGenerator::Hash(Attempt * 3, ChunkHash, WorldSeed + 88888);
    const float RandY =
        FPMChunkGenerator::Hash(Attempt * 3 + 1, ChunkHash, WorldSeed + 99999);

    const int32 WX =
        FMath::Clamp(FMath::FloorToInt(RandX * (WRes - 2)) + 1, 1, WRes - 2);
    const int32 WY =
        FMath::Clamp(FMath::FloorToInt(RandY * (WRes - 2)) + 1, 1, WRes - 2);
    const int32 WIdx = WY * WRes + WX;

    // Check terrain height at this position
    const float H = SampleTerrainHeight(Terrain, WX, WY);
    const float NormH = (H - FPMChunkConstants::MinWorldZ) /
                        FPMChunkConstants::WorldHeightRange;

    if (NormH < MinElev) {
      continue;
    }

    // Check biome
    const EFPMBiome Biome = SampleBiome(Terrain, WX, WY);
    if (Biome != EFPMBiome::Mountain && Biome != EFPMBiome::Snow &&
        Biome != EFPMBiome::Alpine) {
      continue;
    }

    // Place the source
    FFPMWaterSourceDef Src;
    Src.CellIndex = WIdx;
    Src.FlowRate = BaseFlow * (0.5f + RandX); // Vary flow rate
    Src.Type = EFPMWaterSourceType::Spring;
    Src.bInfinite = true;

    OutSources.Add(Src);
    SourcesPlaced++;

    UE_LOG(LogTemp, Verbose,
           TEXT("FPM Water: Placed spring at chunk %s cell (%d,%d) — "
                "Flow=%.1f, Height=%.0f"),
           *Coord.ToString(), WX, WY, Src.FlowRate, H);
  }
}
