from pathlib import Path
import unreal,json,hashlib,math,runpy,re
P=Path(__file__).resolve().parents[2];S=P/'SourceArt'
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/EndlessWorld/Materials';ml=unreal.MaterialEditingLibrary

def node(m,cls,**values):
    n=ml.create_material_expression(m,getattr(unreal,'MaterialExpression'+cls))
    for k,v in values.items():n.set_editor_property(k,v)
    return n
def scalar(m,v):return node(m,'Constant',r=v)
def link(a,b,pin,output=''):
    names=list(ml.get_material_expression_input_names(b))
    if pin=='Input' and pin not in names and len(names)==1:pin=names[0]
    assert ml.connect_material_expressions(a,output,b,pin),(b.get_class().get_name(),pin,names)
def output(n,prop,pin=''):assert ml.connect_material_property(n,pin,getattr(unreal.MaterialProperty,'MP_'+prop))
def op(m,cls,a,b):
    n=node(m,cls);names=list(ml.get_material_expression_input_names(n))
    link(a,n,names[0] if cls=='Power' else 'A');link(b,n,names[1] if cls=='Power' else 'B');return n

collection=assets.load_asset(folder+'/MPC_DayCycle');assert collection
parameters=list(collection.get_editor_property('scalar_parameters'))
previous_parameters=[str(v.get_editor_property('parameter_name')) for v in parameters]
if 'CityNightEmission' not in previous_parameters:
    assert parameters,'existing day-cycle scalar required'
    value=type(parameters[0])();value.set_editor_property('parameter_name','CityNightEmission');value.set_editor_property('default_value',0)
    parameters.append(value);collection.set_editor_property('scalar_parameters',parameters);assert assets.save_loaded_asset(collection)
materials=[]
for family in ['Stone','Edge','White','Metal','Window','Glass','Copper','Roof','Tile','Wood','Leaf','Cloth','Dark','PhoneMetal','Night']:
    name='M_Sky92'+family;path=folder+'/'+name
    exists=assets.does_asset_exist(path)
    m=assets.load_asset(path) if exists else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    if exists:assert assets.get_metadata_tag(m,'EndlessWorld.Sky92')=='v1',('unowned material',path)
    count=ml.get_num_material_expressions(m)
    while count:
        ml.delete_all_material_expressions(m);remaining=ml.get_num_material_expressions(m)
        assert remaining<count;count=remaining
    glass=family=='Glass'
    m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('used_with_nanite',not glass)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if glass else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided',glass)
    vc=op(m,'Power',node(m,'VertexColor'),scalar(m,2.2));uv=node(m,'TextureCoordinate');base=vc
    if family in ['Stone','Edge','White']:
        tex=assets.load_asset('/Game/EndlessWorld/Textures/T_Limestone_Base');assert tex
        sample=node(m,'TextureSample',texture=tex,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        link(op(m,'Multiply',uv,scalar(m,.55)),sample,'UVs')
        # Low-amplitude stone grain. Material contrast never replaces geometric shade.
        grain=op(m,'Add',scalar(m,.74),op(m,'Multiply',sample,scalar(m,.26)))
        base=op(m,'Multiply',vc,grain)
    output(base,'BASE_COLOR')
    rough={'Metal':.29,'Window':.17,'Roof':.32,'Glass':.13,'Copper':.34,'PhoneMetal':.3,'Dark':.25,'Wood':.68}.get(family,.65)
    metal={'Metal':.8,'Window':.65,'Roof':.55,'Copper':.83,'PhoneMetal':.78}.get(family,0)
    output(scalar(m,rough),'ROUGHNESS');output(scalar(m,metal),'METALLIC');output(scalar(m,.35 if family=='Window' else .28),'SPECULAR')
    if glass:
        m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        fr=node(m,'Fresnel',exponent=4,base_reflect_fraction=.04)
        output(op(m,'Add',scalar(m,.30),op(m,'Multiply',fr,scalar(m,.50))),'OPACITY')
    if family=='Night':
        night=node(m,'CollectionParameter',collection=collection,parameter_name='CityNightEmission')
        warm=node(m,'Constant3Vector',constant=unreal.LinearColor(1.,.57,.25,1))
        output(op(m,'Multiply',warm,op(m,'Multiply',night,scalar(m,2200))),'EMISSIVE_COLOR')
    ml.recompile_material(m);assets.set_metadata_tag(m,'EndlessWorld.Sky92','v1');assert assets.save_loaded_asset(m)
    materials.append(name)

report=[];catalog=json.loads((S/'sky92-catalog.json').read_text(encoding='utf8'))
for item in catalog['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True
    nanite='Sky92Glass' not in item['used_materials'];d.build_nanite=nanite
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    t=unreal.AssetImportTask();t.filename=str(f);t.destination_path='/Game/EndlessWorld/Kit';t.destination_name='SM_'+item['name']
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=False;t.options=opt
    tools.import_asset_tasks([t]);objects=t.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['used_materials']:family='Sky92Stone'
        mat=assets.load_asset(folder+'/M_'+family);assert mat
        if nanite:assert mat.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_OPAQUE
        slot.set_editor_property('material_interface',mat)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=nanite;ns.fallback_relative_error=0;ns.fallback_percent_triangles=1.0;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.15 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assert assets.save_loaded_asset(mesh)
    report.append(dict(mesh=item['name'],triangles=item['triangles'],bounds_cm=[lo,hi],sha256=item['sha256'],nanite=nanite,used_materials=item['used_materials']))
match=re.search(r'EWImportReport=(?:"([^"]+)"|(\S+))',unreal.SystemLibrary.get_command_line())
assert match,'EWImportReport required'
out=Path(match.group(1) or match.group(2))
assert not out.exists();out.write_text(json.dumps(dict(success=True,assets=report,materials=materials),indent=2),encoding='utf8')
unreal.log('EW_SKY92_IMPORTED '+str(len(report)))
