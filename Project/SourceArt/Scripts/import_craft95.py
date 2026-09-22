"""Import only the five additive Craft95 meshes, reusing existing finishes.

UE commandlet: -ExecutePythonScript=<this file> -EWImportReport=<fresh JSON>
No levels, old meshes, material graphs, lighting or collision are modified.
Use a fresh report path. Existing Craft95 meshes are replaceable only when
their stored source SHA matches this manifest, so reruns are reproducible.
"""
from pathlib import Path
import hashlib, json, math, re
import unreal

P=Path(__file__).resolve().parents[2];S=P/'SourceArt'
cmd=unreal.SystemLibrary.get_command_line()
match=re.search(r'-EWImportReport=(?:"([^"]+)"|(\S+))',cmd)
assert match, 'Provide -EWImportReport=<new report path>'
report_path=Path(match.group(1) or match.group(2))
assert not report_path.exists(), 'Preserve previous import evidence: '+str(report_path)
manifest=json.loads((S/'craft95-assets.json').read_text(encoding='utf8'))
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/EndlessWorld/Kit'
report={'revision':95,'success':False,'meshes':[],'scope':'Only additive Craft95 meshes; existing material assets reused.'}
for item in manifest['assets']:
    assert item['name'].startswith('Craft95')
    source=S/'Meshes'/item['file']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==item['sha256'],source
    destination=folder+'/SM_'+item['name']
    if assets.does_asset_exist(destination):
        existing=assets.load_asset(destination)
        previous=assets.get_metadata_tag(existing,'EndlessWorld.SourceSHA256')
        assert previous==item['sha256'], 'Unexpected Craft95 destination; preserve and review before replacing: '+destination
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True;d.build_nanite=True
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=folder;task.destination_name='SM_'+item['name']
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1
    mesh=objects[0];assert isinstance(mesh,unreal.StaticMesh)
    slots=list(mesh.get_editor_property('static_materials'));bindings=[]
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        assert family in item['materials'],(item['name'],family)
        target=item['material_paths'].get(family)
        if not target:
            q='/Game/EndlessWorld/Materials/Quality93Final/M_'+family
            target=q if assets.does_asset_exist(q) else '/Game/EndlessWorld/Materials/M_'+family
        material=assets.load_asset(target);assert material,(item['name'],family,target)
        slot.set_editor_property('material_interface',material);bindings.append({'family':family,'asset':target})
    mesh.set_editor_property('static_materials',slots)
    settings=mesh.get_editor_property('nanite_settings');settings.enabled=True;settings.fallback_relative_error=.05
    mesh.set_editor_property('nanite_settings',settings)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    assert desc.get_vertex_count()>0
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,value in enumerate([v.x,v.y,v.z]):
            assert math.isfinite(value),(item['name'],i)
            lo[j]=min(lo[j],value);hi[j]=max(hi[j],value)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.2 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256'])
    assets.set_metadata_tag(mesh,'EndlessWorld.Craft95','original-foreground-kit')
    assert assets.save_loaded_asset(mesh)
    report['meshes'].append({'name':item['name'],'asset':destination,'sha256':item['sha256'],
        'source_triangles':item['triangles'],'bounds_cm':[lo,hi],'materials':bindings,'nanite':True,'automatic_collision':False})
    unreal.log('CRAFT95_IMPORTED '+item['name'])
    del mesh,desc,objects,slots,task
    unreal.SystemLibrary.collect_garbage()
report['success']=True;report_path.parent.mkdir(parents=True,exist_ok=True)
report_path.write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CRAFT95_IMPORT_COMPLETE '+str(len(report['meshes'])))
