from pathlib import Path
import unreal,json,hashlib,math,re
P=Path(__file__).resolve().parents[2];S=P/'SourceArt';assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools();folder='/Game/EndlessWorld/Materials';ml=unreal.MaterialEditingLibrary
def node(m,cls,**kw):
    n=ml.create_material_expression(m,getattr(unreal,'MaterialExpression'+cls))
    for k,v in kw.items():n.set_editor_property(k,v)
    return n
def scalar(m,v):return node(m,'Constant',r=v)
def rgb(m,v):return node(m,'Constant3Vector',constant=unreal.LinearColor(*v,1))
def link(a,b,pin,out=''):
    pins=list(ml.get_material_expression_input_names(b))
    if pin=='Input' and pin not in pins and len(pins)==1:pin=pins[0]
    assert ml.connect_material_expressions(a,out,b,pin),(pin,pins)
def output(n,p,out=''):assert ml.connect_material_property(n,out,getattr(unreal.MaterialProperty,'MP_'+p))
def op(m,cls,a,b):
    n=node(m,cls);pins=list(ml.get_material_expression_input_names(n));link(a,n,pins[0]);link(b,n,pins[1]);return n
def custom(m,code,args,kind=3):
    inputs=[]
    for key in args:
        i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
    n=node(m,'Custom',code=code,output_type=getattr(unreal.CustomMaterialOutputType,'CMOT_FLOAT'+str(kind)),inputs=inputs)
    for k,v in args.items():link(v,n,k)
    return n
def material(name,transparent=False):
    path=folder+'/M_'+name
    m=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_'+name,folder,unreal.Material,unreal.MaterialFactoryNew())
    assert assets.get_metadata_tag(m,'EndlessWorld.Pool90') in ['', 'v1']
    count=ml.get_num_material_expressions(m)
    while count:
        ml.delete_all_material_expressions(m);n=ml.get_num_material_expressions(m);assert n<count;count=n
    m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('used_with_nanite',not transparent)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if transparent else unreal.BlendMode.BLEND_OPAQUE)
    if transparent:m.set_editor_property('two_sided',True);m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    return m
def save(m):
    ml.recompile_material(m);assets.set_metadata_tag(m,'EndlessWorld.Pool90','v1');assert assets.save_loaded_asset(m)
collection=assets.load_asset(folder+'/MPC_DayCycle');assert collection
names=[]
for family in ['Stone','Tile','Water','Glass','Metal','Fabric','Light']:
    m=material('Pool90'+family,family in ['Water','Glass']);uv=node(m,'TextureCoordinate');t=node(m,'Time')
    vc=op(m,'Power',node(m,'VertexColor'),scalar(m,2.2));base=vc
    if family=='Stone':
        tex=assets.load_asset('/Game/EndlessWorld/Textures/T_Limestone_Base');assert tex
        sample=node(m,'TextureSample',texture=tex,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR);link(uv,sample,'UVs')
        base=op(m,'Multiply',base,op(m,'Add',scalar(m,.80),op(m,'Multiply',sample,scalar(m,.20))))
    if family=='Tile':
        # Thirty-centimetre mosaic squares with fine matte grout and moving caustic light.
        base=custom(m,'float2 p=UV*3.333;float2 f=frac(p);float grout=1-smoothstep(.018,.032,min(min(f.x,1-f.x),min(f.y,1-f.y)));float n=frac(sin(dot(floor(p),float2(12.91,78.2)))*43758.54);return lerp(Colour.rgb*(.87+.22*n),float3(.28,.39,.38),grout);',{'UV':uv,'Colour':base})
        glow=custom(m,'float2 p=UV*2.1;float a=sin(p.x*2.4+sin(p.y*2.1+T*.53))+cos(p.y*2.9+sin(p.x*1.8-T*.41));float b=sin(p.x*1.6-p.y*2.2+T*.37);float c=pow(saturate(1-abs(a+.35*b)*2.9),9);return float3(.10,.24,.21)*c*(80+850*Day);',{'UV':uv,'T':t,'Day':node(m,'CollectionParameter',collection=collection,parameter_name='CityNightEmission')})
        # Surface shading supplies daylight caustics as a small albedo modulation.
        pattern=custom(m,'float a=sin(UV.x*8+sin(UV.y*5+T*.5))+cos(UV.y*9+sin(UV.x*6-T*.4));return 1+pow(saturate(1-abs(a)*2),7)*.48;',{'UV':uv,'T':t},1)
        base=op(m,'Multiply',base,pattern);output(glow,'EMISSIVE_COLOR')
    if family in ['Water','Glass']:
        base=rgb(m,(.022,.15,.20) if family=='Water' else (.09,.22,.26))
        f=node(m,'Fresnel',exponent=4,base_reflect_fraction=.04)
        output(op(m,'Add',scalar(m,.28 if family=='Water' else .20),op(m,'Multiply',f,scalar(m,.56))),'OPACITY')
    if family=='Water':
        output(custom(m,'float x=sin(UV.x*2.9+UV.y*1.7+T*.67)*.045+sin(UV.y*5.1-T*.42)*.025;float y=cos(UV.y*3.3-UV.x*1.9+T*.53)*.040;return normalize(float3(x,y,1));',{'UV':uv,'T':t}),'NORMAL')
    if family=='Light':
        night=node(m,'CollectionParameter',collection=collection,parameter_name='CityNightEmission')
        output(op(m,'Multiply',vc,op(m,'Add',scalar(m,20),op(m,'Multiply',night,scalar(m,3200)))),'EMISSIVE_COLOR')
    output(base,'BASE_COLOR');output(scalar(m,{'Stone':.64,'Tile':.38,'Water':.07,'Glass':.10,'Metal':.26,'Fabric':.88,'Light':.3}[family]),'ROUGHNESS')
    output(scalar(m,.86 if family=='Metal' else 0),'METALLIC');output(scalar(m,.45 if family=='Water' else .28),'SPECULAR');save(m);names.append(m.get_name())
