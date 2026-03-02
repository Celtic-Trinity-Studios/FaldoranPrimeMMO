// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "UI/FPMHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/FPMPlayerCharacter.h"
#include "World/FPMChunkData.h"
#include "Engine/Texture2D.h"
#include "World/FPMNoise.h"
#include "World/FPMPlanetTraversal.h"
#include "World/FPMVoxelChunk.h"
#include "World/FPMWorldChunkManager.h"

// ===================================================================
//  Biome metadata
// ===================================================================
struct FBiomeMeta {
  EFPMBiome Biome;
  const TCHAR *Name;
  FLinearColor Color;
};

static const FBiomeMeta GBiomeMeta[] = {
    {EFPMBiome::Meadows, TEXT("Meadows"), {0.40f, 0.80f, 0.30f}},
    {EFPMBiome::Forest, TEXT("Forest"), {0.10f, 0.55f, 0.10f}},
    {EFPMBiome::Plains, TEXT("Plains"), {0.70f, 0.67f, 0.30f}},
    {EFPMBiome::Savanna, TEXT("Savanna"), {0.80f, 0.63f, 0.22f}},
    {EFPMBiome::Jungle, TEXT("Jungle"), {0.04f, 0.39f, 0.08f}},
    {EFPMBiome::Desert, TEXT("Desert"), {0.86f, 0.75f, 0.47f}},
    {EFPMBiome::Taiga, TEXT("Taiga"), {0.24f, 0.39f, 0.24f}},
    {EFPMBiome::BorealForest, TEXT("Boreal"), {0.16f, 0.31f, 0.20f}},
    {EFPMBiome::Tundra, TEXT("Tundra"), {0.59f, 0.63f, 0.67f}},
    {EFPMBiome::Swamp, TEXT("Swamp"), {0.31f, 0.39f, 0.16f}},
    {EFPMBiome::Alpine, TEXT("Alpine"), {0.55f, 0.55f, 0.47f}},
    {EFPMBiome::Mountain, TEXT("Mountain"), {0.47f, 0.43f, 0.39f}},
    {EFPMBiome::Snow, TEXT("Snow"), {0.88f, 0.92f, 1.00f}},
    {EFPMBiome::River, TEXT("River"), {0.24f, 0.55f, 0.78f}},
    {EFPMBiome::Coast, TEXT("Coast"), {0.35f, 0.67f, 0.82f}},
    {EFPMBiome::Beach, TEXT("Beach"), {0.90f, 0.82f, 0.63f}},
    {EFPMBiome::Ocean, TEXT("Ocean"), {0.12f, 0.31f, 0.47f}},
};
static constexpr int32 GBiomeCount = UE_ARRAY_COUNT(GBiomeMeta);

// ===================================================================
//  Helpers
// ===================================================================
static FString HUDYawToCompass(float Yaw) {
  while (Yaw < 0.f)
    Yaw += 360.f;
  while (Yaw >= 360.f)
    Yaw -= 360.f;
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
  return FString::Printf(TEXT("%s (%d\u00B0)"), *Dir, FMath::RoundToInt(Yaw));
}

static FString BiomeToString(EFPMBiome Biome) {
  for (const FBiomeMeta &M : GBiomeMeta)
    if (M.Biome == Biome)
      return M.Name;
  return TEXT("Unknown");
}

// ===================================================================
//  Constructor
// ===================================================================
AFPMHUD::AFPMHUD() {}

