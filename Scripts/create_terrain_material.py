"""
FaldoranPrime - Create M_TerrainBiome Material
Run this from the Unreal Editor Python console:
  File > Execute Python Script > select this file
"""

import unreal

def create_terrain_biome_material():
    asset_path = "/Game/Materials/M_TerrainBiome"
    package_path = "/Game/Materials"
    asset_name = "M_TerrainBiome"

    # Create the Materials folder if it doesn't exist
    unreal.EditorAssetLibrary.make_directory(package_path)

    # Delete existing asset so we can recreate cleanly
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    # Create the material asset
    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(asset_name, package_path,
                                        unreal.Material, factory)
    if not material:
        unreal.log_error("FPM: Failed to create material asset.")
        return

    # ── Vertex Color node ─────────────────────────────────────────────
    vc_expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -350, 0)

    # ── Desaturation to keep a neutral luminance ──────────────────────
    # Connect VertexColor.RGB directly → Base Color
    unreal.MaterialEditingLibrary.connect_material_property(
        vc_expr, "RGB",
        unreal.MaterialProperty.MP_BASE_COLOR)

    # ── Roughness: subtle variation driven by vertex colour value ─────
    desat = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionDesaturation, -150, 150)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vc_expr, "RGB", desat, "Input")
    # Remap desaturated value → roughness range [0.55, 0.85] using Multiply+Add
    mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 50, 150)
    const_mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -50, 230)
    const_mul.set_editor_property("R", 0.30)  # 0.30 range
    unreal.MaterialEditingLibrary.connect_material_expressions(
        desat, "", mul, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        const_mul, "", mul, "B")
    add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 180, 150)
    const_add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, 80, 230)
    const_add.set_editor_property("R", 0.55)  # base roughness
    unreal.MaterialEditingLibrary.connect_material_expressions(
        mul, "", add, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        const_add, "", add, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        add, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # ── Metallic = 0 ─────────────────────────────────────────────────
    zero = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -350, 300)
    zero.set_editor_property("R", 0.0)
    unreal.MaterialEditingLibrary.connect_material_property(
        zero, "", unreal.MaterialProperty.MP_METALLIC)

    # ── Material properties ───────────────────────────────────────────
    material.set_editor_property("blend_mode",
                                 unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)

    # Save
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(asset_path)

    unreal.log("FPM: Created M_TerrainBiome at " + asset_path)
    unreal.log("FPM: Now assign it to your WorldChunkManager > FPM|World > Terrain Material")
    return asset_path

create_terrain_biome_material()
