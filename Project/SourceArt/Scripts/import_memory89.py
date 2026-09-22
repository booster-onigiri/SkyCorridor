from pathlib import Path
import unreal,json,re,hashlib

def import_report_path(command_line):
    match=re.search(r'(?:^|\s)-EWImportReport=(?:"([^"]+)"|([^\s"]+))(?=\s|$)',command_line)
    if not match:
        raise ValueError('Provide -EWImportReport="path to report.json"')
    return Path(match.group(1) or match.group(2))

REPORT_PATH=import_report_path(unreal.SystemLibrary.get_command_line())
R=Path(__file__).resolve().parents[3];S=R/'Project/SourceArt';A=unreal.EditorAssetLibrary;AT=unreal.AssetToolsHelpers.get_asset_tools();ML=unreal.MaterialEditingLibrary
folder='/Game/EndlessWorld/Memory89';made=[]
material_only='-EWMemoryMaterialOnly' in unreal.SystemLibrary.get_command_line()
def node(m,kind,**values):
    n=ML.create_material_expression(m,getattr(unreal,'MaterialExpression'+kind))
    for k,v in values.items():n.set_editor_property(k,v)
    return n
def out(n,p,pin=''):assert ML.connect_material_property(n,pin,getattr(unreal.MaterialProperty,'MP_'+p))
def link(a,b,pin):assert ML.connect_material_expressions(a,'',b,pin)
def fresh(name):
    path=folder+'/'+name;old=A.load_asset(path) if A.does_asset_exist(path) else None
    if old:assert A.get_metadata_tag(old,'EndlessWorld.Memory89')=='v1';ML.delete_all_material_expressions(old);return old
    return AT.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
for name,color,metal,rough in [('Ceramic',(.59,.57,.47),.12,.32),('Edge',(.19,.24,.23),.84,.24),('Glass',(.015,.027,.027),.35,.16),('Copper',(.48,.30,.12),.8,.26)]:
    m=fresh('M_'+name);out(node(m,'Constant3Vector',constant=unreal.LinearColor(*color,1)),'BASE_COLOR');out(node(m,'Constant',r=metal),'METALLIC');out(node(m,'Constant',r=rough),'ROUGHNESS')
    ML.recompile_material(m);A.set_metadata_tag(m,'EndlessWorld.Memory89','v1');assert A.save_loaded_asset(m)
m=fresh('M_MemoryTrace');m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
uv=node(m,'TextureCoordinate');time=node(m,'Time');vis=node(m,'ScalarParameter',parameter_name='Visibility',default_value=0)
c=node(m,'Custom',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,code='''
// FBX converts Blender's vertical UV convention on import.
float2 p=float2(UV.x,1-UV.y); p.x+=.014*sin(p.y*17+Time*.42);
float2 h=(p-float2(.5,.17))/float2(.080,.057);
float head=exp(-dot(h,h)*1.4);
float width=lerp(.105,.19,saturate((p.y-.30)/.58));
float2 b=(p-float2(.5,.56))/float2(width,.26);
float body=exp(-dot(b,b)*1.55)*smoothstep(.22,.34,p.y);
float gap=.66+.20*sin(p.y*73+Time*.27)+.12*sin(p.x*43-p.y*29);
return saturate((head+body*.7)*gap)*Visibility;
''')
inputs=[]
for name in ['UV','Time','Visibility']:
    v=unreal.CustomInput();v.set_editor_property('input_name',name);inputs.append(v)
c.set_editor_property('inputs',inputs);link(uv,c,'UV');link(time,c,'Time');link(vis,c,'Visibility');out(c,'OPACITY')
out(node(m,'VectorParameter',parameter_name='Glow',default_value=unreal.LinearColor(1600,1900,1750,1)),'EMISSIVE_COLOR')
ML.recompile_material(m);A.set_metadata_tag(m,'EndlessWorld.Memory89','v1');assert A.save_loaded_asset(m)
if material_only:made.append({'asset':m.get_path_name(),'head_at_top':True})
for item in ([] if material_only else json.loads((S/'Memory89/catalog.json').read_text())):
    file=S/'Memory89'/item['file'];assert hashlib.sha256(file.read_bytes()).hexdigest()==item['sha256']
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.auto_generate_collision=False;d.build_nanite=False;d.generate_lightmap_u_vs=False
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=folder;task.destination_name='SM_'+item['name'];task.automated=True;task.replace_existing=True;task.options=opt
    AT.import_asset_tasks([task]);mesh=task.get_objects()[0];slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        name=str(slot.get_editor_property('imported_material_slot_name'))
        mat=A.load_asset(folder+'/M_'+name) if item['name']=='Terminal89Body' else m
        assert mat,(item['name'],name);slot.set_editor_property('material_interface',mat)
    if not slots:mesh.set_material(0,m)
    else:mesh.set_editor_property('static_materials',slots)
    assert A.save_loaded_asset(mesh);made.append({'asset':mesh.get_path_name(),'bounds':str(mesh.get_bounds())})
for file in ([] if material_only else sorted((S/'Audio89/Masters-v1').glob('*.wav'))):
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path='/Game/EndlessWorld/Audio89';task.destination_name=file.stem;task.automated=True;task.replace_existing=True
    AT.import_asset_tasks([task]);sound=task.get_objects()[0];sound.set_editor_property('looping',False);sound.set_editor_property('compression_quality',85)
    assert A.save_loaded_asset(sound);made.append({'asset':sound.get_path_name(),'duration':sound.get_editor_property('duration')})
REPORT_PATH.write_text(json.dumps({'success':True,'assets':made},indent=2),encoding='utf8')
