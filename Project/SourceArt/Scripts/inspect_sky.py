"""Read saved sky/material settings for diagnostic evidence; does not save assets."""
from pathlib import Path
import json
import unreal

root = Path(__file__).resolve().parents[2]
ml = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
materials = {}
for path in ("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst",
             "/Game/EndlessWorld/Materials/MI_CloudSea"):
    mat = unreal.load_object(None,path)
    if not mat: raise RuntimeError("Missing cloud material "+path)
    materials[path] = {str(n): ml.get_material_instance_scalar_parameter_value(mat,n)
                       for n in ml.get_scalar_parameter_names(mat)}
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level("/Game/EndlessWorld/Maps/EndlessWorld")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
settings=[]
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() in ("EW_Atmosphere","EW_CloudSea"):
        names=("transform_mode","bottom_radius") if actor.get_actor_label()=="EW_Atmosphere" else ("layer_bottom_altitude","layer_height","planet_radius")
        cls=unreal.SkyAtmosphereComponent if actor.get_actor_label()=="EW_Atmosphere" else unreal.VolumetricCloudComponent
        comp=actor.get_component_by_class(cls)
        settings.append({"actor":actor.get_actor_label(),"position":str(actor.get_actor_location()),
                         "properties":{n:str(comp.get_editor_property(n)) for n in names}})
out=root/"Saved/Verification/sky-inspection.json"
out.write_text(json.dumps({"materials":materials,"actors":settings},ensure_ascii=False,indent=2),encoding="utf8")
unreal.log("EW_SKY_INSPECTION "+str(out))
