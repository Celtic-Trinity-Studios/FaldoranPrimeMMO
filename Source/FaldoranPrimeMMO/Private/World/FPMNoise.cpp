// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

#include "World/FPMNoise.h"
#include "World/FPMChunkData.h"

// ===================================================================
//  2D Simplex Gradient Noise
// ===================================================================

// 8 unit-length gradient directions
static const float Grad2[8][2] = {{1, 0},
                                  {-1, 0},
                                  {0, 1},
                                  {0, -1},
                                  {0.7071f, 0.7071f},
                                  {-0.7071f, 0.7071f},
                                  {0.7071f, -0.7071f},
                                  {-0.7071f, -0.7071f}};

static const float F2 = 0.36602540378f; // (sqrt(3)-1)/2
static const float G2 = 0.21132486540f; // (3-sqrt(3))/6

int32 FPMNoise::GradHash(int32 X, int32 Y, int32 Seed) {
  int32 H = X * 374761393 + Y * 668265263 + Seed * 1274126177;
  H = (H ^ (H >> 13)) * 1103515245;
  return FMath::Abs((H ^ (H >> 16))) & 7;
}

float FPMNoise::GradDot(int32 Hash, float X, float Y) {
  return Grad2[Hash & 7][0] * X + Grad2[Hash & 7][1] * Y;
}

float FPMNoise::CellHash(int32 X, int32 Y, int32 Seed) {
  int32 H = X * 374761393 + Y * 668265263 + Seed * 1274126177;
  H = (H ^ (H >> 13)) * 1103515245;
  H = H ^ (H >> 16);
  return FMath::Abs(static_cast<float>(H) / 2147483647.0f);
}

float FPMNoise::Simplex2D(float X, float Y, int32 Seed) {
  // Skew to simplex grid
  const float S = (X + Y) * F2;
  const int32 I = FMath::FloorToInt(X + S);
  const int32 J = FMath::FloorToInt(Y + S);

  // Unskew back
  const float T = static_cast<float>(I + J) * G2;
  const float x0 = X - (static_cast<float>(I) - T);
  const float y0 = Y - (static_cast<float>(J) - T);

  // Which simplex triangle
  const int32 i1 = (x0 > y0) ? 1 : 0;
  const int32 j1 = (x0 > y0) ? 0 : 1;

  const float x1 = x0 - static_cast<float>(i1) + G2;
  const float y1 = y0 - static_cast<float>(j1) + G2;
  const float x2 = x0 - 1.0f + 2.0f * G2;
  const float y2 = y0 - 1.0f + 2.0f * G2;

  // Contributions from three corners
  float n0 = 0, n1 = 0, n2 = 0;

  float t0 = 0.5f - x0 * x0 - y0 * y0;
  if (t0 > 0) {
    t0 *= t0;
    n0 = t0 * t0 * GradDot(GradHash(I, J, Seed), x0, y0);
  }
  float t1 = 0.5f - x1 * x1 - y1 * y1;
  if (t1 > 0) {
    t1 *= t1;
    n1 = t1 * t1 * GradDot(GradHash(I + i1, J + j1, Seed), x1, y1);
  }
  float t2 = 0.5f - x2 * x2 - y2 * y2;
  if (t2 > 0) {
    t2 *= t2;
    n2 = t2 * t2 * GradDot(GradHash(I + 1, J + 1, Seed), x2, y2);
  }

  return 70.0f * (n0 + n1 + n2); // range [-1, 1]
}

// ===================================================================
//  Fractal Brownian Motion
// ===================================================================

float FPMNoise::FBM(float X, float Y, int32 Seed, int32 Octaves, float Gain,
                    float Lacunarity) {
  float Sum = 0, Amp = 1, TotalAmp = 0, Freq = 1;
  for (int32 i = 0; i < Octaves; ++i) {
    Sum += Simplex2D(X * Freq, Y * Freq, Seed + i * 31337) * Amp;
    TotalAmp += Amp;
    Amp *= Gain;
    Freq *= Lacunarity;
  }
  return (Sum / TotalAmp + 1.0f) * 0.5f; // normalize to [0, 1]
}

