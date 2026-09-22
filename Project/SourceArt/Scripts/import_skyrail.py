from pathlib import Path
import unreal, json, hashlib, math
P=Path(__file__).resolve().parents[2];S=P/'SourceArt';assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
folder='/Game/EndlessWorld/Materials';path=folder+'/M_RailGlass'
mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_RailGlass',folder,unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('two_sided',True);mat.set_editor_property('used_with_instanced_static_meshes',True)
mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
def const(value,prop):
    n=ml.create_material_expression(mat,unreal.MaterialExpressionConstant);n.set_editor_property('r',value)
    ml.connect_material_property(n,'',prop)
vc=ml.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
ml.connect_material_property(vc,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
const(.095,unreal.MaterialProperty.MP_OPACITY);const(.16,unreal.MaterialProperty.MP_ROUGHNESS);const(.48,unreal.MaterialProperty.MP_SPECULAR)
ml.recompile_material(mat);assets.save_loaded_asset(mat)
report=[]
for item in json.loads((S/'skyrail-catalog.json').read_text())['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True
    nanite=item['name'] not in ['SkyrailGlass','SkyrailDoor'];d.build_nanite=nanite
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    t=unreal.AssetImportTask();t.filename=str(f);t.destination_path='/Game/EndlessWorld/Kit';t.destination_name='SM_'+item['name']
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=False;t.options=opt
    tools.import_asset_tasks([t]);objects=t.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        material=assets.load_asset(folder+'/M_'+family);assert material,family;slot.set_editor_property('material_interface',material)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=nanite;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.15 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assets.save_loaded_asset(mesh)
    report.append({'mesh':item['name'],'triangles':item['triangles'],'bounds_cm':[lo,hi],'sha256':item['sha256'],'nanite':nanite})
(P.parent/'Evidence/train78-import.json').write_text(json.dumps({'success':True,'assets':report},indent=2),encoding='utf8')
unreal.log('EW_SKYRAIL_IMPORTED '+str(len(report)))
