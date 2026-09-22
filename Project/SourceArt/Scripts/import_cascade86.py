from pathlib import Path
import unreal,json,hashlib,math,runpy
P=Path(__file__).resolve().parents[2];S=P/'SourceArt';assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary;folder='/Game/EndlessWorld/Materials'
report={'materials':[],'assets':[]}
def save(obj):
    assert assets.save_loaded_asset(obj),obj.get_path_name()
def node(mat,kind,x=0,y=0,**props):
    n=ml.create_material_expression(mat,getattr(unreal,kind),x,y)
    for name,value in props.items():n.set_editor_property(name,value)
    return n
def clear_material(mat):
    # UE 5.7 removes elements while iterating a view of the same expression
    # array. One DeleteAll call can leave nodes behind. Verify a strictly
    # shrinking count until empty, before authoring any new output nodes.
    count=ml.get_num_material_expressions(mat)
    initial=count;passes=0
    while count:
        ml.delete_all_material_expressions(mat);passes+=1
        remaining=ml.get_num_material_expressions(mat)
        if remaining>=count:raise RuntimeError("Material graph did not clear: "+mat.get_path_name())
        count=remaining
    report.setdefault("cleared_material_graphs",[]).append({"asset":mat.get_path_name(),
        "previous_nodes":initial,"passes":passes,"remaining_nodes":count})
def link(a,out,b,pin):
    if isinstance(a,unreal.MaterialExpressionVertexColor) and out=="RGB":out=""
    if not ml.connect_material_expressions(a,out,b,pin):
        raise RuntimeError("Cannot connect "+a.get_class().get_name()+"."+out+" -> "+pin)
def output(n,pin,prop):
    if isinstance(n,unreal.MaterialExpressionVertexColor) and pin=="RGB":pin=""
    if not ml.connect_material_property(n,pin,getattr(unreal.MaterialProperty,prop)):
        raise RuntimeError("Cannot connect material property "+prop)
def scalar(mat,value,x=0,y=0):
    return node(mat,"MaterialExpressionConstant",x,y,r=value)
def vector(mat,values,x=0,y=0):
    return node(mat,"MaterialExpressionConstant3Vector",x,y,constant=unreal.LinearColor(*values,1))
def aligned(mat,texture,normal=False,x=-1000,y=0):
    call=node(mat,"MaterialExpressionMaterialFunctionCall",x+250,y)
    path="/Engine/Functions/Engine_MaterialFunctions01/Texturing/"+("WorldAlignedNormal" if normal else "WorldAlignedTexture")
    function=unreal.load_object(None,path)
    if not function or not call.set_material_function(function):
        raise RuntimeError("World-aligned material function is unavailable: "+path)
    names=ml.get_material_expression_input_names(call)
    unreal.log("EW_FUNCTION_INPUTS "+path+" "+str(names))
    obj=node(mat,"MaterialExpressionTextureObject",x,y,texture=texture)
    # Input names contain explanatory type suffixes in some engine builds.
    tex_pin=next(str(n) for n in names if str(n).startswith("TextureObject"))
    size_pin=next(str(n) for n in names if str(n).startswith("TextureSize"))
    link(obj,"",call,tex_pin);link(vector(mat,(120,120,120),x,y+150),"",call,size_pin)
    return call


shader=runpy.run_path(str(Path(__file__).with_name('cascade86_materials.py')))
for family in shader['FAMILIES']:shader['build_material'](family,globals())
name='M_Cascade86Leaf';path=folder+'/'+name
mat=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
clear_material(mat);mat.set_editor_property('used_with_nanite',True);mat.set_editor_property('used_with_instanced_static_meshes',True);mat.set_editor_property('two_sided',True)
output(node(mat,'MaterialExpressionVertexColor'),'RGB','MP_BASE_COLOR')
output(scalar(mat,.78),'','MP_ROUGHNESS');output(scalar(mat,.18),'','MP_SPECULAR')
ml.recompile_material(mat);save(mat);report['materials'].append(name)
for item in json.loads((S/'cascade86-catalog.json').read_text(encoding="utf8"))['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True
    nanite=item['name'] not in ('Cascade86Water','Cascade86SourceWater','Cascade86Fall36','Cascade86Fall60','Cascade86Weir','Cascade86Foam');d.build_nanite=nanite
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
    report['assets'].append({'mesh':item['name'],'triangles':item['triangles'],'bounds_cm':[lo,hi],'sha256':item['sha256'],'nanite':nanite})
report['success']=True
(P.parent/'Evidence/cascade86-import-02.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('EW_CASCADE86_IMPORTED '+str(len(report['assets'])))
