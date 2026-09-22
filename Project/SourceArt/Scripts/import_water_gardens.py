"""Reimport eight water-design meshes and two owned water shaders only."""
from pathlib import Path
import hashlib,json,runpy,re,unreal

root=Path(__file__).resolve().parents[2];src=root/'SourceArt'
evidence=(Path(__file__).resolve().parents[1] / 'Generated' / 'Water64Evidence')
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
report={'meshes':[],'materials':[]}
revision_match=re.search(r'-EWWaterDesignRevision=(\d+)',unreal.SystemLibrary.get_command_line())
revision=int(revision_match.group(1)) if revision_match else 64
assert not (evidence/f'water{revision}-import.json').exists()
def save(obj):
    assert assets.save_loaded_asset(obj,only_if_is_dirty=False)
def node(mat,kind,x=0,y=0,**props):
    n=ml.create_material_expression(mat,getattr(unreal,kind),x,y)
    for key,value in props.items():n.set_editor_property(key,value)
    return n
def clear_material(mat):
    count=ml.get_num_material_expressions(mat)
    while count:
        ml.delete_all_material_expressions(mat);remaining=ml.get_num_material_expressions(mat)
        assert remaining<count;count=remaining
def link(a,out,b,pin):
    if isinstance(a,unreal.MaterialExpressionVertexColor) and out=='RGB':out=''
    assert ml.connect_material_expressions(a,out,b,pin)
def output(n,pin,prop):
    assert ml.connect_material_property(n,pin,getattr(unreal.MaterialProperty,prop))
def scalar(mat,value,x=0,y=0):return node(mat,'MaterialExpressionConstant',x,y,r=value)
def vector(mat,values,x=0,y=0):return node(mat,'MaterialExpressionConstant3Vector',x,y,constant=unreal.LinearColor(*values,1))
build_material=runpy.run_path(str(Path(__file__).with_name('water_city_materials.py')))['build_material']
for family in ('WaterCitySurface','WaterCityFall'):build_material(family,globals())

catalog=json.loads((src/'kit-catalog.json').read_text())
wanted={x['name'] for x in catalog['assets'] if x['name'].startswith('WaterCity') and x['name'].endswith(('Stone','Fall','Foam'))}
assert len(wanted)==8
for item in catalog['assets']:
    if item['name'] not in wanted:continue
    path='/Game/EndlessWorld/Kit/SM_'+item['name'];mesh=assets.load_asset(path)
    assert mesh and assets.get_metadata_tag(mesh,'EndlessWorld.WaterCity')=='water-city-art-v1'
    old_slots=list(mesh.get_editor_property('static_materials'))
    old_materials=[s.get_editor_property('material_interface').get_path_name() for s in old_slots]
    fbx=src/'Meshes'/item['file'];assert hashlib.sha256(fbx.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    data=opt.static_mesh_import_data
    data.convert_scene_unit=True;data.combine_meshes=True;data.auto_generate_collision=False
    data.generate_lightmap_u_vs=False;data.remove_degenerates=True;data.build_nanite=item['nanite']
    data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(fbx);task.destination_path='/Game/EndlessWorld/Kit'
    task.destination_name='SM_'+item['name'];task.automated=True;task.replace_existing=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1
    mesh=objects[0];mesh.set_editor_property('static_materials',old_slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=item['nanite'];ns.fallback_relative_error=.05
    mesh.set_editor_property('nanite_settings',ns)
    assert [mesh.get_material(i).get_path_name() for i in range(len(old_slots))]==old_materials
    desc=mesh.get_static_mesh_description(0);verts=[]
    for i in range(desc.get_vertex_count()):
        vid=unreal.VertexID(id_value=i)
        if desc.is_vertex_valid(vid):
            v=desc.get_vertex_position(vid);verts.append((v.x,v.y,v.z))
    bounds=[[min(v[i] for v in verts) for i in range(3)],[max(v[i] for v in verts) for i in range(3)]]
    assert all(abs(bounds[j][i]-item['ue_bounds_cm'][j][i])<.05 for i in range(3) for j in range(2)),bounds
    for anchor in item['ue_anchor_vertices_cm']:
        assert min(sum((v[i]-anchor['position'][i])**2 for i in range(3)) for v in verts)<.01
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256'])
    assets.set_metadata_tag(mesh,'EndlessWorld.WaterDesign','framed-falls-and-gardens-v3');save(mesh)
    report['meshes'].append({'asset':path,'source_sha256':item['sha256'],'bounds_cm':bounds,'vertices':len(verts),
                          'source_triangles':item['triangles'],'materials_preserved':old_materials,'nanite':item['nanite']})
report['success']=True
dest=evidence/f'water{revision}-import.json';assert not dest.exists()
dest.write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('EW_WATER_GARDENS_IMPORTED '+str(len(report['meshes'])))