// ===================================================================
//  RefreshBiomeCache
// ===================================================================
void AFPMHUD::RefreshBiomeCache(int32 WorldSeed) {
  if (bCacheBuilt && CachedWorldSeed == WorldSeed)
    return;
  if (bCacheBuilding)
    return; // already in progress

  CachedWorldSeed = WorldSeed;
  bCacheBuilt = false;
  bCacheBuilding = true;
  BiomeLocations.Empty();

  // Pre-seed with bFound=false so buttons show biome colours (dimly)
  // while the background scan is still running.
  for (const FBiomeMeta &M : GBiomeMeta)
    BiomeLocations.Add(static_cast<uint8>(M.Biome), FBiomeLocation{});

  // Kick off scan on a background thread so we don't freeze the game thread.
  // Results arrive via StagingLocations + bStagingReady (polled in DrawHUD).
  Async(EAsyncExecution::ThreadPool, [this, WorldSeed]() {
    TMap<uint8, FBiomeLocation> Results;
    for (const FBiomeMeta &M : GBiomeMeta)
      Results.Add(static_cast<uint8>(M.Biome), FBiomeLocation{});

    // 50km steps, 3000km radius  →  120×120 = 14 400 samples
    // TerrainSurfaceZ is pure math (no UObject access), safe on worker thread
    // Earth-scale world (~12,800km diameter) needs a larger scan radius.
    constexpr float StepCm = 5000000.0f;
    constexpr float RadiusCm = 300000000.0f;
    constexpr float NormScale = 1.0f / 50000.0f;
    int32 Found = 0;

    for (float X = -RadiusCm; X <= RadiusCm && Found < GBiomeCount;
         X += StepCm) {
      for (float Y = -RadiusCm; Y <= RadiusCm; Y += StepCm) {
        const float SurfZ = FPMVoxelGenerator::TerrainSurfaceZ(X, Y, WorldSeed);
        const float NormH = FMath::Clamp(SurfZ * NormScale, 0.0f, 1.0f);
        EFPMBiome B = FPMVoxelGenerator::BiomeAtWorldXY(X, Y, WorldSeed, NormH);

        FBiomeLocation *Loc = Results.Find(static_cast<uint8>(B));
        if (Loc && !Loc->bFound) {
          Loc->WorldXY = FVector2D(X, Y);
          Loc->bFound = true;
          ++Found;
          if (Found >= GBiomeCount)
            break;
        }
      }
    }

    // Post results back — game thread picks them up next DrawHUD call
    StagingLocations = MoveTemp(Results);
    bStagingReady.store(true, std::memory_order_release);

    UE_LOG(LogTemp, Log,
           TEXT("FPM HUD: Biome cache built — %d / %d biomes found"), Found,
           GBiomeCount);
  });
}

// ===================================================================
//  TeleportToBiome — called directly when a button click is detected
// ===================================================================
void AFPMHUD::TeleportToBiome(uint8 BiomeKey) {
  const FBiomeLocation *Loc = BiomeLocations.Find(BiomeKey);
  if (!Loc || !Loc->bFound)
    return;

  APlayerController *PC = GetOwningPlayerController();
  if (!PC)
    return;
  APawn *Pawn = PC->GetPawn();
  if (!Pawn)
    return;

  const float SurfaceZ = FPMVoxelGenerator::TerrainSurfaceZ(
      Loc->WorldXY.X, Loc->WorldXY.Y, CachedWorldSeed);

  // Build a small safety floor (2km radius, 10-step grid = 100 samples) so
  // the player lands on something even before full chunks generate.
  // Do NOT call EnsureChunkLoadedAtWorldPos here — it blocks the game thread
  // for 2+ seconds and corrupts CMC state, making the teleport fail.
  for (TActorIterator<AFPMWorldChunkManager> It(GetWorld()); It; ++It) {
    const FVector DestXY(Loc->WorldXY.X, Loc->WorldXY.Y, SurfaceZ);
    It->BuildSafetyFloorAt(DestXY,
                           /*HalfExtentCm=*/200000.f, // 2 km half-extent
                           /*GridSteps=*/10, // 10×10 = 100 samples, <5 ms
                           /*SinkCm=*/500.f);
    break;
  }

  // Stop any pending movement so CMC doesn't fight the teleport
  if (UPawnMovementComponent *Move = Pawn->GetMovementComponent()) {
    Move->StopMovementImmediately();
  }

  // Teleport 15m above surface — gives chunks time to generate collision
  // TeleportTo (not SetActorLocation) properly notifies the CMC so it
  // doesn't snap back to the previous position on the next physics tick.
  const FVector Dest(Loc->WorldXY.X, Loc->WorldXY.Y, SurfaceZ + 1500.0f);
  const FRotator Rot = Pawn->GetActorRotation();
  const bool bOK =
      Pawn->TeleportTo(Dest, Rot, /*bIsATest=*/false, /*bNoCheck=*/true);

  const EFPMBiome BiomeEnum = static_cast<EFPMBiome>(BiomeKey);
  UE_LOG(LogTemp, Log,
         TEXT("FPM HUD: Teleport to %s @ (%.0f,%.0f,%.0f) success=%d"),
         *BiomeToString(BiomeEnum), Dest.X, Dest.Y, Dest.Z, bOK ? 1 : 0);
}

