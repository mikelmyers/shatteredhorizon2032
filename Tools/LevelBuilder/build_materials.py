# Build PBR master materials from imported CC0 surface textures (Wave 3).
# Headless: UnrealEditor-Cmd.exe <proj> -ExecutePythonScript="...build_materials.py" -nullrhi
#
# For each surface it creates /Game/SH/Materials/M_SH_<name> with Color/Normal/Roughness
# wired from /Game/SH/Textures/Surfaces/sh_<name>_{color,normal,rough}. Additive and
# idempotent: creating these new assets never touches the existing level.

import os

import unreal

TEX_DIR = "/Game/SH/Textures/Surfaces"
MAT_DIR = "/Game/SH/Materials"
RESULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output", "materials_result.txt")

# name -> (default UV tiling). Tiling tuned so 1K textures read at a believable scale.
SURFACES = {
    "beachsand": 8.0,
    "concrete": 4.0,
    "asphalt": 6.0,
    "rock": 3.0,
    "grass": 10.0,
    "ground054": 8.0,
}

mel = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()


def _load_tex(name, suffix):
    path = "%s/sh_%s_%s" % (TEX_DIR, name, suffix)
    return unreal.load_asset(path)


def build_one(name, tiling):
    color = _load_tex(name, "color")
    normal = _load_tex(name, "normal")
    rough = _load_tex(name, "rough")
    if not color:
        return (False, "no color texture for %s (%s)" % (name, TEX_DIR))

    mat_name = "M_SH_%s" % name
    mat_path = "%s/%s" % (MAT_DIR, mat_name)
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        unreal.EditorAssetLibrary.delete_asset(mat_path)

    mat = assets.create_asset(mat_name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        return (False, "create_asset failed for %s" % mat_name)

    # Shared tiling via a TexCoord node scaled by a constant.
    texcoord = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 0)
    tiling_const = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -900, 150)
    tiling_const.set_editor_property("r", float(tiling))
    mel.connect_material_expressions(texcoord, "", mul, "A")
    mel.connect_material_expressions(tiling_const, "", mul, "B")

    # Base color.
    s_color = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, -200)
    s_color.set_editor_property("texture", color)
    mel.connect_material_expressions(mul, "", s_color, "UVs")
    mel.connect_material_property(s_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    # Normal.
    if normal:
        s_norm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 100)
        s_norm.set_editor_property("texture", normal)
        s_norm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(mul, "", s_norm, "UVs")
        mel.connect_material_property(s_norm, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # Roughness. The rough map is a linear RGB texture, so use a LINEAR_COLOR
    # sampler (LINEAR_GRAYSCALE would fail to compile on a 3-channel texture) and
    # drive roughness from the R channel.
    if rough:
        s_rough = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 400)
        s_rough.set_editor_property("texture", rough)
        s_rough.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        mel.connect_material_expressions(mul, "", s_rough, "UVs")
        mel.connect_material_property(s_rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return (True, "built %s" % mat_path)


def main():
    lines = []
    ok = 0
    for name, tiling in SURFACES.items():
        try:
            success, msg = build_one(name, tiling)
        except Exception as e:  # noqa: BLE001
            success, msg = False, "EXC %s: %s" % (name, e)
        ok += 1 if success else 0
        lines.append(("OK  " if success else "FAIL ") + msg)
        unreal.log("[build_materials] " + lines[-1])
    summary = "built %d/%d materials" % (ok, len(SURFACES))
    unreal.log("[build_materials] " + summary)
    os.makedirs(os.path.dirname(RESULT), exist_ok=True)
    with open(RESULT, "w", encoding="utf-8") as f:
        f.write("\n".join([summary] + lines) + "\n")


if __name__ == "__main__":
    main()
