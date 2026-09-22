"""Reimport only SM_WaterCityWalks, preserving its existing material slots."""
from pathlib import Path
import hashlib,json,unreal

root=Path(__file__).resolve().parents[2]
src=root/'SourceArt'
item=next(x for x in json.loads((src/'kit-catalog.json').read_text())['assets'] if x['name']=='WaterCityWalks')
assets=unreal.EditorAssetLibrary
path='/Game/EndlessWorld/Kit/SM_WaterCityWalks'
mesh=assets.load_asset(path)
assert mesh and assets.get_metadata_tag(mesh,'EndlessWorld.WaterCity')=='water-city-art-v1'
old_slots=list(mesh.get_editor_property('static_materials'))
old_materials=[s.get_editor_property('material_interface').get_path_name() for s in old_slots]
fbx=src/'Meshes'/item['file']
assert hashlib.sha256(fbx.read_bytes()).hexdigest()==item['sha256']
opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
data=opt.static_mesh_import_data
data.convert_scene_unit=True;data.combine_meshes=True;data.auto_generate_collision=False
data.generate_lightmap_u_vs=False;data.remove_degenerates=True;data.build_nanite=True
data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
task=unreal.AssetImportTask();task.filename=str(fbx);task.destination_path='/Game/EndlessWorld/Kit'
task.destination_name='SM_WaterCityWalks';task.automated=True;task.replace_existing=True;task.save=False;task.options=opt
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
objects=task.get_objects();assert len(objects)==1
mesh=objects[0];mesh.set_editor_property('static_materials',old_slots)
ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05
mesh.set_editor_property('nanite_settings',ns)
assert [mesh.get_material(i).get_path_name() for i in range(len(old_slots))]==old_materials
desc=mesh.get_static_mesh_description(0)
verts=[]
for i in range(desc.get_vertex_count()):
    vertex=unreal.VertexID(id_value=i)
    if desc.is_vertex_valid(vertex):
        v=desc.get_vertex_position(vertex);verts.append((v.x,v.y,v.z))
assert verts
bounds=[[min(v[i] for v in verts) for i in range(3)],[max(v[i] for v in verts) for i in range(3)]]
assert all(abs(bounds[j][i]-item['ue_bounds_cm'][j][i])<.05 for i in range(3) for j in range(2)),bounds
for anchor in item['ue_anchor_vertices_cm']:
    assert min(sum((v[i]-anchor['position'][i])**2 for i in range(3)) for v in verts)<.01
assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256'])
assets.set_metadata_tag(mesh,'EndlessWorld.WalkFloor','disjoint-inlay-v2')
assert assets.save_loaded_asset(mesh,only_if_is_dirty=False)
out={'success':True,'asset':path,'source_sha256':item['sha256'],'bounds_cm':bounds,'vertices':len(verts),
     'source_triangles':item['triangles'],'materials_preserved':old_materials,'nanite':True}
dest=(Path(__file__).resolve().parents[1] / 'Generated' / 'Floor62Evidence/walks63-import.json')
assert not dest.exists();dest.write_text(json.dumps(out,indent=2),encoding='utf8')
unreal.log('EW_WALK_FLOOR_IMPORTED '+json.dumps(out))