// ===================================================================
//  DrawHUD
// ===================================================================
void AFPMHUD::DrawHUD() {
  Super::DrawHUD();
  if (!Canvas)
    return;

  APlayerController *PC = GetOwningPlayerController();
  if (!PC)
    return;
  AFPMPlayerCharacter *Char = Cast<AFPMPlayerCharacter>(PC->GetPawn());
  if (!Char)
    return;

  // World map overlay takes full screen -- draw it and return early
  if (bWorldMapOpen) {
    DrawWorldMapOverlay(PC, Char);
    return;
  }

  const FVector Pos = Char->GetActorLocation();
  const float Yaw = PC->GetControlRotation().Yaw;
  const float DeltaTime = GetWorld()->GetDeltaSeconds();
  const FFPMChunkCoord Chunk = FPMChunkGenerator::WorldToChunkCoord(Pos);

  // FPS
  const float FPS = (DeltaTime > 0.f) ? (1.f / DeltaTime) : 0.f;
  const FLinearColor FPSColor = (FPS >= 60.f) ? FLinearColor(0.39f, 1.0f, 0.39f)
                                : (FPS >= 30.f)
                                    ? FLinearColor(1.0f, 0.9f, 0.31f)
                                    : FLinearColor(1.0f, 0.31f, 0.31f);

  // Velocity
  const FVector Vel = Char->GetVelocity();
  const float HorizCms = Vel.Size2D();
  const float VertCms = Vel.Z;
  const float HorizMs = HorizCms / 100.f;
  const float HorizKmh = HorizMs * 3.6f;
  FLinearColor VelColor = Char->IsFlying() ? FLinearColor(0.39f, 0.90f, 1.0f)
                          : HorizCms >= 250.f
                              ? FLinearColor(1.0f, 0.85f, 0.2f)
                              : FLinearColor(0.85f, 0.85f, 0.85f);

  // World seed
  int32 WorldSeed = 42;
  const AFPMWorldChunkManager *ChunkMgr = nullptr;
  for (TActorIterator<AFPMWorldChunkManager> It(GetWorld()); It; ++It) {
    WorldSeed = It->WorldSeed;
    ChunkMgr = *It;
    break;
  }
  RefreshBiomeCache(WorldSeed);

  // Pick up results from the background cache scan if ready
  if (bStagingReady.load(std::memory_order_acquire)) {
    BiomeLocations = MoveTemp(StagingLocations);
    StagingLocations.Empty();
    bCacheBuilding = false;
    bCacheBuilt = true;
    bStagingReady.store(false, std::memory_order_release);
  }

  // Biome
  const float NormH = FMath::Clamp(Pos.Z / 50000.f, 0.f, 1.f);
  const EFPMBiome Biome =
      FPMVoxelGenerator::BiomeAtWorldXY(Pos.X, Pos.Y, WorldSeed, NormH);

  // GPS
  const float Latitude = 45.f + Pos.X * (1.f / 11100000.f);
  const float Longitude = -30.f + Pos.Y * (1.f / 11100000.f);
  const TCHAR LatDir = Latitude >= 0.f ? TEXT('N') : TEXT('S');
  const TCHAR LonDir = Longitude >= 0.f ? TEXT('E') : TEXT('W');
  const float AltFeet = Pos.Z / 30.48f;

  // Spring
  FString SpringStr;
  if (ChunkMgr) {
    FVector SP;
    float SD;
    if (ChunkMgr->GetNearestRiverHead(Pos, SP, SD)) {
      const FVector D2 = (SP - Pos).GetSafeNormal2D();
      float A = FMath::RadiansToDegrees(FMath::Atan2(D2.Y, D2.X));
      while (A < 0.f)
        A += 360.f;
      float R = A - Yaw;
      while (R < 0.f)
        R += 360.f;
      while (R >= 360.f)
        R -= 360.f;
      const TCHAR *Arrow = (R >= 337.5f || R < 22.5f) ? TEXT("\u2191")
                           : R < 67.5f                ? TEXT("\u2197")
                           : R < 112.5f               ? TEXT("\u2192")
                           : R < 157.5f               ? TEXT("\u2198")
                           : R < 202.5f               ? TEXT("\u2193")
                           : R < 247.5f               ? TEXT("\u2199")
                           : R < 292.5f               ? TEXT("\u2190")
                                                      : TEXT("\u2196");
      SpringStr = FString::Printf(TEXT("%s Spring: %.0fm"), Arrow, SD / 100.f);
    }
  }

  // HUD mouse mode indicator
  const bool bCursorOn = Char->bHUDMouseMode;

  // ---------------------------------------------------------------
  //  Debug info panel
  // ---------------------------------------------------------------
  struct FHUDLine {
    FString Text;
    FLinearColor Color;
  };
  TArray<FHUDLine> Lines;
  Lines.Add({FString::Printf(TEXT(" %s"), *HUDYawToCompass(Yaw)),
             {0.78f, 0.86f, 1.0f}});
  Lines.Add({FString::Printf(TEXT(" Biome: %s"), *BiomeToString(Biome)),
             {0.63f, 0.9f, 0.63f}});
  Lines.Add({FString::Printf(TEXT(" %.4f %c  %.4f %c"), FMath::Abs(Latitude),
                             LatDir, FMath::Abs(Longitude), LonDir),
             {0.7f, 0.78f, 0.7f}});
  Lines.Add({FString::Printf(TEXT(" Chunk (%d, %d)"), Chunk.Q, Chunk.R),
             {0.6f, 0.65f, 0.7f}});
  Lines.Add(
      {FString::Printf(TEXT(" Alt: %.0f ft"), AltFeet), {0.67f, 0.78f, 0.9f}});
  Lines.Add({FString::Printf(TEXT(" %.0f FPS"), FPS), FPSColor});

  FString VelStr =
      (Char->IsFlying() || FMath::Abs(VertCms) > 50.f)
          ? FString::Printf(TEXT(" %.1f m/s  (V %+.1f m/s)  %.0f km/h"),
                            HorizMs, VertCms / 100.f, HorizKmh)
          : FString::Printf(TEXT(" %.1f m/s  %.0f km/h"), HorizMs, HorizKmh);
  Lines.Add({VelStr, VelColor});
  if (!SpringStr.IsEmpty())
    Lines.Add({SpringStr, {0.39f, 0.71f, 1.0f}});

  // Tab hint
  Lines.Add(
      {bCursorOn ? TEXT(" [Tab] Lock camera") : TEXT(" [Tab] Biome teleport"),
       {0.5f, 0.5f, 0.5f}});

  UFont *Font = GEngine->GetSmallFont();
  const float FS = 1.15f;
  const float LineH = 18.f * FS;
  const float PadX = 12.f;
  const float PadY = 8.f;
  const float PX = 12.f;
  const float PY = 12.f;

  float MaxW = 0.f;
  for (const FHUDLine &L : Lines) {
    float TW, TH;
    GetTextSize(L.Text, TW, TH, Font, FS);
    MaxW = FMath::Max(MaxW, TW);
  }
  const float PW = MaxW + PadX * 2.f;
  const float PH = Lines.Num() * LineH + PadY * 2.f;

  DrawRect({0.15f, 0.17f, 0.22f, 0.85f}, PX - 1.f, PY - 1.f, PW + 2.f,
           PH + 2.f);
  DrawRect({0.05f, 0.06f, 0.09f, 0.80f}, PX, PY, PW, PH);
  float CurY = PY + PadY;
  for (const FHUDLine &L : Lines) {
    DrawText(L.Text, L.Color, PX + PadX, CurY, Font, FS);
    CurY += LineH;
  }

  // ---------------------------------------------------------------
  //  Biome teleport panel — always drawn, buttons only clickable
  //  when cursor is visible (Tab mode)
  // ---------------------------------------------------------------
  DrawBiomeTeleportPanel(PC, Char, PY + PH + 8.f);

  // Rift Runner panel — right side of screen
  DrawRiftRunnerPanel(PC, Char);
}