float FPMNoise::RidgeFBM(float X, float Y, int32 Seed, int32 Octaves,
                         float Gain, float Lacunarity) {
  float Sum = 0, Amp = 1, TotalAmp = 0, Freq = 1, Prev = 1;
  for (int32 i = 0; i < Octaves; ++i) {
    float V = Simplex2D(X * Freq, Y * Freq, Seed + i * 31337);
    V = 1.0f - FMath::Abs(V); // ridge fold
    V = V * V;                // sharpen
    V *= Prev;                // weight by previous (cascade sharpening)
    Prev = V;
    Sum += V * Amp;
    TotalAmp += Amp;
    Amp *= Gain;
    Freq *= Lacunarity;
  }
  return Sum / TotalAmp;
}

// ===================================================================
//  Domain Warping
// ===================================================================

void FPMNoise::DomainWarp(float &X, float &Y, int32 Seed, float Strength,
                          float Freq) {
  const float WX = Simplex2D(X * Freq, Y * Freq, Seed + 50000);
  const float WY = Simplex2D(X * Freq + 17.1f, Y * Freq + 31.7f, Seed + 60000);
  X += WX * Strength;
  Y += WY * Strength;
}

// ===================================================================
//  Voronoi 2D Cellular Noise
// ===================================================================

void FPMNoise::Voronoi2D(float X, float Y, int32 Seed, float &OutMinDist,
                         float &OutCellHash, float &OutEdgeDist) {
  const int32 CellX = FMath::FloorToInt(X);
  const int32 CellY = FMath::FloorToInt(Y);
  const float FracX = X - static_cast<float>(CellX);
  const float FracY = Y - static_cast<float>(CellY);

  float MinDist1 = 99.0f; // Distance to closest cell point
  float MinDist2 = 99.0f; // Distance to second-closest
  float BestHash = 0.0f;

  // Search 3×3 neighborhood of cells
  for (int32 DY = -1; DY <= 1; ++DY) {
    for (int32 DX = -1; DX <= 1; ++DX) {
      const int32 NX = CellX + DX;
      const int32 NY = CellY + DY;

      // Random point position within this cell (0-1 range)
      const float PX = static_cast<float>(DX) + CellHash(NX, NY, Seed + 7777);
      const float PY = static_cast<float>(DY) + CellHash(NX, NY, Seed + 8888);

      const float DistX = PX - FracX;
      const float DistY = PY - FracY;
      const float Dist = FMath::Sqrt(DistX * DistX + DistY * DistY);

      if (Dist < MinDist1) {
        MinDist2 = MinDist1;
        MinDist1 = Dist;
        BestHash = CellHash(NX, NY, Seed + 9999);
      } else if (Dist < MinDist2) {
        MinDist2 = Dist;
      }
    }
  }

  OutMinDist = MinDist1;
  OutCellHash = BestHash;
  // Edge distance: 0 at boundary between two cells, ~1 at cell center
  OutEdgeDist = FMath::Clamp((MinDist2 - MinDist1) / 0.5f, 0.0f, 1.0f);
}

// ===================================================================
//  Macro Biome Region Map
//
//  Very low-frequency Voronoi cells (~200-400km per region).
//  Each cell produces climate biases that shift Temperature and
//  Moisture within the region, creating continent-scale coherence.
//  The edge distance provides hysteresis at region boundaries.
// ===================================================================

