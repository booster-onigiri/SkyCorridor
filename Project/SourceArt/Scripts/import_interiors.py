"""Import the bounded interior kit with verified coordinates and named finish bindings."""
from pathlib import Path
import ast, hashlib, json, math, re, unreal

project=Path(__file__).resolve().parents[2];src=project/'SourceArt'
evidence=(Path(__file__).resolve().parents[1] / 'Generated' / 'Interiors70Evidence')
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
report={'meshes':[],'materials':[]}
selection=re.search(r'-EWInteriorOnly=([A-Za-z0-9_,]+)',unreal.SystemLibrary.get_command_line())
selector=selection.group(1) if selection else 'all' if '-EWInteriorAllMeshes' in unreal.SystemLibrary.get_command_line() else None
selected=set(selector.split(',')) if selector else None
materials_only='-EWInteriorMaterialsOnly' in unreal.SystemLibrary.get_command_line()
helpers=Path(__file__).with_name('import_kit.py')
names={'save','node','clear_material','link','output','scalar','vector','aligned'}
defs=[n for n in ast.parse(helpers.read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef) and n.name in names]
assert {n.name for n in defs}==names
exec(compile(ast.Module(body=defs,type_ignores=[]),str(helpers),'exec'),globals())

for family,texture_family,clean,normal,rough in [('InteriorOak','Timber',.76,.055,.47),
        ('InteriorPlaster','Plaster',.85,.04,.78),('InteriorFabric','Cloth',.76,.075,.86),
        ('InteriorCeramic','Ceramic',.90,.02,.24),('InteriorGlow',None,1,0,.5),
        ('InteriorBrass','Ceramic',.98,0,.30),('InteriorLeaf','Cloth',.96,.02,.62),
        ('InteriorStem','Timber',.80,.03,.78),('InteriorPaper','Cloth',.96,0,.85)]:
    if selected and '-EWInteriorUpdateFinishes' not in unreal.SystemLibrary.get_command_line():continue
    path='/Game/EndlessWorld/Materials/M_'+family
    mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_'+family,'/Game/EndlessWorld/Materials',unreal.Material,unreal.MaterialFactoryNew())
    assert mat
    marker=assets.get_metadata_tag(mat,'EndlessWorld.Interiors')
    assert marker in ('','interiors-v1','interiors-v2')
    clear_material(mat);mat.set_editor_property('used_with_nanite',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('tangent_space_normal',False)
    vc=node(mat,'MaterialExpressionVertexColor',-900,0)
    if texture_family:
        tex=assets.load_asset('/Game/EndlessWorld/Textures/T_'+texture_family+'_Base');assert tex
        base=aligned(mat,tex);mix=node(mat,'MaterialExpressionLinearInterpolate',-600,0)
        link(base,'XYZ Texture',mix,'A');link(vector(mat,(1,1,1)),'',mix,'B');link(scalar(mat,clean),'',mix,'Alpha')
        colour=node(mat,'MaterialExpressionMultiply',-300,0);link(vc,'RGB',colour,'A');link(mix,'',colour,'B')
        output(colour,'','MP_BASE_COLOR')
        texn=assets.load_asset('/Game/EndlessWorld/Textures/T_'+texture_family+'_Normal');assert texn
        n=aligned(mat,texn,normal=True,y=400)
        pin=next(str(p) for p in ml.get_material_expression_input_names(n) if str(p).startswith('WorldSpace'))
        link(node(mat,'MaterialExpressionStaticBool',value=True),'',n,pin)
        blend=node(mat,'MaterialExpressionLinearInterpolate',-400,400)
        link(node(mat,'MaterialExpressionVertexNormalWS'),'',blend,'A')
        link(n,'XYZ Texture',blend,'B');link(scalar(mat,normal),'',blend,'Alpha')
        normalize=node(mat,'MaterialExpressionNormalize',-150,400);link(blend,'',normalize,'');output(normalize,'','MP_NORMAL')
    else:
        glow=node(mat,'MaterialExpressionMultiply',-300,0);link(vc,'RGB',glow,'A');link(scalar(mat,6000),'',glow,'B');output(glow,'','MP_EMISSIVE_COLOR')
        output(vector(mat,(.1,.1,.1)),'','MP_BASE_COLOR')
    output(scalar(mat,rough),'','MP_ROUGHNESS');output(scalar(mat,.68 if family=='InteriorBrass' else 0),'','MP_METALLIC')
    if family=='InteriorLeaf':
        mat.set_editor_property('two_sided',True)
        models=[getattr(unreal.MaterialShadingModel,n) for n in dir(unreal.MaterialShadingModel) if 'FOLIAGE' in n]
        assert len(models)==1
        mat.set_editor_property('shading_model',models[0]);output(vc,'RGB','MP_SUBSURFACE_COLOR')
    assets.set_metadata_tag(mat,'EndlessWorld.Interiors','interiors-v2');ml.recompile_material(mat);save(mat)
    report['materials'].append({'asset':path,'roughness':rough,'normal_blend':normal})

catalog=json.loads((src/'kit-catalog.json').read_text(encoding='utf8'))
wanted=set(catalog['interior_art']['assets']);assert len(wanted)==29
if selector=='all':selected=wanted
for item in catalog['assets']:
    if materials_only or item['name'] not in wanted or selected and item['name'] not in selected:continue
    fbx=src/'Meshes'/item['file'];assert hashlib.sha256(fbx.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.reset_to_fbx_on_material_conflict=True
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    data=opt.static_mesh_import_data;data.convert_scene_unit=True;data.combine_meshes=True
    data.reorder_material_to_fbx_order=True
    data.auto_generate_collision=False;data.generate_lightmap_u_vs=False;data.remove_degenerates=True;data.build_nanite=True
    data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(fbx);task.destination_path='/Game/EndlessWorld/Kit'
    task.destination_name='SM_'+item['name'];task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1
    mesh=objects[0]
    # FBX keeps section-to-slot identity by name; its actual slot order is not
    # the catalogue's global material palette (which also has unused entries).
    slots=list(mesh.get_editor_property('static_materials'));bindings=[]
    for index,slot in enumerate(slots):
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        assert family in item['materials'],(item['name'],index,family)
        if item['name'].startswith('Interior'):
            assert family not in {'Timber','Paint','Cloth','Ceramic','Glow','Bronze','Foliage','Bark','Paper'},(item['name'],'stale material remapping',family)
        else:assert not family.startswith('Interior'),(item['name'],'unexpected interior finish',family)
        mat=assets.load_asset('/Game/EndlessWorld/Materials/M_'+family);assert mat,family
        slot.set_editor_property('material_interface',mat)
        bindings.append({'slot':index,'imported_name':str(slot.get_editor_property('imported_material_slot_name')),'material':mat.get_path_name()})
    mesh.set_editor_property('static_materials',slots)
    assert all(mesh.get_material(i)==slot.get_editor_property('material_interface') for i,slot in enumerate(slots))
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    anchors=item['ue_anchor_vertices_cm'];found=[False]*len(anchors)
    cells={}
    for i,a in enumerate(anchors):
        cell=[math.floor(v) for v in a['position']]
        for dx in [-1,0,1]:
            for dy in [-1,0,1]:
                for dz in [-1,0,1]:cells.setdefault((cell[0]+dx,cell[1]+dy,cell[2]+dz),[]).append(i)
    for i in range(desc.get_vertex_count()):
        vid=unreal.VertexID(id_value=i);assert desc.is_vertex_valid(vid)
        v=desc.get_vertex_position(vid);p=(v.x,v.y,v.z)
        for j in range(3):lo[j]=min(lo[j],p[j]);hi[j]=max(hi[j],p[j])
        for a in cells.get(tuple(math.floor(v) for v in p),[]):
            if sum((p[j]-anchors[a]['position'][j])**2 for j in range(3))<.04:found[a]=True
    assert all(found),(item['name'],'anchor',found)
    assert all(abs([lo,hi][j][i]-item['ue_bounds_cm'][j][i])<.1 for j in range(2) for i in range(3)),item['name']
    assets.set_metadata_tag(mesh,'EndlessWorld.Interiors','interiors-v2')
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);save(mesh)
    report['meshes'].append({'asset':'/Game/EndlessWorld/Kit/SM_'+item['name'],'vertices':desc.get_vertex_count(),
         'triangles':item['triangles'],'source_sha256':item['sha256'],'bounds_cm':[lo,hi],'anchors_pass':all(found),'material_bindings':bindings})
report['success']=True
(evidence/(('reimport70-'+selector.replace(',','_')+'.json') if selected else 'materials70.json' if materials_only else 'import70.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('EW_INTERIORS_IMPORTED '+str(len(report['meshes'])))
