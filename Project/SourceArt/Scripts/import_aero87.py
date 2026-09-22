from pathlib import Path
import unreal, json, hashlib, math
P=Path(__file__).resolve().parents[2];S=P/'SourceArt';assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools();folder='/Game/EndlessWorld/Materials';ml=unreal.MaterialEditingLibrary
def node(m,cls,**values):
    n=ml.create_material_expression(m,getattr(unreal,'MaterialExpression'+cls))
    for k,v in values.items():n.set_editor_property(k,v)
    return n
def scalar(m,v):return node(m,'Constant',r=v)
def colour(m,v):return node(m,'Constant3Vector',constant=unreal.LinearColor(*v,1))
def link(a,b,input,output=''):
    names=ml.get_material_expression_input_names(b)
    if input=='Input' and input not in names and len(names)==1:input=names[0]
    assert ml.connect_material_expressions(a,output,b,input),(b.get_class().get_name(),input,names)
def output(a,prop,pin=''):assert ml.connect_material_property(a,pin,getattr(unreal.MaterialProperty,'MP_'+prop))
def op(m,cls,a,b):
    n=node(m,cls);names=list(ml.get_material_expression_input_names(n))
    link(a,n,names[0] if cls=='Power' else 'A');link(b,n,names[1] if cls=='Power' else 'B');return n
materials=[]
for family in ['Hull','Metal','Teak','Fabric','Glass','Pool','Glow']:
    name='M_Aero87'+family
    m=assets.load_asset(folder+'/'+name) if assets.does_asset_exist(folder+'/'+name) else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    count=ml.get_num_material_expressions(m)
    while count:
        ml.delete_all_material_expressions(m)
        remaining=ml.get_num_material_expressions(m)
        assert remaining<count,('material graph did not clear',name)
        count=remaining
    m.set_editor_property('used_with_instanced_static_meshes',True)
    m.set_editor_property('used_with_nanite',family not in ['Glass','Pool'])
    # StaticMeshBuilder writes vertex colours with ToFColor(true); decode those
    # sRGB bytes for linear PBR, especially the dark bow and warm teak.
    vc=op(m,'Power',node(m,'VertexColor'),scalar(m,2.2));uv=node(m,'TextureCoordinate');base=vc
    if family in ['Teak','Fabric']:
        noise=node(m,'Noise',scale=1,quality=1,levels=2,output_min=.86,output_max=1)
        uvscale=node(m,'Multiply');link(uv,uvscale,'A');link(node(m,'Constant2Vector',r=1.0,g=18.0 if family=='Teak' else 1.0),uvscale,'B')
        pos=node(m,'AppendVector');link(uvscale,pos,'A');link(scalar(m,0),pos,'B');link(pos,noise,ml.get_material_expression_input_names(noise)[0])
        base=op(m,'Multiply',vc,noise)
    if family=='Glass':
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
        m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        output(colour(m,(.018,.09,.13)),'BASE_COLOR');output(scalar(m,.10),'ROUGHNESS');output(scalar(m,.5),'SPECULAR')
        fr=node(m,'Fresnel',exponent=4,base_reflect_fraction=.08)
        output(op(m,'Add',scalar(m,.38),op(m,'Multiply',fr,scalar(m,.44))),'OPACITY')
    elif family=='Pool':
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
        m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        output(colour(m,(.028,.23,.34)),'BASE_COLOR');output(scalar(m,.105),'ROUGHNESS');output(scalar(m,.55),'SPECULAR');output(scalar(m,.60),'OPACITY')
        time=node(m,'Time');phase=op(m,'Add',op(m,'Multiply',uv,scalar(m,2.5)),op(m,'Multiply',time,scalar(m,.55)))
        mask=node(m,'ComponentMask',r=True,g=False,b=False,a=False);link(phase,mask,'Input')
        sine=node(m,'Sine',period=6.283185);link(mask,sine,'Input')
        n=node(m,'AppendVector');link(op(m,'Multiply',sine,scalar(m,.035)),n,'A');link(scalar(m,0),n,'B')
        normal=node(m,'AppendVector');link(n,normal,'A');link(scalar(m,1),normal,'B');output(normal,'NORMAL')
    elif family=='Glow':
        output(vc,'BASE_COLOR');power=node(m,'ScalarParameter',parameter_name='Power',default_value=800)
        output(op(m,'Multiply',vc,power),'EMISSIVE_COLOR');output(scalar(m,.30),'ROUGHNESS')
    else:
        output(base,'BASE_COLOR');output(scalar(m,1 if family=='Metal' else .08 if family=='Hull' else 0),'METALLIC')
        output(scalar(m,{'Hull':.29,'Metal':.30,'Teak':.52,'Fabric':.87}[family]),'ROUGHNESS');output(scalar(m,.28),'SPECULAR')
    ml.recompile_material(m);assets.set_metadata_tag(m,'EndlessWorld.Aero87','object-uv-pbr-srgb-decoded-v2');assets.save_loaded_asset(m);materials.append(name)

report=[]
catalog=json.loads((S/'aero87-catalog.json').read_text(encoding='utf8'))
for item in catalog['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True;d.one_convex_hull_per_ucx=True
    nanite=item['name'] not in ['Aero87Glass','Aero87Water','Aero87Collision'];d.build_nanite=nanite
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    t=unreal.AssetImportTask();t.filename=str(f);t.destination_path='/Game/EndlessWorld/Kit';t.destination_name='SM_'+item['name']
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=False;t.options=opt
    tools.import_asset_tasks([t]);objects=t.get_objects();assert len(objects)==1,(item['name'],len(objects));mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        if family not in item['used_materials']:
            # FBX reimport retains now-unused slots. The bottles moved into the
            # glass asset, so the former slot must no longer disable Nanite.
            assert family in item['materials'],('unknown material slot',item['name'],family)
            family='Aero87Hull'
        mat=assets.load_asset(folder+'/M_'+('Aero87Glow' if family=='Glow' else family));assert mat,family;slot.set_editor_property('material_interface',mat)
        if nanite:assert mat.get_editor_property('blend_mode') in [unreal.BlendMode.BLEND_OPAQUE,unreal.BlendMode.BLEND_MASKED],(item['name'],family,'translucency must be in a separate mesh')
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=nanite;ns.fallback_relative_error=0;ns.fallback_percent_triangles=1.0;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.15 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assets.save_loaded_asset(mesh)
    report.append(dict(mesh=item['name'],triangles=item['triangles'],bounds_cm=[lo,hi],sha256=item['sha256'],nanite=nanite,fallback_relative_error=0))
out=P.parent/'Evidence/aero87-import-07.json'
assert not out.exists()
out.write_text(json.dumps(dict(success=True,assets=report,materials=materials,collision_boxes=catalog['collision_boxes']),indent=2),encoding='utf8')
unreal.log('EW_AERO87_IMPORTED '+str(len(report)))