void FPMNoise::BiomeRegion(float WorldX, float WorldY, int32 Seed,
                           float &OutTempBias, float &OutMoistBias,
                           float &OutEdgeBlend) {
  // Scale world coords to Voronoi space.
  // Cell size ~ 300km (30,000,000 cm) → each Voronoi cell spans one
  // major climate zone (like "the desert region" or "the forest belt").
  constexpr float RegionScale = 1.0f / 30000000.0f; // ~300km cells

  // Domain warp for organic region boundaries (not grid-aligned)
  float WX = WorldX, WY = WorldY;
  DomainWarp(WX, WY, Seed + 70000, 12000000.0f, 0.000000025f);

  const float VX = WX * RegionScale;
  const float VY = WY * RegionScale;

  float MinDist, RegionHash, EdgeDist;
  Voronoi2D(VX, VY, Seed + 77777, MinDist, RegionHash, EdgeDist);

  // Per-region climate offsets derived from the cell hash.
  // Each region gets a stable, unique temperature and moisture bias.
  // Range: ±0.20 — enough to push a "warm" region fully into "hot".
  //
  // Use two different hash-based values for independent T/M variation.
  // The hash is in [0,1], so (hash - 0.5) * 0.40 gives ±0.20.
  const int32 RegionSeedT =
      static_cast<int32>(RegionHash * 2147483647.0f) ^ (Seed + 11111);
  const int32 RegionSeedM =
      static_cast<int32>(RegionHash * 2147483647.0f) ^ (Seed + 22222);

  // Use the hash bits to generate two independent bias values
  float TBias =
      CellHash(RegionSeedT & 0xFFFF, (RegionSeedT >> 16) & 0xFFFF, Seed);
  float MBias =
      CellHash(RegionSeedM & 0xFFFF, (RegionSeedM >> 16) & 0xFFFF, Seed);

  OutTempBias = (TBias - 0.5f) * 0.40f;  // ±0.20
  OutMoistBias = (MBias - 0.5f) * 0.40f; // ±0.20

  // Smooth the edge blend with a quintic curve for organic transitions
  float ESmooth = EdgeDist;
  ESmooth = ESmooth * ESmooth * ESmooth *
            (ESmooth * (ESmooth * 6.0f - 15.0f) + 10.0f);
  OutEdgeBlend = ESmooth;
}

// ===================================================================
//  Island Mask
// ===================================================================

float FPMNoise::IslandMask(float WorldX, float WorldY, int32 Seed) {
  // Planet mode: no island edges. The entire surface generates terrain.
  // Ocean biomes exist where continental noise puts terrain below sea level.
  return 1.0f;
}

// ===================================================================
//  Continuous Terrain Height
// ===================================================================

