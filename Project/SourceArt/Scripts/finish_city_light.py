"""Keep a readable cool daylight fill inside the deep, inhabited street canyons."""
from pathlib import Path
import json,unreal
assets=unreal.EditorAssetLibrary
ml=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level("/Game/EndlessWorld/Maps/EndlessWorld"):
    raise RuntimeError("Cannot open the city lighting map")
settings_values={"lumen_skylight_leaking":.22,"lumen_full_skylight_leaking_distance":180.,
                 "lumen_diffuse_color_boost":1.35,"indirect_lighting_intensity":1.25,
                 "auto_exposure_min_brightness":13.3,"auto_exposure_max_brightness":15.2,
                 "auto_exposure_bias":.25,"auto_exposure_low_percent":55.,"auto_exposure_high_percent":80.,
                 "histogram_log_min":8.,"histogram_log_max":18.,
                 "auto_exposure_speed_up":2.,"auto_exposure_speed_down":1.,
                 "ambient_occlusion_intensity":.5}
for actor in actors.get_all_level_actors():
    name=actor.get_actor_label()
    if name=="EW_Sun":
        actor.set_actor_rotation(unreal.Rotator(pitch=-65,yaw=-80,roll=0),False)
        actor.light_component.set_editor_property("light_source_angle",.8)
    elif name=="EW_SkyFill":
        actor.light_component.set_intensity(2.2)
    elif name=="EW_Look":
        pp=actor.get_editor_property("settings")
        pp.set_editor_property("override_auto_exposure_method",True)
        pp.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_HISTOGRAM)
        for key,value in settings_values.items():
            pp.set_editor_property("override_"+key,True);pp.set_editor_property(key,value)
        pp.set_editor_property("override_lumen_skylight_leaking_tint",True)
        pp.set_editor_property("lumen_skylight_leaking_tint",unreal.LinearColor(.70,.84,1,1))
        actor.set_editor_property("settings",pp)

# Interior windows need daylight-scale radiance; values around one disappear
# under daylight exposure. The surface keeps its palette.
mat=assets.load_asset("/Game/EndlessWorld/Materials/M_Glow")
if not mat:raise RuntimeError("Missing the window-light material")
ml.delete_all_material_expressions(mat)
vc=ml.create_material_expression(mat,unreal.MaterialExpressionVertexColor,-550,0)
mul=ml.create_material_expression(mat,unreal.MaterialExpressionMultiply,-180,150)
power=ml.create_material_expression(mat,unreal.MaterialExpressionConstant,-500,250)
power.set_editor_property("r",3000.)
rough=ml.create_material_expression(mat,unreal.MaterialExpressionConstant,-200,400)
rough.set_editor_property("r",.68)
for a,out,b,pin in [(vc,"",mul,"A"),(power,"",mul,"B")]:
    if not ml.connect_material_expressions(a,out,b,pin):raise RuntimeError("Cannot connect the window-light expression")
for n,out,prop in [(vc,"",unreal.MaterialProperty.MP_BASE_COLOR),(mul,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                   (rough,"",unreal.MaterialProperty.MP_ROUGHNESS)]:
    if not ml.connect_material_property(n,out,prop):raise RuntimeError("Cannot connect the window-light material")
ml.recompile_material(mat)
if not assets.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError("Cannot save the window-light material")
if not levels.save_current_level():raise RuntimeError("Cannot save the city lighting")
root=Path(__file__).resolve().parents[2]
(root/"Saved/Verification/city-light.json").write_text(json.dumps({"success":True,"post_process":settings_values,
    "skylight_intensity":2.2,"window_emissive_scale":3000,"sun_pitch":-65,"sun_yaw":-80},indent=2),encoding="utf8")
unreal.log("EW_CITY_LIGHT_READY")