# Optical absorption is in scene-linear colour before bloom/tonemapping, so the
# existing SDR/HDR exposure remains authoritative. No screen-wide blue UI image.
m=material('Pool90Underwater');m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM);m.set_editor_property('blendable_priority',10)
uv=node(m,'ScreenPosition')
scene=node(m,'SceneTexture',scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
depth=node(m,'SceneTexture',scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
args={'UV':uv,'Scene':scene,'Z':depth,'T':node(m,'Time'),'DebugMode':node(m,'ScalarParameter',parameter_name='DebugMode',default_value=0)}
for k in ['Forward','Right','Up','WaterRight','WaterForward','LocalEye','Extent']:
    args[k]=node(m,'VectorParameter',parameter_name=k,default_value=unreal.LinearColor(0,0,0,0))
args['Lens']=node(m,'VectorParameter',parameter_name='Lens',default_value=unreal.LinearColor(1,1,1,0))
code=r'''
if(DebugMode>.5 && DebugMode<1.5)return Scene.rgb;
float2 screenUV=UV.xy;
float3 ray=normalize(Forward.xyz+Right.xyz*(screenUV.x*2-1)*Lens.x+Up.xyz*(1-screenUV.y*2)*Lens.y);
float3 dir=float3(dot(ray,WaterRight.xyz),dot(ray,WaterForward.xyz),ray.z);
float3 safeDir=sign(dir+1e-7)*max(abs(dir),1e-5);
float3 boundary=float3(dir.x>0?Extent.x:-Extent.x,dir.y>0?Extent.y:-Extent.y,dir.z>0?Extent.z:Lens.z);
float3 exitRay=(boundary-LocalEye.xyz)/safeDir;
float waterPath=min(max(0,min(exitRay.x,min(exitRay.y,exitRay.z))),Z.r/max(dot(ray,Forward.xyz),.05));
waterPath=min(waterPath,4000);
if(DebugMode>2.5)return float3(saturate(waterPath/1500),saturate(Z.r/1500),.1);
float amount=saturate((Z.r-55)/160);
float2 sceneUV=GetDefaultSceneTextureUV(Parameters,14);
float2 warp=float2(sin(screenUV.y*27+T*.85)+sin(screenUV.x*13-T*.49),cos(screenUV.x*25+T*.65))*.0011*amount;
float2 target=ClampSceneTextureUV(sceneUV+warp,14);
float blur=saturate(waterPath/1800)*.0012;
float3 c=(SceneTextureLookup(target,14,false).rgb*.6+SceneTextureLookup(ClampSceneTextureUV(target+float2(blur,0),14),14,false).rgb*.2+SceneTextureLookup(ClampSceneTextureUV(target-float2(blur,0),14),14,false).rgb*.2);
// SceneTexture nodes undo pre-exposure; raw custom lookups require the same conversion.
c*=View.OneOverPreExposure;
if(DebugMode>1.5)return c;
float3 transmission=exp(-waterPath*float3(.0022,.00060,.00030));
float brightness=max(dot(c,float3(.2126,.7152,.0722)),.0001);
float3 scatter=float3(.07,.29,.34)*brightness;
float3 result=c*transmission+scatter*(1-transmission);
return lerp(Scene.rgb,result,amount);
'''
effect=custom(m,code,args);output(effect,'EMISSIVE_COLOR');save(m);names.append(m.get_name())
report=[];catalog=json.loads((S/'pool90-catalog.json').read_text(encoding='utf8'))
for item in catalog['assets']:
    f=S/'Meshes'/item['file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True;d.auto_generate_collision=False;d.generate_lightmap_u_vs=False
    nanite=item['name'] not in ['Pool90Water','Pool90Glass'];d.build_nanite=nanite;d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(f);task.destination_path='/Game/EndlessWorld/Kit';task.destination_name='SM_'+item['name'];task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['used_materials']:family='Pool90Stone'
        mat=assets.load_asset(folder+'/M_'+family);assert mat;slot.set_editor_property('material_interface',mat)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=nanite;ns.fallback_relative_error=0;ns.fallback_percent_triangles=1;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.15 for a in range(2) for b in range(3)),(item['name'],lo,hi)
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assert assets.save_loaded_asset(mesh)
    report.append(dict(mesh=item['name'],sha256=item['sha256'],bounds=[lo,hi],triangles=item['triangles'],nanite=nanite))
match=re.search(r'EWImportReport=(?:"([^"]+)"|(\S+))',unreal.SystemLibrary.get_command_line());assert match
out=Path(match.group(1) or match.group(2));assert not out.exists();out.write_text(json.dumps(dict(success=True,assets=report,materials=names),indent=2),encoding='utf8')
unreal.log('EW_POOL90_IMPORTED')