float FPMNoise::TerrainHeight(float WorldX, float WorldY, int32 Seed) {
  // ================================================================
  //  WORLD-SIMULATION SCALE TERRAIN
  //
  //  Target: 1024km island, 20km vertical range (-11km to +9km)
  //  Sea level sits at normalized 0.55 (which maps to Z=0).
  //
  //  Real-world reference:
  //    - Continents: features span 200-500km
  //    - Mountain ranges: 50-100km wide
  //    - Hills/valleys: 5-20km features
  //    - Surface detail: 1-5km texture
  //
  //  Noise frequencies (in 1/cm):
  //    Continental: 0.00000002 → wavelength ~500km
  //    Mountains:   0.0000001  → wavelength ~100km
  //    Hills:       0.0000005  → wavelength ~20km
  //    Detail:      0.000002   → wavelength ~5km
  // ================================================================

  // --- Domain warp for organic macro shapes ---
  // Two passes: large-scale (300km) + medium-scale (50km) for organic contours
  float WX = WorldX, WY = WorldY;
  DomainWarp(WX, WY, Seed, 15000000.0f, 0.00000003f);      // 300km warp
  DomainWarp(WX, WY, Seed + 9999, 3000000.0f, 0.0000002f); // 50km warp

  // --- LAYER 1: Continental base ---
  // Very low freq: broad highlands and lowlands spanning 300-500km.
  // Linear (not squared) for meaningful lowland-to-highland variation.
  float Continental =
      FBM(WX * 0.00000002f, WY * 0.00000002f, Seed + 1000, 5, 0.50f, 2.0f);

  // --- LAYER 2: Medium-scale terrain ---
  // Bridges the gap between continental (500km) and hills (20km).
  // Creates broad valleys, plateaus, and gradual slopes at ~50-100km scale.
  float MedX = WX, MedY = WY;
  DomainWarp(MedX, MedY, Seed + 8888, 5000000.0f, 0.0000001f);
  float MedTerrain =
      FBM(MedX * 0.0000001f, MedY * 0.0000001f, Seed + 1500, 4, 0.45f, 2.0f);

  // --- LAYER 3: Mountain ranges (RidgeFBM masked to highlands)
  // ---
  // Onset at Continental 0.52 → mountains begin building at moderate
  // continental heights, creating broad ranges like real Earth.
  float MountainMask = FMath::Clamp((Continental - 0.52f) / 0.30f, 0.0f, 1.0f);
  MountainMask = MountainMask * MountainMask; // quadratic onset for natural ramp

  float MX = WX, MY = WY;
  DomainWarp(MX, MY, Seed + 7000, 8000000.0f, 0.00000012f);
  float Ridges =
      RidgeFBM(MX * 0.00000012f, MY * 0.00000012f, Seed + 2000, 4, 0.45f, 2.1f);

  // --- LAYER 3b: Foothills (plain FBM, no shaping) ---
  // Begin at Continental 0.38 for a gradual highland approach.
  float FoothillMask = FMath::Clamp((Continental - 0.38f) / 0.30f, 0.0f, 1.0f);
  FoothillMask = FoothillMask * FoothillMask;
  float FX = WX, FY = WY;
  DomainWarp(FX, FY, Seed + 5555, 4000000.0f, 0.00000025f);
  float Foothills =
      FBM(FX * 0.00000025f, FY * 0.00000025f, Seed + 3300, 3, 0.45f, 2.0f);

  // ================================================================
  //  ROLLING HILLS — each layer warped at its OWN scale (KEY FIX)
  //
  //  When all layers share ONE warp, they all tilt in the same direction
  //  = visible parallel ridges. Each layer needs its own independent warp
  //  sized to match its features:
  //    warpStrength = 0.3 × featureWavelength
  //    warpFreq     = 1.5 × featureFrequency
  // ================================================================

  // HillsBroad: ~15km features. Warp: 4.5km strength at 10km scale.
  float BX = WX, BY = WY;
  DomainWarp(BX, BY, Seed + 3000, 450000.0f, 0.0000010f);
  float HillsBroad =
      FBM(BX * 0.00000065f, BY * 0.00000065f, Seed + 4000, 4, 0.48f, 2.0f);

  // HillsMed: ~5km features. Warp: 1.5km strength at 3.5km scale.
  float MMX = WX, MMY = WY;
  DomainWarp(MMX, MMY, Seed + 4100, 150000.0f, 0.0000027f);
  float HillsMed =
      FBM(MMX * 0.0000020f, MMY * 0.0000020f, Seed + 4400, 3, 0.45f, 2.0f);

  // HillsMicro: ~1.5km features. Warp: 450m strength at 1km scale.
  // Only 2 octaves so finest octave stays at 750m (above aliasing floor).
  float SMX = WX, SMY = WY;
  DomainWarp(SMX, SMY, Seed + 4800, 45000.0f, 0.0000070f);
  float HillsMicro =
      FBM(SMX * 0.0000065f, SMY * 0.0000065f, Seed + 4700, 2, 0.40f, 2.0f);

  // --- Compose terrain height ---
  // Planet mode: no island mask. Oceans form where H < 0.55 (sea level).
  // LandBase lowered to 0.52 so continental noise can push below sea level.
  constexpr float LandBase = 0.52f;

  // ================================================================
  //  REGIONALLY-GATED HILLINESS
  //
  //  Key insight: real erosion flattens valley floors and leaves hills
  //  only in upland zones. We simulate this without actual erosion by
  //  using the REGIONAL layer (MedTerrain, ~100km scale) as a gatekeeper:
  //
  //    MedTerrain < 0.38  →  LocalHillAmp ≈ 0  →  flat valley floor
  //    MedTerrain 0.38-0.55 →  LocalHillAmp 0→1  →  transition hillside
  //    MedTerrain > 0.55  →  LocalHillAmp = 1  →  full rolling hills
  //
  //  Earth-like target amplitudes (in normalized height units):
  //    Continental: 0.10  → 2000m range (broad continental shelves vs highlands)
  //    Regional:    0.08  → ±1600m (valleys vs plateaus)
  //    Mountains:   0.30  → up to 6000m peaks (masked to highlands only)
  //    Foothills:   0.04  → 800m (highland approach zones)
  //    Hills:       0.035 → ±700m (rolling terrain in uplands)
  //    HillsMed:    0.015 → ±300m (secondary undulation)
  //    HillsMicro:  0.005 → ±100m (surface texture)
  // ================================================================

  // LocalHillAmp: how hilly should local terrain be here?
  const float LocalHillAmp = FMath::SmoothStep(0.38f, 0.55f, MedTerrain);

  // Asymmetric erosion on HillsBroad: flatten downslopes (valleys), keep
  // upslopes (crests). This gives hill crests character while valley sides are
  // gentle.
  float LH = HillsBroad - 0.5f;
  LH = LH * (LH < 0.0f ? 0.50f : 1.30f); // valleys compressed, crests expanded
  const float ErodedHills = 0.5f + LH;

  float H =
      LandBase + Continental * 0.10f  // continental tilt (0-2000m)
      + (MedTerrain - 0.5f) * 0.08f   // REGIONAL valleys vs uplands (±1600m)
      + Ridges * MountainMask * 0.30f  // mountain ridgelines (up to 6000m)
      + Foothills * FoothillMask * 0.04f // highland approach (0-800m, masked)
      // Local hills gated by regional height:
      + (ErodedHills - 0.5f) * 0.035f *
            LocalHillAmp // main hills (±700m in uplands)
      + (HillsMed - 0.5f) * 0.015f * LocalHillAmp // secondary (±300m)
      + (HillsMicro - 0.5f) * 0.005f * LocalHillAmp; // micro detail (±100m)

  // Planet mode: no island-edge ocean blend.
  // Low continental noise naturally produces sub-sea-level terrain = oceans.
  return FMath::Clamp(H, 0.0f, 1.0f);
}

