"""Crea in locale i MetaHuman di Alessia, Luca e Jonathan dai preset gratuiti Epic.

Gli asset generati sono contenuti Epic/MetaHuman: restano sul computer di chi
lancia lo script e sono esclusi da Git (vedi LICENSES.md e .gitignore).

Requisiti: Unreal Engine 5.8 con plugin MetaHumanCharacter, accesso con un
account Epic nell'editor (rig e texture sono generati dal servizio cloud
MetaHuman). L'assemblaggio cuoce texture sulla GPU, quindi serve l'editor
completo (non UnrealEditor-Cmd). Uso da terminale, dalla cartella del progetto:

  CLARA_MH_QUIT=1 UnrealEditor "$PWD/ClaraDayOne.uproject" \
      -ExecutePythonScript="$PWD/Scripts/setup_metahuman_cast.py"

oppure, con l'editor aperto: File > Execute Python Script.

Variabili facoltative: CLARA_MH_FORCE=1 rigenera anche i personaggi esistenti;
CLARA_MH_ONLY=Luca,Jonathan limita i personaggi; CLARA_MH_QUIT=1 chiude
l'editor a fine lavoro.
"""

import os
import unreal

PRESETS = "/MetaHumanCharacter/Optional/Presets"
WARDROBE = "/MetaHumanCharacter/Optional"
SOURCE_DIR = "/Game/Characters/MetaHumans"
BUILD_DIR = "/Game/MetaHumans"

# Colori in spazio lineare per l'abito base MetaHuman (camicia/maglia + pantaloni).
CAST = [
    {
        "name": "Alessia", "role": "Segretaria", "preset": "Jelena",
        "hair": "Hair/WI_Hair_S_LowPonytail",
        "colors": {"Shirt": (0.80, 0.78, 0.74), "Pants": (0.02, 0.025, 0.04)},
    },
    {
        "name": "Luca", "role": "Informatico", "preset": "Lorenzo",
        "hair": "Hair/WI_Hair_S_Casual",
        "colors": {"Shirt": (0.30, 0.45, 0.62), "Pants": (0.05, 0.05, 0.055)},
    },
    {
        "name": "Jonathan", "role": "Informatico", "preset": "Mateo",
        "hair": "Hair/WI_Hair_S_Messy",
        "colors": {"Shirt": (0.035, 0.035, 0.04), "Pants": (0.05, 0.08, 0.15)},
    },
]


def log(message):
    unreal.log_warning(f"CLARA_MH: {message}")


def enum_value(enum, *names):
    for name in names:
        if hasattr(enum, name):
            return getattr(enum, name)
    raise RuntimeError(f"Nessuno di {names} in {enum}")


def select_wardrobe_item(character, slot, item_path):
    item = unreal.load_asset(item_path)
    if not item:
        log(f"{character.get_name()}: elemento non trovato {item_path}")
        return None
    collection = character.internal_collection
    instance = collection.default_instance
    existing = collection.get_item_keys_for_wardrobe_item(item)
    key = existing[0] if existing else collection.try_add_item_from_wardrobe_item(slot, item)
    if key is None:
        log(f"{character.get_name()}: elemento {slot} non aggiunto")
        return None
    instance.set_single_slot_selection(slot, key)
    return key


def tint_outfit(character, outfit_key, colors):
    params = character.internal_collection.default_instance.get_instance_parameters(
        item_path=unreal.MetaHumanPaletteItemPath(item_key=outfit_key))
    log(f"{character.get_name()}: parametri abito {[p.name for p in params]}")
    for param in params:
        for part, rgb in colors.items():
            if str(param.name).startswith("PrimaryColor") and part in str(param.name):
                param.set_color(value=unreal.LinearColor(*rgb, 1.0))


