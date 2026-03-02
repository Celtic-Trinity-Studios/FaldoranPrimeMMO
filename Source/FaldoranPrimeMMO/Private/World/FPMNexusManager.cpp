// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMNexusManager.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "World/FPMVoxelChunk.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMNexus, Log, All);

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AFPMNexusManager::AFPMNexusManager() {
  PrimaryActorTick.bCanEverTick = false;
  bReplicates = false;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

AFPMNexusManager *AFPMNexusManager::GetOrCreate(UWorld *World) {
  if (!World)
    return nullptr;

  // Try to find an existing instance
  for (TActorIterator<AFPMNexusManager> It(World); It; ++It) {
    return *It;
  }

  // Spawn a new one
  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AFPMNexusManager *Manager = World->SpawnActor<AFPMNexusManager>(
      AFPMNexusManager::StaticClass(), FVector::ZeroVector,
      FRotator::ZeroRotator, Params);

  if (Manager) {
    Manager->LoadFromConfig();
    UE_LOG(LogFPMNexus, Log,
           TEXT("FPM Nexus: Manager spawned with %d nexus(es)"),
           Manager->Nexuses.Num());
  }
  return Manager;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void AFPMNexusManager::BeginPlay() {
  Super::BeginPlay();
  if (!bConfigLoaded) {
    LoadFromConfig();
  }
}

// ---------------------------------------------------------------------------
// Config Loading
//
// WorldGen.ini [Nexus] format:
//
//   [Nexus]
//   NexusCount=1
//   NexusSafeRadius=51200.0   ; cm — applies to ALL nexuses unless overridden
//
//   ; Starter Island Nexus (Continent 0)
//   Nexus0_Name=Aelvorn Nexus
//   Nexus0_WorldX=0.0
//   Nexus0_WorldY=0.0
//   Nexus0_SafeRadius=51200.0  ; optional per-nexus override
//
//   ; Future continents:
//   ; Nexus1_Name=Korrath Nexus
//   ; Nexus1_WorldX=2048000.0
//   ; Nexus1_WorldY=0.0
// ---------------------------------------------------------------------------

void AFPMNexusManager::LoadFromConfig() {
  const FString IniPath = FPaths::ProjectConfigDir() + TEXT("WorldGen.ini");

  // ---- Read global safe radius (default if per-nexus not specified) ----
  float GlobalSafeRadius = 51200.f; // ~512m
  GConfig->GetFloat(TEXT("Nexus"), TEXT("NexusSafeRadius"), GlobalSafeRadius,
                    IniPath);

  // ---- Read nexus count ----
  int32 NexusCount = 1; // Always at least one (starter island)
  GConfig->GetInt(TEXT("Nexus"), TEXT("NexusCount"), NexusCount, IniPath);
  NexusCount = FMath::Max(1, NexusCount);

  Nexuses.Reset();
  Nexuses.Reserve(NexusCount);

  for (int32 I = 0; I < NexusCount; ++I) {
    const FString Prefix = FString::Printf(TEXT("Nexus%d_"), I);

    FFPMNexusDefinition Def;
    Def.ContinentId = I;
    Def.SafeRadius = GlobalSafeRadius;

    // Name
    FString Name;
    if (GConfig->GetString(TEXT("Nexus"), *(Prefix + TEXT("Name")), Name,
                           IniPath)) {
      Def.NexusName = Name;
    } else {
      Def.NexusName = FString::Printf(TEXT("Nexus %d"), I);
    }

    // World XY
    float WorldX = 0.f, WorldY = 0.f;
    GConfig->GetFloat(TEXT("Nexus"), *(Prefix + TEXT("WorldX")), WorldX,
                      IniPath);
    GConfig->GetFloat(TEXT("Nexus"), *(Prefix + TEXT("WorldY")), WorldY,
                      IniPath);
    Def.WorldCenter = FVector2D(WorldX, WorldY);

    // Per-nexus safe radius override
    float OverrideRadius = 0.f;
    if (GConfig->GetFloat(TEXT("Nexus"), *(Prefix + TEXT("SafeRadius")),
                          OverrideRadius, IniPath) &&
        OverrideRadius > 0.f) {
      Def.SafeRadius = OverrideRadius;
    }

    Nexuses.Add(Def);

    UE_LOG(LogFPMNexus, Log,
           TEXT("FPM Nexus[%d]: '%s' at (%.0f, %.0f) SafeRadius=%.0f cm"), I,
           *Def.NexusName, WorldX, WorldY, Def.SafeRadius);
  }

  bConfigLoaded = true;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

FVector AFPMNexusManager::GetNewCharacterSpawnPos(int32 WorldSeed) const {
  // Default to world origin if somehow no nexuses exist
  if (Nexuses.Num() == 0) {
    UE_LOG(LogFPMNexus, Warning,
           TEXT("FPM Nexus: No nexuses defined — falling back to origin."));
    const float FallbackZ =
        FPMVoxelGenerator::TerrainSurfaceZ(0.f, 0.f, WorldSeed);
    return FVector(0.f, 0.f, FallbackZ);
  }

  // Always spawn new characters at Nexus 0 (the starter island nexus)
  const FFPMNexusDefinition &StarterNexus = Nexuses[0];
  const float SurfaceZ = FPMVoxelGenerator::TerrainSurfaceZ(
      StarterNexus.WorldCenter.X, StarterNexus.WorldCenter.Y, WorldSeed);

  UE_LOG(LogFPMNexus, Log,
         TEXT("FPM Nexus: New character spawn at '%s' (%.0f, %.0f, %.0f)"),
         *StarterNexus.NexusName, StarterNexus.WorldCenter.X,
         StarterNexus.WorldCenter.Y, SurfaceZ);

  return FVector(StarterNexus.WorldCenter.X, StarterNexus.WorldCenter.Y,
                 SurfaceZ);
}

bool AFPMNexusManager::IsInNexusSafeZone(const FVector &WorldPos) const {
  return IsInNexusSafeZone(FVector2D(WorldPos.X, WorldPos.Y));
}

bool AFPMNexusManager::IsInNexusSafeZone(const FVector2D &WorldXY) const {
  for (const FFPMNexusDefinition &Def : Nexuses) {
    const float DistSq = FVector2D::DistSquared(WorldXY, Def.WorldCenter);
    const float RadiusSq = Def.SafeRadius * Def.SafeRadius;
    if (DistSq <= RadiusSq) {
      return true;
    }
  }
  return false;
}

float AFPMNexusManager::GetStarterNexusSafeRadius() const {
  if (Nexuses.Num() > 0)
    return Nexuses[0].SafeRadius;
  return 51200.f;
}
