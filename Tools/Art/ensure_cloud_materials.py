"""Recreate excluded engine-derived cloud assets from the installed Unreal Engine.

Run on the exported project using Unreal's PythonScript commandlet. Existing
assets are inspected and preserved. This script contains no Epic shader source.
"""
from pathlib import Path
import hashlib
import json
import unreal

REPO = Path(__file__).resolve().parents[2]
PROJECT = REPO / 'Project'
assert Path(unreal.Paths.project_dir()).resolve() == PROJECT.resolve(), 'Wrong project'
ART = PROJECT / 'SourceArt'
recipe = json.loads((ART / 'cloud-material-recipe.json').read_text(encoding='utf-8'))
mask_path = ART / recipe['custom_code']
assert hashlib.sha256(mask_path.read_bytes()).hexdigest() == recipe['custom_code_sha256']
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
created = []
mat = assets.load_asset(recipe['material']) if assets.does_asset_exist(recipe['material']) else None
if mat is None:
    base = unreal.load_object(None, recipe['engine_material'])
    assert base, 'Install matching UE engine content: ' + recipe['engine_material']
    folder, name = recipe['material'].rsplit('/', 1)
    mat = tools.duplicate_asset(name, folder, base)
    assert mat
    extinction = materials.get_material_property_input_node(mat, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    assert extinction, 'Engine cloud material contract differs from UE 5.8'
    output_name = materials.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mask = materials.create_material_expression(mat, unreal.MaterialExpressionCustom, 100, 0)
    mask.set_editor_property('desc', 'EW_CloudLayerMask')
    inputs = []
    for name in recipe['custom_inputs']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    mask.set_editor_property('inputs', inputs)
    mask.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    mask.set_editor_property('code', mask_path.read_text(encoding='utf-8'))
    position = materials.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -200, 500)
    assert materials.connect_material_expressions(extinction, output_name, mask, 'Base')
    assert materials.connect_material_expressions(position, '', mask, 'Position')
    assert materials.connect_material_property(mask, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    materials.recompile_material(mat)
    assets.set_metadata_tag(mat, 'SkyCorridor.SourcePolicy', 'engine-generated-not-source-archive')
    assert assets.save_loaded_asset(mat, only_if_is_dirty=False)
    created.append(recipe['material'])

instance = assets.load_asset(recipe['instance']) if assets.does_asset_exist(recipe['instance']) else None
if instance is None:
    base = unreal.load_object(None, recipe['engine_instance'])
    assert base, 'Install matching UE engine content: ' + recipe['engine_instance']
    folder, name = recipe['instance'].rsplit('/', 1)
    instance = tools.duplicate_asset(name, folder, base)
    assert instance
    materials.set_material_instance_parent(instance, mat)
    for name, value in recipe['parameters'].items():
        materials.set_material_instance_scalar_parameter_value(instance, name, value)
    materials.update_material_instance(instance)
    assets.set_metadata_tag(instance, 'SkyCorridor.SourcePolicy', 'engine-generated-not-source-archive')
    assert assets.save_loaded_asset(instance, only_if_is_dirty=False)
    created.append(recipe['instance'])

measured = {name: materials.get_material_instance_scalar_parameter_value(instance, name)
            for name in recipe['parameters']}
for name, value in measured.items():
    if recipe['instance'] in created:
        assert abs(value - recipe['parameters'][name]) < 1e-5, (name, value)
output = ART / 'Generated'
output.mkdir(exist_ok=True)
(output / 'cloud-materials-setup.json').write_text(json.dumps({
    'success': True, 'created': created, 'existing_assets_modified': False,
    'measured_parameters': measured, 'recipe_parameters': recipe['parameters'],
    'recipe_match': all(abs(measured[n] - v) < 1e-5 for n, v in recipe['parameters'].items()),
    'source_archive_exclusions': recipe['source_archive_exclusions'],
    'engine_source_distributed': False,
}, indent=2), encoding='utf-8')
unreal.log('PUBLIC_CLOUD_MATERIALS_READY ' + str(created))
