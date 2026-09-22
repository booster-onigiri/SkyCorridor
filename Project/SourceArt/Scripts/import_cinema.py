"""Import three authored meshes and the bounded lighting material update."""
from pathlib import Path
import ast, hashlib, json, math, unreal
project=Path(__file__).resolve().parents[2];src=project/'SourceArt'
evidence=(Path(__file__).resolve().parents[1] / 'Generated' / 'Cinema73Evidence')
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
report={'meshes':[],'materials':[],'collections':[]}
helpers=Path(__file__).with_name('import_kit.py');names={'save','node','link','output','scalar','vector'}
defs=[n for n in ast.parse(helpers.read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef) and n.name in names]
exec(compile(ast.Module(body=defs,type_ignores=[]),str(helpers),'exec'),globals())
folder='/Game/EndlessWorld/Materials'
path=folder+'/MPC_DayCycle';collection=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('MPC_DayCycle',folder,unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
assert collection
if not list(collection.get_editor_property('scalar_parameters')):
    parameter=unreal.CollectionScalarParameter();parameter.set_editor_property('parameter_name','CityEmission');parameter.set_editor_property('default_value',1.)
    collection.set_editor_property('scalar_parameters',[parameter])
assert str(collection.get_editor_property('scalar_parameters')[0].get_editor_property('parameter_name'))=='CityEmission'
save(collection);report['collections'].append({'asset':path,'default':1.})
for name in ['Glow','InteriorGlow']:
    path=folder+'/M_'+name;mat=assets.load_asset(path);assert mat
    marker=assets.get_metadata_tag(mat,'EndlessWorld.DayCycle')
    if marker!='cinema73':
        assert marker==''
        original=ml.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR);assert original
        pin=ml.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        scale=node(mat,'MaterialExpressionCollectionParameter',-250,750)
        scale.set_editor_property('collection',collection);scale.set_editor_property('parameter_name','CityEmission')
        multiply=node(mat,'MaterialExpressionMultiply',-80,0);link(original,pin,multiply,'A');link(scale,'',multiply,'B');output(multiply,'','MP_EMISSIVE_COLOR')
        assets.set_metadata_tag(mat,'EndlessWorld.DayCycle','cinema73');ml.recompile_material(mat);save(mat)
    report['materials'].append({'asset':path,'city_emission_parameter':True})
path=folder+'/M_CinemaGlow'
mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_CinemaGlow',folder,unreal.Material,unreal.MaterialFactoryNew())
if assets.get_metadata_tag(mat,'EndlessWorld.Cinema')!='cinema73':
    mat.set_editor_property('used_with_nanite',True);mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    vc=node(mat,'MaterialExpressionVertexColor',-500,0);glow=node(mat,'MaterialExpressionMultiply',-250,0)
    link(vc,'RGB',glow,'A');link(scalar(mat,45),'',glow,'B');output(glow,'','MP_EMISSIVE_COLOR')
    assets.set_metadata_tag(mat,'EndlessWorld.Cinema','cinema73');ml.recompile_material(mat);save(mat)
report['materials'].append({'asset':path,'fixed_auditorium_luminance':45})
catalog=json.loads((src/'kit-catalog.json').read_text(encoding='utf8'));wanted=set(catalog['cinema_art']['assets']+catalog['cinema_art']['guest_assets']);assert len(wanted)==5
for item in catalog['assets']:
    if item['name'] not in wanted:continue
    fbx=src/'Meshes'/item['file'];assert hashlib.sha256(fbx.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.reset_to_fbx_on_material_conflict=True
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    data=opt.static_mesh_import_data;data.convert_scene_unit=True;data.combine_meshes=True;data.reorder_material_to_fbx_order=True
    data.auto_generate_collision=False;data.generate_lightmap_u_vs=False;data.remove_degenerates=True;data.build_nanite=True
    data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(fbx);task.destination_path='/Game/EndlessWorld/Kit';task.destination_name='SM_'+item['name']
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'));bindings=[]
    for index,slot in enumerate(slots):
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        assert family in item['materials'];material=assets.load_asset(folder+'/M_'+family);assert material,family
        slot.set_editor_property('material_interface',material);bindings.append(material.get_path_name())
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3;points=[]
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i));p=(v.x,v.y,v.z);points.append(p)
        for j in range(3):lo[j]=min(lo[j],p[j]);hi[j]=max(hi[j],p[j])
    assert all(abs([lo,hi][j][i]-item['ue_bounds_cm'][j][i])<.15 for j in range(2) for i in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    for anchor in item['ue_anchor_vertices_cm']:
        assert any(sum((p[j]-anchor['position'][j])**2 for j in range(3))<.04 for p in points),(item['name'],anchor)
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);save(mesh)
    report['meshes'].append({'asset':'/Game/EndlessWorld/Kit/SM_'+item['name'],'triangles':item['triangles'],'source_sha256':item['sha256'],'bounds_cm':[lo,hi],'anchors_pass':True,'material_bindings':bindings})
report['success']=True;(evidence/'import73.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('EW_CINEMA_IMPORTED '+str(len(report['meshes'])))
