from pathlib import Path
import unreal,json,hashlib,math
P=Path(__file__).resolve().parents[2];S=P/"SourceArt";assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools();folder="/Game/EndlessWorld/Materials"
report=[]
for item in json.loads((S/'city82-catalog.json').read_text())['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True
    nanite=item['name'] not in ['Skyrail82CabGlass','Airship82Glass','Airship82Prop'];d.build_nanite=nanite
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    t=unreal.AssetImportTask();t.filename=str(f);t.destination_path='/Game/EndlessWorld/Kit';t.destination_name='SM_'+item['name']
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=False;t.options=opt
    tools.import_asset_tasks([t]);objects=t.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        material=assets.load_asset(folder+'/M_'+('NightFixtureGlow' if family=='Glow' else family));assert material,family;slot.set_editor_property('material_interface',material)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=nanite;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.15 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assets.save_loaded_asset(mesh)
    report.append({'mesh':item['name'],'triangles':item['triangles'],'bounds_cm':[lo,hi],'sha256':item['sha256'],'nanite':nanite})
(P.parent/'Evidence/city82-import-01.json').write_text(json.dumps({'success':True,'assets':report},indent=2),encoding='utf8')
unreal.log('EW_SKYRAIL_IMPORTED '+str(len(report)))
