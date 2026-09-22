"""A subtle color wash over the real 3D scene, preserving spatial detail."""
import unreal
assets=unreal.EditorAssetLibrary
ml=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path="/Game/EndlessWorld/Maps/EndlessWorld"
if not levels.load_level(map_path):raise RuntimeError("Unable to load the art map")
path="/Game/EndlessWorld/Materials/M_IllustratedLight"
mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset("M_IllustratedLight","/Game/EndlessWorld/Materials",unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(mat)
mat.set_editor_property("material_domain",unreal.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property("blendable_location",unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
scene=ml.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-700,0)
scene.set_editor_property("scene_texture_id",unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
scene.set_editor_property("filtered",False)
wash=ml.create_material_expression(mat,unreal.MaterialExpressionCustom,-350,0)
wash.set_editor_property("output_type",unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom_input=unreal.CustomInput()
custom_input.set_editor_property("input_name","Base")
wash.set_editor_property("inputs",[custom_input])
wash.set_editor_property("code",r"""
// Keep the source pixel: soften the palette without filtering neighbouring pixels.
float3 color = Base.rgb;
float lum=dot(color,float3(.2126,.7152,.0722));
// A tiny cool wash in shade leaves text and the image's overall contrast intact.
color *= lerp(float3(.985,1.005,1.025),float3(1.005,1.003,.993),saturate(lum));
return color;
""")
if not ml.connect_material_expressions(scene,"Color",wash,"Base"):raise RuntimeError("Missing postprocess input")
if not ml.connect_material_property(wash,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR):raise RuntimeError("Missing postprocess output")
ml.recompile_material(mat)
if not assets.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError("Cannot save illustrated light")
look=[a for a in actors.get_all_level_actors() if a.get_actor_label()=="EW_Look"]
if len(look)!=1:raise RuntimeError("Expected one art volume")
look[0].add_or_update_blendable(mat,1.0)
if not levels.save_current_level():raise RuntimeError("Cannot save art map")
unreal.log("EW_ILLUSTRATED_LIGHT_READY")
