from pathlib import Path
import json
script=Path(__file__).with_name('import_exploration85.py')
# Reapply the material alone; the eighteen geometry imports remain unchanged.
exec(compile(script.read_text(encoding='utf8').split('report=[]')[0],str(script),'exec'))
node=ml.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
assert node==vc
(P.parent/'Evidence/exploration85-steel-material.json').write_text(json.dumps({'success':True,'base_colour_connected':True,'nanite':mat.get_editor_property('used_with_nanite'),'metallic':1.0,'roughness':.32}),encoding='utf8')
print('EXPLORATION85_STEEL_VERIFIED')