// ===================================================================
//  DrawBiomeTeleportPanel
//  Uses direct mouse polling — no AddHitBox callbacks needed.
// ===================================================================
void AFPMHUD::DrawBiomeTeleportPanel(APlayerController *PC,
                                     AFPMPlayerCharacter *Char, float TopY) {
  if (!Canvas)
    return;

  const bool bCursorOn = Char->bHUDMouseMode;

  UFont *Font = GEngine->GetSmallFont();

  constexpr float BtnW = 90.f;
  constexpr float BtnH = 20.f;
  constexpr float GapX = 4.f;
  constexpr float GapY = 3.f;
  constexpr float Pad = 6.f;
  constexpr float Cols = 2.f;
  constexpr float Rows = (GBiomeCount + 1) / 2;

  const float PW = Cols * BtnW + (Cols - 1) * GapX + Pad * 2.f;
  const float PH =
      Rows * (BtnH + GapY) - GapY + Pad * 2.f + 16.f; // +16 for title
  const float PX = 12.f;
  const float PY = TopY;

  // Panel background
  DrawRect({0.10f, 0.12f, 0.16f, 0.88f}, PX - 1.f, PY - 1.f, PW + 2.f,
           PH + 2.f);
  DrawRect({0.04f, 0.05f, 0.08f, 0.82f}, PX, PY, PW, PH);

  // Title
  const FString Title = bCursorOn ? TEXT("BIOME TELEPORT  [click]")
                                  : TEXT("BIOME TELEPORT  [Tab]");
  DrawText(Title, {0.6f, 0.7f, 0.9f}, PX + Pad, PY + 3.f, Font, 0.85f);

  const float StartY = PY + Pad + 16.f;

  // Get mouse position for hover + click detection
  float MX = -1.f, MY = -1.f;
  if (bCursorOn)
    PC->GetMousePosition(MX, MY);

  // Debounce left click (static, persists across frames)
  static bool bWasClicking = false;
  const bool bIsClicking =
      bCursorOn && PC->IsInputKeyDown(EKeys::LeftMouseButton);
  const bool bJustClicked = bIsClicking && !bWasClicking;
  bWasClicking = bIsClicking;

  for (int32 I = 0; I < GBiomeCount; ++I) {
    const FBiomeMeta &Meta = GBiomeMeta[I];
    const int32 Col = I % 2;
    const int32 Row = I / 2;
    const float BX = PX + Pad + Col * (BtnW + GapX);
    const float BY = StartY + Row * (BtnH + GapY);

    // Check if this biome has a cached location
    const FBiomeLocation *Loc =
        BiomeLocations.Find(static_cast<uint8>(Meta.Biome));
    const bool bAvail = Loc && Loc->bFound;
    const bool bSearching = bCacheBuilding && !bAvail;

    // Hover detection (only when available)
    const bool bHover = bCursorOn && bAvail && MX >= BX && MX <= BX + BtnW &&
                        MY >= BY && MY <= BY + BtnH;

    // Click detection
    if (bJustClicked && bHover) {
      TeleportToBiome(static_cast<uint8>(Meta.Biome));
    }

    // Button colours
    //   bAvail   = found   → bright biome tint
    //   bSearching = not yet → dim biome tint with searching indicator
    //   neither  = not on this seed → very dim grey
    const float BrightMult = bHover ? 1.6f : 1.0f;
    const float FillScale = bAvail ? 0.22f * BrightMult : 0.08f;
    const float BorderScale = bAvail ? 0.55f * BrightMult : 0.20f;
    const float TextBright = bAvail ? 1.8f * BrightMult : 0.60f;
    const float TextOffset = bAvail ? 0.15f : 0.05f;
    const FLinearColor C = Meta.Color;

    const FLinearColor BtnBg =
        FLinearColor(C.R * FillScale, C.G * FillScale, C.B * FillScale, 0.92f);
    const FLinearColor Border = FLinearColor(
        C.R * BorderScale, C.G * BorderScale, C.B * BorderScale, 1.0f);
    const FLinearColor TxtCol =
        FLinearColor(FMath::Min(C.R * TextBright + TextOffset, 1.f),
                     FMath::Min(C.G * TextBright + TextOffset, 1.f),
                     FMath::Min(C.B * TextBright + TextOffset, 1.f), 1.0f);

    DrawRect(Border, BX - 1.f, BY - 1.f, BtnW + 2.f, BtnH + 2.f);
    DrawRect(BtnBg, BX, BY, BtnW, BtnH);

    // Label — append '...' while searching
    const FString Label = bSearching
                              ? FString::Printf(TEXT("%s ..."), Meta.Name)
                              : FString(Meta.Name);
    float TW, TH;
    GetTextSize(Label, TW, TH, Font, 1.0f);
    DrawText(Label, TxtCol, BX + (BtnW - TW) * 0.5f, BY + (BtnH - TH) * 0.5f,
             Font, 1.0f);
  }
}