// ===================================================================
//  Climate Fields — MACRO ONLY + Voronoi Region Bias + Tiny Detail
//
//  DESIGN RULES (from world-sim spec):
//    1. BiomeFreq = TerrainFreq / 8 to / 16
//       Terrain detail goes down to 0.000002 (5km features)
//       Climate base       uses   0.00000012 (~80km features)
//       Climate medium     uses   0.0000003  (~30km features) at 15%
//       Climate detail     uses   0.0000005  (~20km features) at 15%
//    2. Low octave count: 2-3 max (vs terrain's 4-6)
//    3. Voronoi region bias: ±0.12 per-region offset (coherence, not dominance)
//    4. Medium-freq layer at 15% for within-region variety
//    5. Detail noise at 15% strength for local character
// ===================================================================

float FPMNoise::Temperature(float WorldX, float WorldY, int32 Seed) {
// --- Base: warm at sea level, cooling with altitude (lapse rate) ---
const float H = TerrainHeight(WorldX, WorldY, Seed);
const float SeaLevel = 0.55f;
const float AltAboveSea = FMath::Max(H - SeaLevel, 0.0f);
float BaseTemp = 0.75f - AltAboveSea * 3.5f;

// --- Latitude gradient (spherical planet) ---
// Convert world Y to latitude: Y=0 is equator, ±PlanetCirc/4 is poles.
// Temperature follows real-Earth pattern: hot at equator, cold at poles.
const float HalfCirc = FPMChunkConstants::PlanetCircumferenceCm * 0.25f;
const float LatFraction = FMath::Clamp(WorldY / HalfCirc, -1.0f, 1.0f);
// Cosine curve: 1.0 at equator (lat=0), 0.0 at poles (lat=±90°)
const float LatTemp = FMath::Cos(LatFraction * PI * 0.5f);
// Equator adds +0.15, poles subtract -0.15 from base
BaseTemp += (LatTemp - 0.5f) * 0.30f;

  // --- Voronoi region bias (±0.12 per-region: coherence, not dominance) ---
  // Each macro region has a stable temperature tendency.
  // Reduced from ±0.20 so within-region variety can still shine through.
  float TBias, MBias, EdgeBlend;
  BiomeRegion(WorldX, WorldY, Seed, TBias, MBias, EdgeBlend);
  BaseTemp += TBias * 0.60f; // Scale ±0.20 → ±0.12

  // --- Macro climate noise (3 octaves, ~80km wavelength) ---
  // Primary variation layer. Freq = terrain / 8.
  float TX = WorldX, TY = WorldY;
  DomainWarp(TX, TY, Seed + 100000, 8000000.0f, 0.00000003f);
  float MacroClimate =
      FBM(TX * 0.00000012f, TY * 0.00000012f, Seed + 200000, 3, 0.45f, 2.0f);
  BaseTemp += (MacroClimate - 0.5f) * 0.18f; // ±0.09

  // --- Medium-frequency climate (~30km wavelength, 3 octaves) ---
  // This is the within-region variety layer: creates meadows→forest→plains
  // transitions within a single large region. Different domain warp seed
  // ensures this doesn't correlate with the macro layer.
  float MTX = WorldX, MTY = WorldY;
  DomainWarp(MTX, MTY, Seed + 150000, 4000000.0f, 0.0000001f);
  float MedClimate =
      FBM(MTX * 0.0000003f, MTY * 0.0000003f, Seed + 250000, 3, 0.40f, 2.0f);
  BaseTemp += (MedClimate - 0.5f) * 0.12f; // ±0.06

  // --- Detail noise (15% strength, ~20km wavelength) ---
  // Local variation for organic feel within sub-regions.
  float DetailNoise =
      Simplex2D(TX * 0.0000005f, TY * 0.0000005f, Seed + 300000);
  BaseTemp += DetailNoise * 0.05f; // ±0.05

  return FMath::Clamp(BaseTemp, 0.0f, 1.0f);
}

