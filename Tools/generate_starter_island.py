"""
Faldoran Prime MMO - Starter Island Heightmap Generator (v2)

Fixed for UE5 import: values centered around 32768 (UE5 baseline).

HOW TO RUN:
  python Tools/generate_starter_island.py

Copyright Celtic Trinity Studios, 2026. All Rights Reserved.
"""

import os
import numpy as np
from PIL import Image

# ===================================================================
#  SETTINGS
# ===================================================================

SEED = 42
WIDTH = 2017
HEIGHT = 2017
ISLAND_RADIUS = 0.40
OUTPUT_DIR = "Tools/Output"

# UE5 height mapping:
#   32768 = Z=0 (flat baseline)
#   Values above 32768 = hills/mountains
#   Values below 32768 = below ground
#
# Height in game (cm) = (value - 32768) * ScaleZ / 128
# With ScaleZ=100:  1 heightmap unit = 100/128 = 0.78 cm
# So a value of 32768 + 5000 = 37768 gives height of 5000*100/128 = 3906 cm = ~39m
#
# For our island we want:
#   Sea floor:      ~50m below baseline  -> 32768 - 6400 = 26368
#   Sea level:      at baseline          -> 32768
#   Meadows:        ~10-30m above        -> 32768 + 1280 to 3840
#   Forest hills:   ~50-150m above       -> 32768 + 6400 to 19200
#   Mountain peaks: ~300-400m above      -> 32768 + 25600 to 32000

BASELINE = 32768
OCEAN_FLOOR = BASELINE - 6400       # ~50m below
BEACH_HEIGHT = BASELINE + 500       # ~4m above
MEADOW_HEIGHT = BASELINE + 2500     # ~20m above
FOREST_HEIGHT = BASELINE + 12000    # ~94m above
MOUNTAIN_PEAK = BASELINE + 28000    # ~219m above

# ===================================================================
#  NOISE (vectorized numpy, fast)
# ===================================================================

def make_gradient_grid(size, rng):
    angles = rng.uniform(0, 2 * np.pi, (size + 1, size + 1))
    return np.cos(angles), np.sin(angles)