// ===================================================================
//  DrawHUDLine helper
// ===================================================================
float AFPMHUD::DrawHUDLine(float X, float Y, const FString &Text,
                           const FLinearColor &Color, UFont *Font,
                           float Scale) const {
  float TW, TH;
  GetTextSize(Text, TW, TH, Font, Scale);
  const_cast<AFPMHUD *>(this)->DrawText(Text, Color, X, Y, Font, Scale);
  return TH + 2.f;
}

// ===================================================================
//  DrawRiftRunnerPanel
//  Right side of screen, mirroring the debug panel aesthetic.
//  Shows Rift Runner status, speed tier, distance, and keybind hints.
// ===================================================================
void AFPMHUD::DrawRiftRunnerPanel(APlayerController *PC,
                                  AFPMPlayerCharacter *Char) {
  if (!Canvas || !Char)
    return;

  // Find the PlanetTraversal component
  UFPMPlanetTraversal *Rift = Char->FindComponentByClass<UFPMPlanetTraversal>();

  UFont *Font = GEngine->GetSmallFont();
  const float FS = 1.15f;
  const float LineH = 18.f * FS;
  const float PadX = 12.f;
  const float PadY = 8.f;
  const float PY = 12.f;

  const bool bActive = Rift && Rift->IsRiftRunnerActive();

  // --- Build lines ---
  struct FRiftLine {
    FString Text;
    FLinearColor Color;
  };
  TArray<FRiftLine> Lines;

  // Title / status
  if (bActive) {
    Lines.Add({TEXT(" ⚡ RIFT RUNNER  ACTIVE"), {0.29f, 0.98f, 0.86f}});
  } else {
    Lines.Add({TEXT(" ○ Rift Runner  off"), {0.45f, 0.55f, 0.65f}});
  }

  // Speed tier
  if (Rift) {
    // Derive speed by reading IsRiftRunnerActive state — we expose tier info
    // via the on-screen message system, so just show the keybind hint here.
    const FLinearColor SpeedCol = bActive ? FLinearColor(0.20f, 0.85f, 1.0f)
                                          : FLinearColor(0.4f, 0.5f, 0.55f);
    Lines.Add({TEXT(" [G]  Toggle on / off"), SpeedCol});
    Lines.Add({TEXT(" [H]  Cycle speed tier"), SpeedCol});
  } else {
    Lines.Add({TEXT(" Component not found!"), {1.0f, 0.3f, 0.3f}});
  }

  // Separator
  Lines.Add({TEXT(" ───────────────"), {0.25f, 0.30f, 0.38f}});

  // Speed tier labels
  const FLinearColor DimCol = bActive ? FLinearColor(0.65f, 0.80f, 0.90f)
                                      : FLinearColor(0.30f, 0.38f, 0.45f);
  Lines.Add({TEXT(" Tier 1 =    500 km/h"), DimCol});
  Lines.Add({TEXT(" Tier 2 =  5,000 km/h"), DimCol});
  Lines.Add({TEXT(" Tier 3 = 50,000 km/h"), DimCol});

  // --- Size & position (right-anchored) ---
  float MaxW = 0.f;
  for (const FRiftLine &L : Lines) {
    float TW, TH;
    GetTextSize(L.Text, TW, TH, Font, FS);
    MaxW = FMath::Max(MaxW, TW);
  }
  const float PW = MaxW + PadX * 2.f;
  const float PH = Lines.Num() * LineH + PadY * 2.f;
  const float PX = Canvas->SizeX - PW - 12.f; // right-anchored, 12px margin

  // Background — cyan tint when active, dark slate when off
  const FLinearColor BgOuter = bActive
                                   ? FLinearColor(0.08f, 0.22f, 0.22f, 0.88f)
                                   : FLinearColor(0.15f, 0.17f, 0.22f, 0.85f);
  const FLinearColor BgInner = bActive
                                   ? FLinearColor(0.03f, 0.10f, 0.12f, 0.82f)
                                   : FLinearColor(0.05f, 0.06f, 0.09f, 0.80f);

  DrawRect(BgOuter, PX - 1.f, PY - 1.f, PW + 2.f, PH + 2.f);
  DrawRect(BgInner, PX, PY, PW, PH);

  // Active glow border
  if (bActive) {
    DrawRect({0.20f, 0.85f, 0.82f, 0.55f}, PX - 2.f, PY - 2.f, PW + 4.f,
             2.f); // top
    DrawRect({0.20f, 0.85f, 0.82f, 0.55f}, PX - 2.f, PY + PH, PW + 4.f,
             2.f); // bottom
    DrawRect({0.20f, 0.85f, 0.82f, 0.55f}, PX - 2.f, PY - 2.f, 2.f,
             PH + 4.f); // left
    DrawRect({0.20f, 0.85f, 0.82f, 0.55f}, PX + PW, PY - 2.f, 2.f,
             PH + 4.f); // right
  }

  float CurY = PY + PadY;
  for (const FRiftLine &L : Lines) {
    DrawText(L.Text, L.Color, PX + PadX, CurY, Font, FS);
    CurY += LineH;
  }
}