float FPMNoise::Moisture(float WorldX, float WorldY, int32 Seed) {
  // --- Base: moister near coasts, drier inland ---
  const float Mask = IslandMask(WorldX, WorldY, Seed);
  float CoastalMoisture;
  if (Mask < 0.35f) {
    CoastalMoisture = 0.75f;
  } else {
    float InlandFactor = (Mask - 0.35f) / 0.65f;
    CoastalMoisture = FMath::Lerp(0.75f, 0.40f, InlandFactor);
  }

  // --- Altitude modifier: drier at high altitude ---
  const float H = TerrainHeight(WorldX, WorldY, Seed);
  const float AltAboveSea = FMath::Max(H - 0.55f, 0.0f);
  float BaseMoist = CoastalMoisture - AltAboveSea * 1.5f;

  // --- Voronoi region bias (±0.12 per-region) ---
  float TBias, MBias, EdgeBlend;
  BiomeRegion(WorldX, WorldY, Seed, TBias, MBias, EdgeBlend);
  BaseMoist += MBias * 0.60f; // Scale ±0.20 → ±0.12

  // --- Macro climate noise (3 octaves, ~80km wavelength) ---
  float MX = WorldX, MY = WorldY;
  DomainWarp(MX, MY, Seed + 400000, 8000000.0f, 0.00000003f);
  float MacroClimate =
      FBM(MX * 0.00000012f, MY * 0.00000012f, Seed + 500000, 3, 0.45f, 2.0f);
  BaseMoist += (MacroClimate - 0.5f) * 0.18f; // ±0.09

  // --- Medium-frequency climate (~30km wavelength, 3 octaves) ---
  // Within-region moisture variation: creates wet/dry patches organically.
  float MMX = WorldX, MMY = WorldY;
  DomainWarp(MMX, MMY, Seed + 450000, 4000000.0f, 0.0000001f);
  float MedClimate =
      FBM(MMX * 0.0000003f, MMY * 0.0000003f, Seed + 550000, 3, 0.40f, 2.0f);
  BaseMoist += (MedClimate - 0.5f) * 0.12f; // ±0.06

  // --- Detail noise (15% strength, ~20km wavelength) ---
  float DetailNoise =
      Simplex2D(MX * 0.0000005f, MY * 0.0000005f, Seed + 600000);
  BaseMoist += DetailNoise * 0.05f; // ±0.05

  return FMath::Clamp(BaseMoist, 0.0f, 1.0f);
}

