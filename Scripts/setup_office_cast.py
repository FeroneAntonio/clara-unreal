"""Create two local child presets of Epic's complete, already dressed Blueprint.

Run in Unreal after installing the official Epic City Sample Crowds package.
Does not save or modify any original CitySampleCrowd asset.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "OfficeCast"
ROOT.mkdir(parents=True, exist_ok=True)
BASE = "/Game/CitySampleCrowd/Blueprints/BP_CrowdCharacter.BP_CrowdCharacter_C"
DESTINATION = "/Game/Characters/OfficeCast"
parent = unreal.load_class(None, BASE)
if not parent:
    raise RuntimeError("Original Epic crowd Blueprint could not load")
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = {"parent": BASE, "presets": [], "original_defaults": {}}
original = unreal.get_default_object(parent)
for name in ("Gender", "BodyType", "RandomOptions", "HeadIndex", "HairIndex",
             "OutfitIndex", "OutfitMaterialIndex", "AccessoryIndex"):
    try:
        report["original_defaults"][name] = str(original.get_editor_property(name))
    except Exception as error:
        report["original_defaults"][name] = "UNAVAILABLE: " + str(error)
(ROOT / "city-preset-probe.json").write_text(json.dumps(report, indent=2))


def set_options(obj, values):
    for name, value in values.items():
        obj.set_editor_property(name, value)
        assert obj.get_editor_property(name) == value, name


def inspect_actor(cls, values=None):
    actor = subsystem.spawn_actor_from_class(cls, unreal.Vector(), transient=True)
    if not actor:
        raise RuntimeError("Could not instantiate native preset")
    try:
        if values:
            set_options(actor, values)
            actor.call_method("UserConstructionScript")
        components = {}
        for part in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            mesh = part.get_skeletal_mesh_asset()
            components[part.get_name()] = mesh.get_path_name() if mesh else None
        for required in ("Body", "Top", "Bottom", "Shoes", "Face"):
            assert components.get(required), f"Missing original Epic part: {required}"
        assert components["Shoes"].endswith("m_tal_nrw_oxfords.m_tal_nrw_oxfords")
        assert components["Bottom"].endswith("m_tal_nrw_slacks.m_tal_nrw_slacks")
        return components
    finally:
        subsystem.destroy_actor(actor)


presets = [("Luca", {"RandomOptions": False, "HeadIndex": 0, "OutfitIndex": 0, "OutfitMaterialIndex": 0}),
           ("Jonathan", {"RandomOptions": False, "HeadIndex": 3, "OutfitIndex": 0, "OutfitMaterialIndex": 1})]
validated = []
for name, values in presets:
    # Validate the original Blueprint's parameter interface before creating assets.
    validated.append(inspect_actor(parent, values))
assert validated[0]["Face"] != validated[1]["Face"], "Presets must have distinct original heads"
for (name, values), expected in zip(presets, validated):
    asset_name = f"BP_TNES_{name}Office"
    path = DESTINATION + "/" + asset_name
    blueprint = unreal.load_asset(path)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, DESTINATION, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f"Could not create {path}")
    cls = blueprint.generated_class()
    set_options(unreal.get_default_object(cls), values)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cls = blueprint.generated_class()
    observed = inspect_actor(cls)
    assert observed == expected, (name, observed, expected)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save local preset {path}")
    report["presets"].append({"name": name, "asset": path, "options": values,
                               "components": observed, "runtime_validation": "pending"})
(ROOT / "city-presets.json").write_text(json.dumps(report, indent=2))
unreal.log("CITY_OFFICE_PRESETS_PASS " + json.dumps(report))

import unreal
for name in ["MTN_N_Idle","MTN_N_Walk_InPlace"]:
    src="/Game/CitySampleCrowd/Character/Anims/Loco/"+name
    dst="/Game/Characters/OfficeCast/Animations/"+name
    asset=unreal.load_asset(dst) or unreal.EditorAssetLibrary.duplicate_asset(src,dst)
    assert asset is not None,dst
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset),dst
    unreal.log("TNES_NATIVE_ANIMATION_READY "+asset.get_path_name())
