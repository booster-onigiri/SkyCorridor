"""Create the independent dusk-switched fixture material; shared windows stay intact."""
from pathlib import Path
import json,unreal
assets=unreal.EditorAssetLibrary
path='/Game/EndlessWorld/Materials/M_NightFixtureGlow'
if assets.does_asset_exist(path):
    raise RuntimeError('Night material exists; preserve it rather than silently rebuilding.')
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_NightFixtureGlow','/Game/EndlessWorld/Materials',unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('used_with_instanced_static_meshes',True)
ml=unreal.MaterialEditingLibrary
vc=ml.create_material_expression(mat,unreal.MaterialExpressionVertexColor,-600,0)
power=ml.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-600,180)
power.set_editor_property('parameter_name','Power');power.set_editor_property('default_value',0.)
mul=ml.create_material_expression(mat,unreal.MaterialExpressionMultiply,-240,80)
ml.connect_material_expressions(vc,'RGB',mul,'A');ml.connect_material_expressions(power,'',mul,'B')
ml.connect_material_property(vc,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
ml.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ml.recompile_material(mat)
assert assets.save_loaded_asset(mat,only_if_is_dirty=False)
print('EW_NIGHT_MATERIAL_READY '+path)