// ===================================================================
//  Diffusion Smoothing (kills all remaining speckling)
//
//  Runs iterative relaxation on a 2D grid:
//    field = lerp(field, neighborAverage, strength)
//  Each pass spreads values to neighbors, creating spatial coherence.
//  6 passes with strength=0.5 is equivalent to a ~3-pixel Gaussian blur.
// ===================================================================

void FPMNoise::DiffusionSmooth(TArray<float> &Grid, int32 Width, int32 Height,
                               int32 Passes, float Strength) {
  if (Width < 3 || Height < 3)
    return;

  for (int32 Pass = 0; Pass < Passes; ++Pass) {
    // Interior cells only (border stays fixed = natural boundary condition)
    for (int32 Y = 1; Y < Height - 1; ++Y) {
      for (int32 X = 1; X < Width - 1; ++X) {
        const int32 Idx = Y * Width + X;
        const float Avg = (Grid[Idx - 1] + Grid[Idx + 1] + Grid[Idx - Width] +
                           Grid[Idx + Width]) *
                          0.25f;
        Grid[Idx] = FMath::Lerp(Grid[Idx], Avg, Strength);
      }
    }
  }
}

// ===================================================================
//  Talus Erosion (eliminates cliffs)
// ===================================================================

void FPMNoise::TalusErosion(TArray<float> &Heights, int32 Res,
                            float CellSpacing, int32 Iters, float MaxSlopeRad) {
  const float MaxDiff = FMath::Tan(MaxSlopeRad) * CellSpacing;

  for (int32 Pass = 0; Pass < Iters; ++Pass) {
    for (int32 Y = 1; Y < Res - 1; ++Y) {
      for (int32 X = 1; X < Res - 1; ++X) {
        const int32 Idx = Y * Res + X;
        float &H = Heights[Idx];

        const int32 Nb[] = {Idx - 1, Idx + 1, Idx - Res, Idx + Res};
        for (int32 N = 0; N < 4; ++N) {
          const float Diff = H - Heights[Nb[N]];
          if (Diff > MaxDiff) {
            const float Excess = (Diff - MaxDiff) * 0.25f;
            H -= Excess;
            Heights[Nb[N]] += Excess;
          }
        }
      }
    }
  }
}

// ===================================================================
//  World-space terrain surface Z (convenience)
// ===================================================================

float FPMNoise::TerrainSurfaceZ(float WorldX, float WorldY, int32 Seed) {
  const float H = TerrainHeight(WorldX, WorldY, Seed);
  return FPMChunkConstants::MinWorldZ +
         H * FPMChunkConstants::WorldHeightRange;
}
