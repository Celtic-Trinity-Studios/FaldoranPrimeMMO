# PCG Biome Population Guide

This guide details how to set up the PCG Graph for the Faldoran Prime MMO terrain system.

## 1. Prerequisites
- Ensure the **PCG Framework** and **PCG Geometry Script Interop** plugins are enabled in Unreal Engine 5.4.
- Code changes in `AFPMChunkActor` have been compiled.

## 2. Create the PCG Graph
1. In the Content Browser, right-click and select **PCG > PCG Graph**.
2. Name it `PCG_BiomeSpawner`.

## 3. Graph Logic
The graph should sample the underlying chunk mesh and spawn assets based on Vertex Colors.

### A. Input & Sampling
1. **Input Node**: Start with the `Input` node.
2. **Mesh Sampler**:
   - Add a `Mesh Sampler` node.
   - Connect `Input` (Actor) to `Mesh Sampler` (Spatial).
   - *Settings*:
     - **Sampling Method**: `One Point Per Vertex` (since we are filtering by vertex color) OR `Poisson` if you want random distribution on the surface.
     - **Attribute to Copy**: Ensure Vertex Colors are copied.

### B. Biome Filtering (Vertex Color)
We map Vertex Colors to Biomes:
- **Red**: Meadows
- **Green**: Forest
- **Blue**: Mountain
- **Alpha**: Snow
- **Black (Zero)**: Ocean

1. **Point Filter (Forest)**:
   - Add a `Point Filter` node.
   - **Target Attribute**: `Color.G` (Green Channel).
   - **Operator**: `Greater` > `0.5`.
   - **Output**: Points in the Forest.

2. **Point Filter (Mountain/Rocks)**:
   - Add a parallel `Point Filter` node from the source points.
   - **Target Attribute**: `Color.B` (Blue Channel).
   - **Operator**: `Greater` > `0.5`.

### C. Spawning Logic
#### Forest Spawner (Green)
1. **Density Filter**: Add `Density Filter` to control how many trees spawn.
2. **Transform Points**: Add random Rotation (Z: 0-360) and Component Scale (0.8-1.2) variation.
3. **Static Mesh Spawner**:
   - Add `Static Mesh Spawner`.
   - **Mesh Entries**: Add your Tree meshes (e.g., `SM_PineTree`).
   - Connect the Forest points here.

#### Rock Spawner (Blue)
1. **Density Filter**: Lower density (e.g., 0.1).
2. **Transform Points**: Random Rotation/Scale.
3. **Static Mesh Spawner**:
   - **Mesh Entries**: Add Rock meshes.

## 4. Integration
1. Open the Blueprint subclass of `AFPMChunkActor` (e.g., `BP_FPMChunkActor` if it exists, or create one).
2. Select the `BiomeSpawner` component (inherited).
3. In the Details panel, find the **Graph** property (or `BiomeGraph` variable under "FPM | PCG").
4. Assign `PCG_BiomeSpawner` to the `BiomeGraph` property.
   - *Note*: If you set it on the component directly, it might work, but the C++ logic explicitly sets it from the `BiomeGraph` variable during generation to ensure consistency.

## 5. Validation
- Run PIE (Play In Editor).
- As chunks spawn, the `InitializeChunk` function will build the mesh.
- `TriggerBiomeGeneration` is called automatically.
- Trees and rocks should populate after the mess appears.

## 6. Troubleshooting
### Missing Vegetation?
If trees do not appear, it may be due to **Async Collision Cooking**. 
- The `PCGComponent` might try to sample the surface before the collision mesh is fully cooked.
- **Fix**: In `AFPMChunkActor.cpp`, set `TerrainMesh->bUseAsyncCooking = false;` temporarily to verify. 
- Alternatively, add a delay in the blueprint before triggering generation, or ensure use of `Mesh Sampler` allows sampling visual mesh data directly.
