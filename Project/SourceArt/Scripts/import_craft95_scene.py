"""Import reversible roof/cafe craft and a physically scaled plaza surface."""
from pathlib import Path
import unreal,json,re,hashlib,shutil,math
P=Path(__file__).resolve().parents[2];S=P/'SourceArt';backup=P.parent/'BeforeCraft95/Project/Content'
cmd=unreal.SystemLibrary.get_command_line();match=re.search(r'-EWImportReport=(?:"([^"]+)"|(\S+))',cmd)
assert match
report_path=Path(match.group(1) or match.group(2));assert not report_path.exists()
report={'success':False,'revision':95,'meshes':[],'textures':[],'backups':[]}
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def preserve(path):
    rel=Path(path.removeprefix('/Game/')+'.uasset');src=P/'Content'/rel;dst=backup/rel
    assert src.is_file();dst.parent.mkdir(parents=True,exist_ok=True)
    if not dst.exists():shutil.copy2(src,dst)
    report['backups'].append({'path':str(dst),'sha256':sha(dst)})
for item in json.loads((S/'craft95-surroundings.json').read_text())['assets']:
    src=S/'Meshes'/item['file'];assert sha(src)==item['sha256']
    name=item['name'].replace('Craft95RoofGarden_','UrbanGarden_');dest='/Game/EndlessWorld/Kit/SM_'+name
    if name.startswith('UrbanGarden_'):preserve(dest)
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True;d.build_nanite=True
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(src);task.destination_path='/Game/EndlessWorld/Kit';task.destination_name='SM_'+name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);mesh=task.get_objects()[0];slots=list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        q='/Game/EndlessWorld/Materials/Quality93Final/M_'+family
        target=q if assets.does_asset_exist(q) else '/Game/EndlessWorld/Materials/M_'+family
        mat=assets.load_asset(target);assert mat,(name,family);slot.set_editor_property('material_interface',mat)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for a,k in enumerate((v.x,v.y,v.z)):lo[a]=min(lo[a],k);hi[a]=max(hi[a],k)
    assert all(abs([lo,hi][a][k]-item['ue_bounds_cm'][a][k])<.2 for a in range(2) for k in range(3)),(name,lo,hi)
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assets.save_loaded_asset(mesh)
    report['meshes'].append({'name':name,'bounds':[lo,hi],'triangles':item['triangles'],'nanite':True})
    del desc,mesh,task,slots;unreal.SystemLibrary.collect_garbage()

tex={};spec=json.loads((S/'craft95-paving.json').read_text())
for item in spec['textures']:
    src=S/'Textures/Craft95'/item['file'];assert sha(src)==item['sha256']
    t=unreal.AssetImportTask();t.filename=str(src);t.destination_path='/Game/EndlessWorld/Textures/Craft95';t.destination_name=src.stem
    t.automated=True;t.replace_existing=True;t.save=False;tools.import_asset_tasks([t]);obj=t.get_objects()[0]
    obj.set_editor_property('srgb',item['kind']=='Base')
    if item['kind']=='Normal':obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    if item['kind']=='ORM':obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    assets.save_loaded_asset(obj);tex[item['kind']]=obj;report['textures'].append(item)
path='/Game/EndlessWorld/Materials/Craft95/M_Craft95Paving'
mat=assets.load_asset(path) if assets.does_asset_exist(path) else None
if not mat:
    mat=tools.create_asset('M_Craft95Paving','/Game/EndlessWorld/Materials/Craft95',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes',True);mat.set_editor_property('used_with_nanite',True)
    def node(cls,**kw):
        n=ml.create_material_expression(mat,getattr(unreal,'MaterialExpression'+cls))
        for k,v in kw.items():n.set_editor_property(k,v)
        return n
    def link(a,b,pin,out=''):
        names=list(ml.get_material_expression_input_names(b))
        if pin=='Input' and pin not in names and len(names)==1:pin=names[0]
        assert ml.connect_material_expressions(a,out,b,pin),(b.get_class().get_name(),pin,names)
    def output(a,prop,out=''):assert ml.connect_material_property(a,out,getattr(unreal.MaterialProperty,'MP_'+prop))
    wp=node('WorldPosition');xy=node('ComponentMask',r=True,g=True,b=False,a=False);link(wp,xy,'Input')
    uv=node('Multiply');link(xy,uv,'A');link(node('Constant',r=1/spec['period_cm']),uv,'B')
    def sample(kind):
        t=node('TextureSample',texture=tex[kind],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind=='ORM' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        link(uv,t,'UVs');return t
    output(sample('Base'),'BASE_COLOR','RGB');output(sample('Normal'),'NORMAL','RGB')
    orm=sample('ORM');output(orm,'ROUGHNESS','G');output(orm,'AMBIENT_OCCLUSION','R');output(node('Constant',r=.30),'SPECULAR')
    ml.recompile_material(mat);assets.save_loaded_asset(mat)
dest='/Game/EndlessWorld/Kit/SM_Craft95StoneDeck'
mesh=assets.load_asset(dest) if assets.does_asset_exist(dest) else assets.duplicate_asset('/Game/EndlessWorld/Kit/SM_StoneDeck',dest)
assert mesh
slots=list(mesh.get_editor_property('static_materials'))
for slot in slots:slot.set_editor_property('material_interface',mat)
mesh.set_editor_property('static_materials',slots);assets.save_loaded_asset(mesh)
report['meshes'].append({'name':'Craft95StoneDeck','geometry':'unchanged duplicate of StoneDeck','period_cm':400,'paver_cm':50})
report['success']=True;report_path.write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CRAFT95_SCENE_COMPLETE')