def create_character(entry, subsystem, force):
    name = entry["name"]
    asset_path = f"{SOURCE_DIR}/{name}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path) and not force:
        character = unreal.load_asset(asset_path)
        log(f"{name}: asset esistente riutilizzato")
    else:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            unreal.EditorAssetLibrary.delete_asset(asset_path)
        preset = unreal.load_asset(f"{PRESETS}/{entry['preset']}.{entry['preset']}")
        if not preset:
            raise RuntimeError(f"Preset {entry['preset']} non trovato")
        character = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            name, f"{SOURCE_DIR}/{name}", preset)
        log(f"{name}: creato dal preset {entry['preset']}")

    select_wardrobe_item(character, "Hair", f"{WARDROBE}/Grooms/Bindings/{entry['hair']}.{entry['hair'].split('/')[-1]}")
    outfit = select_wardrobe_item(character, "Outfits", f"{WARDROBE}/Clothing/WI_DefaultGarment.WI_DefaultGarment")

    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError(f"{name}: impossibile aprire il personaggio (è aperto nell'editor?)")
    try:
        skin = character.skin_settings
        resolutions = skin.desired_texture_sources_resolutions
        res4k = enum_value(unreal.RequestTextureResolution, "RES4K", "RES_4K", "RES4_K")
        for field in ["face_albedo", "face_normal", "face_cavity", "face_animated_maps",
                      "body_albedo", "body_normal", "body_cavity", "body_masks"]:
            resolutions.set_editor_property(field, res4k)
        skin.desired_texture_sources_resolutions = resolutions
        subsystem.commit_skin_settings(character, skin)

        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking = True
        rig.report_progress = False
        rig.rig_type = enum_value(unreal.MetaHumanRigType, "JOINTS_AND_BLEND_SHAPES", "JOINTS_AND_BLENDSHAPES", "JOINTS_ONLY")
        subsystem.request_auto_rigging(character=character, params=rig)

        textures = unreal.MetaHumanCharacterTextureRequestParams()
        textures.blocking = True
        textures.report_progress = False
        subsystem.request_texture_sources(character=character, params=textures)
        if not character.has_high_resolution_textures:
            raise RuntimeError(f"{name}: texture ad alta risoluzione non scaricate (accesso Epic attivo?)")

        if outfit is not None:
            subsystem.assemble_for_preview(character=character)
            tint_outfit(character, outfit, entry["colors"])

        if not subsystem.can_build_meta_human(character):
            raise RuntimeError(f"{name}: il personaggio non è pronto per l'assemblaggio")

        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        build.absolute_build_path = BUILD_DIR
        build.common_folder_path = f"{BUILD_DIR}/Common"
        build.enable_wardrobe_item_validation = False
        try:
            subsystem.build_meta_human(character=character, params=build)
        except Exception as error:
            # UE 5.8 segnala come errore un avviso sul Control Rig comune della testa;
            # il risultato si verifica sotto controllando il Blueprint assemblato.
            log(f"{name}: avviso durante l'assemblaggio: {error}")
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)

    unreal.EditorAssetLibrary.save_directory(f"{SOURCE_DIR}/{name}", only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(BUILD_DIR, only_if_is_dirty=True)
    blueprint = f"{BUILD_DIR}/{name}/BP_{name}"
    if not unreal.EditorAssetLibrary.does_asset_exist(blueprint):
        raise RuntimeError(f"{name}: Blueprint assemblato non trovato in {blueprint}")
    log(f"{name} ({entry['role']}): pronto {blueprint}")


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([PRESETS, f"{WARDROBE}/Clothing", f"{WARDROBE}/Grooms", SOURCE_DIR, BUILD_DIR], True)
    force = os.environ.get("CLARA_MH_FORCE") == "1"
    only = {n.strip() for n in os.environ.get("CLARA_MH_ONLY", "").split(",") if n.strip()}
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    failures = []
    for entry in CAST:
        if only and entry["name"] not in only:
            continue
        try:
            create_character(entry, subsystem, force)
        except Exception as error:  # continua con gli altri personaggi
            failures.append(entry["name"])
            unreal.log_error(f"CLARA_MH: {entry['name']}: {error}")
    log("completato" if not failures else f"falliti: {', '.join(failures)}")
    if os.environ.get("CLARA_MH_QUIT") == "1":
        unreal.SystemLibrary.quit_editor()


main()