// ===================================================================
//  World Map — texture generation + overlay
// ===================================================================

static constexpr int32 GMapRes = 1024; // pixels per axis

// ---------------------------------------------------------------------------
//  ToggleWorldMap — called by PlayerController M key
// ---------------------------------------------------------------------------
void AFPMHUD::ToggleWorldMap(int32 WorldSeed) {
  bWorldMapOpen = !bWorldMapOpen;

  if (bWorldMapOpen && !bWorldMapReady && !bWorldMapPending) {
    BeginWorldMapGeneration(WorldSeed);
  }
}

// ---------------------------------------------------------------------------
//  BeginWorldMapGeneration — fires async pixel-fill on a worker thread
// ---------------------------------------------------------------------------
void AFPMHUD::BeginWorldMapGeneration(int32 WorldSeed) {
  bWorldMapPending = true;
  WorldMapSeed     = WorldSeed;
  bMapStagingReady.store(false, std::memory_order_relaxed);

  // Allocate staging buffer
  StagingMapPixels.SetNumUninitialized(GMapRes * GMapRes);
  TArray<FColor>* Staging = &StagingMapPixels;

  const float CircumCm = FPMChunkConstants::PlanetCircumferenceCm;
  const float Step     = CircumCm / static_cast<float>(GMapRes);
  const float OriginX  = -CircumCm * 0.5f;
  const float OriginY  = -CircumCm * 0.5f;

  Async(EAsyncExecution::ThreadPool,
        [this, WorldSeed, Step, OriginX, OriginY, CircumCm]() {
    // Pure math — safe on any thread
    for (int32 Row = 0; Row < GMapRes; ++Row) {
      for (int32 Col = 0; Col < GMapRes; ++Col) {
        const float WX = FPMChunkConstants::WrapWorldCoord(OriginX + Col * Step);
        const float WY = FPMChunkConstants::WrapWorldCoord(OriginY + Row * Step);

        const float SurfZ = FPMNoise::TerrainSurfaceZ(WX, WY, WorldSeed);
        const float NormH = FMath::Clamp(SurfZ / 2000000.f, 0.f, 1.f);
        const EFPMBiome Biome =
            FPMVoxelGenerator::BiomeAtWorldXY(WX, WY, WorldSeed, NormH);
        FColor C = FPMVoxelGenerator::BiomeToVertexColor(Biome);

        // Ocean / below sea level: deep blue gradient
        if (SurfZ <= 0.f) {
          const float D = FMath::Clamp(-SurfZ / 1100000.f, 0.f, 1.f);
          C = FColor(FMath::Lerp(C.R, uint8(15),  D),
                     FMath::Lerp(C.G, uint8(55),  D),
                     FMath::Lerp(C.B, uint8(120), D), 255);
        }
        // Mountain / snow cap brightening
        else if (SurfZ > 500000.f) {
          const float P = FMath::Clamp((SurfZ - 500000.f) / 800000.f, 0.f, 1.f);
          C = FColor(FMath::Clamp((int32)FMath::Lerp((float)C.R, 240.f, P), 0, 255),
                     FMath::Clamp((int32)FMath::Lerp((float)C.G, 240.f, P), 0, 255),
                     FMath::Clamp((int32)FMath::Lerp((float)C.B, 255.f, P), 0, 255), 255);
        }

        StagingMapPixels[Row * GMapRes + Col] = C;
      }
    }

    bMapStagingReady.store(true, std::memory_order_release);
  });
}

