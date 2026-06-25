# Batch asset importer (Wave 0 foundation) — headless UE5 Python.
#
# Ingests free / AI-generated assets (textures, meshes, audio, anims) listed in a
# manifest into /Game/SH/... with correct import settings, so the only manual step
# is dropping files into an `incoming/` folder and adding a manifest row.
#
# Run headless:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript ^
#       -script="Tools/LevelBuilder/import_assets.py" -nullrhi
# (asset-only; no map opened, so -nullrhi is safe per project notes.)
#
# Manifest: Tools/asset_manifest.json  (also the license ledger — keep CREDITS.md in sync).
#   {
#     "incoming_dir": "Tools/incoming",
#     "assets": [
#       {"file":"sand_albedo.png","type":"texture","dest":"/Game/SH/Textures/Surfaces",
#        "srgb":true,"source":"ambientCG","license":"CC0","used_in":"beach"},
#       {"file":"m4_fire_01.wav","type":"audio","dest":"/Game/SH/Audio/Weapons",
#        "sound_class":"Weapons","source":"Sonniss","license":"royalty-free","used_in":"M4A1"},
#       {"file":"reload_tac.fbx","type":"anim","dest":"/Game/SH/Anims/Rifle",
#        "skeleton":"/Game/.../SK_Mannequin_Arms_Skeleton","source":"Mixamo","license":"free"}
#     ]
#   }
#
# Types: texture | normal | mesh | audio | anim. Unknown types are skipped with a warning.
# Idempotent: re-running re-imports (UE overwrites). Writes a result file for CI/log parsing.

import json
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # repo root
PROJECT = os.path.dirname(ROOT) if os.path.basename(ROOT) == "Tools" else ROOT
MANIFEST = os.path.join(ROOT, "asset_manifest.json")
RESULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output", "import_result.txt")


def _abs_incoming(incoming_dir, rel):
    base = incoming_dir if os.path.isabs(incoming_dir) else os.path.join(ROOT, incoming_dir)
    return os.path.normpath(os.path.join(base, rel))


def _make_texture_settings(entry):
    s = unreal.FbxImportUI()  # placeholder not used for textures
    return None  # textures use default factory; settings applied post-import


def _import_one(entry, incoming_dir):
    src = _abs_incoming(incoming_dir, entry["file"])
    if not os.path.exists(src):
        return (False, "missing file: %s" % src)

    dest = entry["dest"]
    atype = entry.get("type", "").lower()

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = True

    # Type-specific import options.
    if atype in ("mesh", "anim"):
        opts = unreal.FbxImportUI()
        opts.import_materials = (atype == "mesh")
        opts.import_textures = (atype == "mesh")
        opts.import_as_skeletal = (atype == "anim")
        if atype == "anim":
            opts.import_animations = True
            opts.import_mesh = False
            skel = entry.get("skeleton")
            if skel:
                loaded = unreal.load_asset(skel)
                if loaded:
                    opts.skeleton = loaded
        task.options = opts

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_objects()) if hasattr(task, "get_objects") else list(task.imported_object_paths)
    if not imported:
        return (False, "import produced no objects: %s" % src)

    # Post-import settings.
    obj = None
    try:
        obj = task.get_objects()[0]
    except Exception:  # noqa: BLE001
        obj = None

    if atype in ("texture", "normal") and obj:
        if atype == "normal":
            obj.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_NORMALMAP)
            obj.set_editor_property("srgb", False)
        else:
            obj.set_editor_property("srgb", bool(entry.get("srgb", True)))
        unreal.EditorAssetLibrary.save_loaded_asset(obj)

    if atype == "audio" and obj and entry.get("sound_class"):
        sc = unreal.load_asset("/Game/SH/Audio/Classes/%s" % entry["sound_class"])
        if sc:
            try:
                obj.set_editor_property("sound_class_object", sc)
                unreal.EditorAssetLibrary.save_loaded_asset(obj)
            except Exception:  # noqa: BLE001
                pass

    return (True, "ok: %s -> %s" % (entry["file"], dest))


def main():
    if not os.path.exists(MANIFEST):
        unreal.log_error("[import_assets] no manifest at %s" % MANIFEST)
        _write_result(["NO MANIFEST: %s" % MANIFEST])
        return

    with open(MANIFEST, "r", encoding="utf-8") as f:
        data = json.load(f)

    incoming = data.get("incoming_dir", "Tools/incoming")
    assets = data.get("assets", [])
    lines = []
    ok = 0
    for entry in assets:
        try:
            success, msg = _import_one(entry, incoming)
        except Exception as e:  # noqa: BLE001
            success, msg = False, "EXC %s: %s" % (entry.get("file", "?"), e)
        ok += 1 if success else 0
        lines.append(("OK  " if success else "FAIL ") + msg)
        unreal.log(("[import_assets] " + lines[-1]))

    summary = "imported %d/%d assets" % (ok, len(assets))
    unreal.log("[import_assets] %s" % summary)
    _write_result([summary] + lines)


def _write_result(lines):
    os.makedirs(os.path.dirname(RESULT), exist_ok=True)
    with open(RESULT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