def fade(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


def perlin_2d(width, height, grid_size, rng):
    gx, gy = make_gradient_grid(grid_size, rng)
    x_coords = np.linspace(0, grid_size, width, endpoint=False)
    y_coords = np.linspace(0, grid_size, height, endpoint=False)
    x_grid, y_grid = np.meshgrid(x_coords, y_coords)
    x0 = x_grid.astype(int)
    y0 = y_grid.astype(int)
    x1 = x0 + 1
    y1 = y0 + 1
    fx = x_grid - x0
    fy = y_grid - y0
    u = fade(fx)
    v = fade(fy)
    dot00 = gx[y0, x0] * fx + gy[y0, x0] * fy
    dot10 = gx[y0, x1] * (fx - 1) + gy[y0, x1] * fy
    dot01 = gx[y1, x0] * fx + gy[y1, x0] * (fy - 1)
    dot11 = gx[y1, x1] * (fx - 1) + gy[y1, x1] * (fy - 1)
    top = dot00 * (1 - u) + dot10 * u
    bottom = dot01 * (1 - u) + dot11 * u
    return top * (1 - v) + bottom * v


def fractal_noise(width, height, rng, octaves=5, base_grid=4):
    result = np.zeros((height, width), dtype=np.float64)
    amplitude = 1.0
    total_amplitude = 0.0
    for i in range(octaves):
        grid_size = base_grid * (2 ** i)
        noise = perlin_2d(width, height, grid_size, rng)
        result += noise * amplitude
        total_amplitude += amplitude
        amplitude *= 0.5
    return result / total_amplitude


# ===================================================================
#  ISLAND SHAPE
# ===================================================================

def make_island_mask(width, height):
    y = np.linspace(0, 1, height)
    x = np.linspace(0, 1, width)
    xx, yy = np.meshgrid(x, y)
    dx = xx - 0.5
    dy = yy - 0.5
    dist = np.sqrt(dx * dx + dy * dy) / ISLAND_RADIUS
    mask = np.clip(1.0 - dist, 0.0, 1.0)
    # Smooth cubic falloff for natural coast
    return mask * mask * (3 - 2 * mask)


def make_biome_bias(width, height):
    """North = mountains, middle = forest, south = flat meadows."""
    y = np.linspace(0, 1, height)
    x = np.linspace(0, 1, width)
    _, yy = np.meshgrid(x, y)
    bias = np.zeros((height, width), dtype=np.float64)

    # Mountains in the north (y < 0.35)
    mt = np.clip(1.0 - (yy / 0.35), 0, 1)
    bias += (mt ** 1.5) * 0.8

    # Forest hills in the middle (around y=0.45)
    fd = np.abs(yy - 0.45) / 0.20
    fs = np.clip(1.0 - fd, 0, 1)
    bias += fs * 0.25

    # Flatten meadows in the south (y > 0.55)
    ms = np.clip((yy - 0.55) / 0.30, 0, 1)
    bias -= (ms ** 0.8) * 0.35

    return bias


def carve_river(heightmap_float, width, height, rng):
    """Carve a river channel (operates on 0-1 float heightmap)."""
    y = np.linspace(0, 1, height)
    x = np.linspace(0, 1, width)
    xx, yy = np.meshgrid(x, y)

    river_noise = perlin_2d(1, height, 6, rng)
    river_x_offset = river_noise[:, 0] * 0.08

    base_x = 0.45 + yy * 0.12
    river_center = base_x + river_x_offset[:, np.newaxis]

    dist = np.abs(xx - river_center)
    river_w = 0.005 + yy * 0.006
    factor = np.clip(1.0 - (dist / river_w), 0, 1) ** 2

    carve = factor * 0.12
    above_base = heightmap_float > 0.05
    carved = np.maximum(0.02, heightmap_float - carve)
    return np.where(above_base & (factor > 0), carved, heightmap_float)


# ===================================================================
#  MAIN
# ===================================================================

def generate():
    print("=" * 60)
    print("  Faldoran Prime - Starter Island Generator v2")
    print("=" * 60)
    print(f"  Seed: {SEED}  |  Resolution: {WIDTH}x{HEIGHT}")
    print("=" * 60)
    print()

    rng = np.random.default_rng(SEED)

    # 1. Base noise (0 to 1 range)
    print("  [1/6] Generating terrain noise...")
    noise = fractal_noise(WIDTH, HEIGHT, rng, octaves=6, base_grid=3)
    noise = (noise - noise.min()) / (noise.max() - noise.min())

    # 2. Island mask (0 at edges, 1 at center)
    print("  [2/6] Creating island shape...")
    mask = make_island_mask(WIDTH, HEIGHT)

    # 3. Biome elevation bias
    print("  [3/6] Applying biome heights...")
    bias = make_biome_bias(WIDTH, HEIGHT)

    # 4. Combine into floating point heightmap (0 = lowest, 1 = highest)
    hm = (noise * 0.3 + bias) * mask
    hm = np.clip(hm, 0, 1)

    # 5. Flatten meadows
    print("  [4/6] Flattening Meadows...")
    y = np.linspace(0, 1, HEIGHT)
    _, yy = np.meshgrid(np.linspace(0, 1, WIDTH), y)
    meadow_factor = np.clip((yy - 0.58) / 0.25, 0, 1)
    target = 0.08
    strength = meadow_factor * 0.75
    above_zero = hm > 0.02
    blended = hm * (1 - strength) + target * strength
    hm = np.where(above_zero, blended, hm)

    # 6. Carve river
    print("  [5/6] Carving river...")
    river_rng = np.random.default_rng(SEED + 500)
    hm = carve_river(hm, WIDTH, HEIGHT, river_rng)
    hm = np.clip(hm, 0, 1)

    # 7. Convert float (0-1) to UE5 uint16 centered on 32768
    #    0.0 -> OCEAN_FLOOR (below baseline)
    #    island land -> BASELINE to MOUNTAIN_PEAK (above baseline)
    print("  [6/6] Saving files...")

    # Map: where mask=0 (ocean) -> OCEAN_FLOOR, where hm>0 -> scale to land heights
    hm_16 = np.full((HEIGHT, WIDTH), OCEAN_FLOOR, dtype=np.float64)

    # Land areas: map 0-1 float to BEACH_HEIGHT..MOUNTAIN_PEAK
    land_mask = mask > 0.01
    land_range = MOUNTAIN_PEAK - BEACH_HEIGHT
    hm_16[land_mask] = BEACH_HEIGHT + hm[land_mask] * land_range

    # Ocean areas stay at OCEAN_FLOOR
    ocean_mask = mask <= 0.01
    hm_16[ocean_mask] = OCEAN_FLOOR

    # Coastal transition: blend between ocean floor and land
    coast_mask = (mask > 0.01) & (mask < 0.15)
    coast_blend = (mask[coast_mask] - 0.01) / 0.14
    hm_16[coast_mask] = (OCEAN_FLOOR * (1 - coast_blend)
                         + hm_16[coast_mask] * coast_blend)

    # Clamp to valid uint16 range
    hm_16 = np.clip(hm_16, 0, 65535).astype(np.uint16)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Save .r16
    r16_path = os.path.join(OUTPUT_DIR, "StarterIsland_Heightmap.r16")
    hm_16.tofile(r16_path)
    mb = os.path.getsize(r16_path) / (1024 * 1024)
    print(f"         {r16_path} ({mb:.1f} MB)")

    # Save 16-bit PNG (alternate import format)
    png16_path = os.path.join(OUTPUT_DIR, "StarterIsland_Heightmap.png")
    img16 = Image.fromarray(hm_16, mode='I;16')
    img16.save(png16_path)
    print(f"         {png16_path}")

    # Save 8-bit preview
    preview_data = ((hm_16.astype(np.float64) / 65535) * 255).astype(np.uint8)
    preview = Image.fromarray(preview_data, mode='L')
    preview_path = os.path.join(OUTPUT_DIR, "StarterIsland_Preview.png")
    preview.save(preview_path)
    print(f"         {preview_path} (preview)")

    abs_r16 = os.path.abspath(r16_path)

    print()
    print("=" * 60)
    print("  DONE!")
    print("=" * 60)
    print()
    print("  IMPORT INTO UE5:")
    print("  1. File > New Level > Empty Open World")
    print("  2. Save as Content/Maps/L_StarterIsland")
    print("  3. Shift+3 > Manage > Import from File")
    print(f"  4. Browse to: {abs_r16}")
    print("  5. Settings:")
    print("       Section Size:           63 x 63 Quads")
    print("       Sections Per Component: 1 x 1")
    print("       Number of Components:   32 x 32")
    print("       Scale X: 100")
    print("       Scale Y: 100")
    print("       Scale Z: 100")
    print("  6. Click Import")
    print("  7. Add a Directional Light so you can see!")
    print()
    print("  Height values are centered around 32768 (UE5 baseline).")
    print("  Mountains reach ~219m above baseline.")
    print("  Ocean floor is ~50m below baseline.")
    print()


if __name__ == "__main__":
    generate()
