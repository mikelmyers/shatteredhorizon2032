# Creates the 11 weapon DataAssets at /Game/SH/Weapons using the authoritative
# C++ USHWeaponDataFactory::ApplyDefaults_* functions (BlueprintCallable).
# This is the CORRECT path — the generated WeaponDataPipeline importer creates
# base UDataAssets with a flat schema that doesn't map to USHWeaponDataAsset.
# Run headless (asset-only, no map): UnrealEditor-Cmd <proj> -ExecutePythonScript=... -nullrhi
import unreal

BASE = "/Game/SH/Weapons"

# (asset name, distinctive token to match a factory method)
WEAPONS = [
    ("DA_M27_IAR",     "m27"),
    ("DA_M4A1",        "m4a1"),
    ("DA_M249_SAW",    "m249"),
    ("DA_M110_SASS",   "m110"),
    ("DA_M17_SIG",     "m17"),
    ("DA_M320_GL",     "m320"),
    ("DA_Mossberg590", "mossberg"),
    ("DA_SniperLapua", "lapua"),
    ("DA_M2_Browning", "browning"),
    ("DA_QBZ95",       "qbz"),
    ("DA_Type56",      "type56"),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
factory_cls = unreal.SHWeaponDataFactory
methods = [m for m in dir(factory_cls) if "apply_defaults" in m.lower()]
unreal.log("[weapons] factory methods: %s" % methods)


def find_method(token):
    t = token.replace("_", "").lower()
    for m in methods:
        if t in m.replace("_", "").lower():
            return m
    return None


created = updated = failed = 0
for asset_id, token in WEAPONS:
    path = "%s/%s" % (BASE, asset_id)
    method = find_method(token)
    if not method:
        unreal.log_warning("[weapons] no factory method for %s (token=%s)" % (asset_id, token))
        failed += 1
        continue

    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        updated += 1
    else:
        asset = tools.create_asset(asset_id, BASE, unreal.SHWeaponDataAsset, None)
        created += 1

    if asset is None:
        unreal.log_warning("[weapons] failed to create/load %s" % path)
        failed += 1
        continue

    try:
        getattr(factory_cls, method)(asset)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log("[weapons] OK %s via %s" % (asset_id, method))
    except Exception as e:
        unreal.log_warning("[weapons] %s failed: %s" % (asset_id, e))
        failed += 1

unreal.log("[weapons] DONE created=%d updated=%d failed=%d" % (created, updated, failed))