// ---------------------------------------------------------------------------
//  DrawWorldMapOverlay — called from DrawHUD when bWorldMapOpen
// ---------------------------------------------------------------------------
void AFPMHUD::DrawWorldMapOverlay(APlayerController *PC,
                                  AFPMPlayerCharacter *Char) {
  if (!Canvas) return;

  // --- Promote staging buffer to GPU texture once ready ---
  if (bMapStagingReady.load(std::memory_order_acquire) && bWorldMapPending) {
    bWorldMapPending = false;
    bWorldMapReady   = true;

    // Create or re-create the texture
    if (!WorldMapTexture
     || WorldMapTexture->GetSizeX() != GMapRes
     || WorldMapTexture->GetSizeY() != GMapRes) {
      WorldMapTexture = UTexture2D::CreateTransient(GMapRes, GMapRes, PF_B8G8R8A8);
      WorldMapTexture->Filter = TF_Bilinear;
      WorldMapTexture->SRGB   = 0;
    }

    if (FTexture2DMipMap* Mip = &WorldMapTexture->GetPlatformData()->Mips[0]) {
      void* Data = Mip->BulkData.Lock(LOCK_READ_WRITE);
      // Convert FColor (RGBA) to PF_B8G8R8A8 (BGRA) layout
      TArray<FColor>& Pixels = StagingMapPixels;
      uint8* Dst = static_cast<uint8*>(Data);
      for (int32 I = 0; I < GMapRes * GMapRes; ++I) {
        Dst[I*4+0] = Pixels[I].B;
        Dst[I*4+1] = Pixels[I].G;
        Dst[I*4+2] = Pixels[I].R;
        Dst[I*4+3] = 255;
      }
      Mip->BulkData.Unlock();
    }
    WorldMapTexture->UpdateResource();
  }

  // --- Dim the world behind the map ---
  DrawRect({0.f, 0.f, 0.f, 0.72f}, 0, 0, Canvas->SizeX, Canvas->SizeY);

  // --- Map panel dimensions ---
  const float MapSize = FMath::Min(Canvas->SizeX, Canvas->SizeY) * 0.85f;
  const float MapX    = (Canvas->SizeX - MapSize) * 0.5f;
  const float MapY    = (Canvas->SizeY - MapSize) * 0.5f;

  // Border
  DrawRect({0.25f, 0.35f, 0.55f, 0.90f}, MapX - 3, MapY - 3,
           MapSize + 6, MapSize + 6);
  DrawRect({0.08f, 0.10f, 0.14f, 0.95f}, MapX, MapY, MapSize, MapSize);

  // Texture
  if (bWorldMapReady && WorldMapTexture) {
    FCanvasTileItem Tile(
        FVector2D(MapX, MapY),
        WorldMapTexture->GetResource(),
        FVector2D(MapSize, MapSize),
        FLinearColor::White);
    Tile.BlendMode = SE_BLEND_Opaque;
    Canvas->DrawItem(Tile);
  } else {
    // Still generating — draw spinner text
    UFont* Font = GEngine->GetMediumFont();
    const FString Msg = bWorldMapPending
        ? TEXT("Generating planet map...")
        : TEXT("Press M to open World Map");
    float TW, TH;
    GetTextSize(Msg, TW, TH, Font, 1.2f);
    DrawText(Msg, {0.6f, 0.78f, 1.0f}, MapX + (MapSize-TW)*0.5f,
             MapY + (MapSize-TH)*0.5f, Font, 1.2f);
  }

  // --- Player dot ---
  if (Char) {
    const float CircumCm = FPMChunkConstants::PlanetCircumferenceCm;
    const FVector Pos     = Char->GetActorLocation();
    const float NormX     = FMath::Fmod(Pos.X / CircumCm + 1.5f, 1.f);
    const float NormY     = FMath::Fmod(Pos.Y / CircumCm + 1.5f, 1.f);
    const float DotX      = MapX + NormX * MapSize;
    const float DotY      = MapY + NormY * MapSize;
    DrawRect({1.f, 0.2f, 0.2f, 1.f},  DotX - 5, DotY - 5, 10, 10);
    DrawRect({1.f, 1.f, 1.f, 1.f},    DotX - 2, DotY - 2,  4,  4);
  }

  // --- Legend & keybind hint ---
  UFont* Font = GEngine->GetSmallFont();
  const float HintY = MapY + MapSize + 10.f;
  DrawText(TEXT("[M] Close map     Red dot = You"),
           {0.55f, 0.65f, 0.80f},
           MapX, HintY, Font, 1.0f);
}
