"""Create only the new local fishing materials in the isolated development project."""
import unreal
from pathlib import Path
import json

project = Path(unreal.Paths.project_dir()).resolve()
expected = Path(__file__).resolve().parents[2]
if project != expected.resolve():
    raise RuntimeError('Fishing assets must be generated in social79/Project only.')
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
factory = unreal.AssetToolsHelpers.get_asset_tools()
created = []
for name, glass in [('M_LifeSurface', False), ('M_LifeGlass', True)]:
    path = '/Game/EndlessWorld/Materials/' + name
    if assets.does_asset_exist(path):
        raise RuntimeError('Material already exists; preserve it and inspect the prior run.')
    material = factory.create_asset(name, '/Game/EndlessWorld/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if glass:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property('two_sided', True)
    colour = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property('parameter_name', 'Tint')
    colour.set_editor_property('default_value', unreal.LinearColor(.50, .77, .76, 1))
    if not editing.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError('Colour connection failed.')
    for prop, value, y in [(unreal.MaterialProperty.MP_ROUGHNESS, .20 if glass else .34, 170),
                           (unreal.MaterialProperty.MP_METALLIC, 0 if glass else .12, 290),
                           (unreal.MaterialProperty.MP_OPACITY, .08 if glass else 1, 410)]:
        node = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y)
        node.set_editor_property('r', value)
        if not editing.connect_material_property(node, '', prop):
            raise RuntimeError('Material connection failed.')
    editing.recompile_material(material)
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Material save failed.')
    created.append(path)
receipt = Path(__file__).resolve().parents[1] / 'Generated' / 'Evidence' / 'fishing-materials.json'
with receipt.open('x', encoding='utf-8') as stream:
    json.dump({'project': str(project), 'created': created}, stream, indent=2)
unreal.log('EW_FISHING_MATERIALS_READY ' + json.dumps(created))
