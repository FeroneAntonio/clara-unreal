"""Create native Unreal assets once; gameplay itself is C++. Run in Unreal Editor."""
import unreal
from pathlib import Path
import json
import traceback


def build_floor_material(tools, name="M_Floor", tiling=(11.2, 7.3)):
    if unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/" + name):
        return
    source = Path(unreal.SystemLibrary.get_project_directory()) / "SourceAssets" / "PolyHaven"
    textures = {}
    for kind in ("diff", "nor_dx", "rough"):
        name = "laminate_floor_02_" + kind + "_4k"
        asset_path = "/Game/Textures/" + name
        texture = unreal.load_asset(asset_path)
        if not texture:
            task = unreal.AssetImportTask()
            task.filename = str(source / (name + ".jpg"))
            task.destination_path = "/Game/Textures"
            task.destination_name = name
            task.automated = True
            task.save = True
            tools.import_asset_tasks([task])
            texture = unreal.load_asset(asset_path)
        if not texture:
            raise RuntimeError("Texture pavimento mancante: " + name)
        texture.set_editor_property("max_texture_size", 2048)
        if kind != "diff":
            texture.set_editor_property("srgb", False)
        if kind == "nor_dx":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        textures[kind] = texture

    floor = tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    uv = unreal.MaterialEditingLibrary.create_material_expression(floor, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    uv.set_editor_property("u_tiling", tiling[0])
    uv.set_editor_property("v_tiling", tiling[1])
    for index, (kind, output) in enumerate((("diff", unreal.MaterialProperty.MP_BASE_COLOR), ("nor_dx", unreal.MaterialProperty.MP_NORMAL), ("rough", unreal.MaterialProperty.MP_ROUGHNESS))):
        sample = unreal.MaterialEditingLibrary.create_material_expression(floor, unreal.MaterialExpressionTextureSample, -300, index * 250)
        sample.set_editor_property("texture", textures[kind])
        if kind == "nor_dx":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif kind == "rough":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        if not unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", sample, "UVs"):
            raise RuntimeError("Coordinate del materiale non collegate")
        unreal.MaterialEditingLibrary.connect_material_property(sample, "R" if kind == "rough" else "RGB", output)
    unreal.MaterialEditingLibrary.recompile_material(floor)
    unreal.EditorAssetLibrary.save_loaded_asset(floor)


def build_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    build_floor_material(tools)
    build_floor_material(tools, "M_Wood", (0.47, 0.94))
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/M_Glass"):
        glass = tools.create_asset("M_Glass", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
        glass.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        glass.set_editor_property("two_sided", True)
        for parameter, value, prop in (("Opacity", 0.1, unreal.MaterialProperty.MP_OPACITY), ("Roughness", 0.08, unreal.MaterialProperty.MP_ROUGHNESS)):
            node = unreal.MaterialEditingLibrary.create_material_expression(glass, unreal.MaterialExpressionScalarParameter, -300, 0)
            node.set_editor_property("parameter_name", parameter)
            node.set_editor_property("default_value", value)
            unreal.MaterialEditingLibrary.connect_material_property(node, "", prop)
        node = unreal.MaterialEditingLibrary.create_material_expression(glass, unreal.MaterialExpressionVectorParameter, -300, 300)
        node.set_editor_property("parameter_name", "BaseColor")
        node.set_editor_property("default_value", unreal.LinearColor(0.75, 0.9, 0.95, 1))
        unreal.MaterialEditingLibrary.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(glass)
        unreal.EditorAssetLibrary.save_loaded_asset(glass)
    material_path = "/Game/Materials/M_Office"
    material = unreal.load_asset(material_path)
    if not material:
        material = tools.create_asset("M_Office", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
        if not material:
            raise RuntimeError("Creazione materiale non riuscita")
        color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
        color.set_editor_property("parameter_name", "BaseColor")
        color.set_editor_property("default_value", unreal.LinearColor(0.45, 0.48, 0.5, 1))
        roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 200)
        roughness.set_editor_property("parameter_name", "Roughness")
        roughness.set_editor_property("default_value", 0.55)
        unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)


def build_assets():
    build_materials()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    map_path = "/Game/Maps/DayOne"
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        if not levels.load_level(map_path):
            raise RuntimeError("Apertura mappa non riuscita")
    elif not levels.new_level(map_path):
        raise RuntimeError("Creazione mappa non riuscita")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mode = unreal.load_class(None, "/Script/ClaraDayOne.ClaraGameMode")
    if not mode:
        raise RuntimeError("Modulo di gioco non caricato")
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not any(isinstance(actor, unreal.PlayerStart) for actor in actors.get_all_level_actors()):
        actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-600, 0, 100), unreal.Rotator(0, 0, 0))
    if not levels.save_current_level():
        raise RuntimeError("Salvataggio mappa non riuscito")
    unreal.log("CLARA_BOOTSTRAP_OK: mappa e materiale salvati")


try:
    build_assets()
    result = {"success": True}
except Exception:
    result = {"success": False, "error": traceback.format_exc()}
    unreal.log_error(result["error"])
finally:
    report = Path(unreal.SystemLibrary.get_project_directory()) / "Saved" / "bootstrap-result.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2))
    unreal.SystemLibrary.quit_editor()
