"""Import the reviewed craft kit and physically scaled shared finishes."""
from pathlib import Path
import json,math,hashlib,re,shutil,unreal
P=Path(__file__).resolve().parents[2];S=P/'SourceArt'
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
cmd=unreal.SystemLibrary.get_command_line();report_path=Path(re.search(r'-EWImportReport="?([^" ]+)',cmd).group(1))
assert not report_path.exists();report={'revision':93,'success':False,'textures':[],'materials':[],'meshes':[]}
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
def node(m,cls,**values):
    n=ml.create_material_expression(m,getattr(unreal,'MaterialExpression'+cls))
    for k,v in values.items():n.set_editor_property(k,v)
    return n
def scalar(m,v):return node(m,'Constant',r=v)
def link(a,b,pin,output=''):
    assert ml.connect_material_expressions(a,output,b,pin),(b.get_class().get_name(),pin)
def output(n,prop,pin=''):assert ml.connect_material_property(n,pin,getattr(unreal.MaterialProperty,'MP_'+prop))
def op(m,cls,a,b):
    n=node(m,cls);names=list(ml.get_material_expression_input_names(n))
    link(a,n,names[0] if cls=='Power' else 'A');link(b,n,names[1] if cls=='Power' else 'B');return n
texs={};tiling={}
for item in json.loads((S/'quality93-textures.json').read_text())['textures']:
    f=S/'Textures/Quality93'/item['file'];assert sha(f)==item['sha256']
    task=unreal.AssetImportTask();task.filename=str(f);task.destination_path='/Game/EndlessWorld/Textures/Quality93'
    task.destination_name=f.stem;task.automated=True;task.replace_existing=True;task.save=False
    tools.import_asset_tasks([task]);texture=task.get_objects()[0]
    texture.set_editor_property('srgb',item['map']=='Base')
    if item['map']=='Normal':texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif item['map']=='ORM':texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('lod_bias',0);assets.save_loaded_asset(texture)
    texs[item['surface'],item['map']]=texture;tiling[item['surface']]=1/item['tile_metres']
    report['textures'].append({'asset':texture.get_path_name(),'source_sha256':item['sha256'],'srgb':item['map']=='Base'})

# Preserve translucent glazing, all video faces, water and time-controlled emitters.
families={
 'InteriorOak':('Oak',0),'InteriorPlaster':('Plaster',0),'InteriorFabric':('Linen',0),
 'InteriorCeramic':('Glaze',0),'InteriorBrass':('Metal',.88),'InteriorPaper':('Paper',0),
 'InteriorLeaf':('Leaf',0),'InteriorStem':('Oak',0),
 'Timber':('Oak',0),'Cloth':('Linen',0),'Bronze':('Metal',.88),'Ceramic':('Glaze',0),
 'Paint':('Plaster',0),'Paper':('Paper',0),
 'Sky92Stone':('Stone',0),'Sky92White':('Stone',0),'Sky92Edge':('Stone',0),
 'Sky92Wood':('Oak',0),'Sky92Cloth':('Linen',0),'Sky92Leaf':('Leaf',0),
 'Sky92Metal':('Metal',.88),'Sky92Copper':('Metal',.9),'Sky92Roof':('Metal',.62),'Sky92Tile':('Glaze',0),
 'Aero87Fabric':('Linen',0),'Aero87Teak':('Oak',0),'Aero87Metal':('Metal',.92),'Aero87Hull':('Glaze',.05),
 'Explore85Steel':('Metal',1.),
}
folder='/Game/EndlessWorld/Materials';quality_folder=folder+'/Quality93Final'
reuse_materials='-EWQualityReuseMaterials' in cmd
if reuse_materials:
    previous=json.loads((P.parent/'Evidence/quality93-import-05.json').read_text())
    assert previous['success']
    existing_specs={item['asset']:item for item in previous['materials']}
for family,(surface,metal) in families.items():
    path=quality_folder+'/M_'+family
    if reuse_materials:
        mat=assets.load_asset(path);spec=existing_specs[path]
        assert mat and spec['surface']==surface and spec['metallic']==metal
        assert spec['srgb_vertex_decode']==family.startswith(('Sky92','Aero87'))
        assert abs(spec['tile_metres']-1/tiling[surface])<1e-6
        assert ml.get_num_material_expressions(mat)>0
        report['materials'].append(dict(spec,reused=True))
        continue
    # Keep previous graphs intact. Deleting rooted Python expressions can assert
    # in UE 5.8; create a fresh reviewed revision and only rebind mesh slots.
    assert not assets.does_asset_exist(path),'Use a fresh material revision: '+path
    mat=tools.create_asset('M_'+family,quality_folder,unreal.Material,unreal.MaterialFactoryNew())
    assert mat and ml.get_num_material_expressions(mat)==0
    mat.set_editor_property('used_with_instanced_static_meshes',True);mat.set_editor_property('used_with_nanite',True)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('tangent_space_normal',True)
    leaf='Leaf' in family
    mat.set_editor_property('two_sided',leaf)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE if leaf else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # Retain each authored kit's colour contract. The older interiors already
    # carry display-calibrated vertex colours; decoding those again crushes fabric.
    decode=family.startswith(('Sky92','Aero87'))
    vc=node(mat,'VertexColor')
    if decode:vc=op(mat,'Power',vc,scalar(mat,2.2))
    uv=node(mat,'TextureCoordinate',u_tiling=tiling[surface],v_tiling=tiling[surface])
    def sample(suffix):
        t=node(mat,'TextureSample',texture=texs[surface,suffix],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if suffix=='Base' else unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        link(uv,t,'UVs');return t
    base=op(mat,'Multiply',vc,sample('Base'));output(base,'BASE_COLOR')
    output(sample('Normal'),'NORMAL','RGB')
    orm=sample('ORM');output(orm,'ROUGHNESS','G');output(orm,'AMBIENT_OCCLUSION','R')
    output(scalar(mat,metal),'METALLIC');output(scalar(mat,.38),'SPECULAR')
    if surface=='Metal':output(scalar(mat,.32),'ANISOTROPY')
    if leaf:output(op(mat,'Multiply',base,scalar(mat,.32)),'SUBSURFACE_COLOR')
    ml.recompile_material(mat);assets.set_metadata_tag(mat,'EndlessWorld.Quality93','original-craft-pbr-v1');assert assets.save_loaded_asset(mat)
    report['materials'].append({'asset':path,'surface':surface,'tile_metres':1/tiling[surface],'metallic':metal,'srgb_vertex_decode':decode,'texture_samples':3})

manifest=json.loads((S/'quality93-assets.json').read_text())
selection=re.search(r'-EWQualityMeshPrefix=([A-Za-z0-9_,]+)',cmd)
prefixes=tuple(selection.group(1).split(',')) if selection else None
for item in manifest['assets']:
    if prefixes and not item['name'].startswith(prefixes):continue
    f=S/'Meshes'/item['file'];assert sha(f)==item['sha256'],f
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
    d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True;d.build_nanite=True
    d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task=unreal.AssetImportTask();task.filename=str(f);task.destination_path='/Game/EndlessWorld/Kit';task.destination_name='SM_'+item['name']
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=opt
    tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1;mesh=objects[0]
    slots=list(mesh.get_editor_property('static_materials'));bindings=[]
    for slot in slots:
        family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        assert family in item['materials'],(item['name'],family)
        target='NightFixtureGlow' if family=='Glow' and item['name'].startswith(('Hotel83','Explore85','SkyTheatre')) else family
        mat=assets.load_asset((quality_folder if target in families else folder)+'/M_'+target);assert mat,(item['name'],target)
        slot.set_editor_property('material_interface',mat);bindings.append(target)
    mesh.set_editor_property('static_materials',slots)
    ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05;mesh.set_editor_property('nanite_settings',ns)
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,value in enumerate([v.x,v.y,v.z]):lo[j]=min(lo[j],value);hi[j]=max(hi[j],value)
    assert all(abs([lo,hi][a][b]-item['ue_bounds_cm'][a][b])<.2 for a in range(2) for b in range(3)),(item['name'],lo,hi,item['ue_bounds_cm'])
    assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256']);assets.set_metadata_tag(mesh,'EndlessWorld.Quality93','v1')
    assert assets.save_loaded_asset(mesh)
    report['meshes'].append({'name':item['name'],'triangles':item['triangles'],'bounds_cm':[lo,hi],'materials':bindings,'sha256':item['sha256'],'nanite':True})
    unreal.log('QUALITY93_IMPORTED '+item['name'])
    del desc,mesh,objects,slots,task
    unreal.SystemLibrary.collect_garbage()

# Apply identical finishes to every existing static kit using those named families.
# Back up each additional mesh before updating its material slots; geometry is retained.
report['rebound_existing_meshes']=[]
for path in assets.list_assets('/Game/EndlessWorld/Kit',recursive=True,include_folder=False):
    mesh=assets.load_asset(path)
    if not isinstance(mesh,unreal.StaticMesh):continue
    slots=list(mesh.get_editor_property('static_materials'));changed=[]
    for slot in slots:
        material=slot.get_editor_property('material_interface')
        if not material:continue
        old=material.get_path_name();family=material.get_name().removeprefix('M_')
        if family not in families or old.startswith(quality_folder+'/'):continue
        if not old.startswith(folder+'/'):continue
        changed.append({'from':old,'to':quality_folder+'/M_'+family})
        slot.set_editor_property('material_interface',assets.load_asset(quality_folder+'/M_'+family))
    if changed:
        rel=Path(path.split('.')[0].removeprefix('/Game/')+'.uasset')
        source=P/'Content'/rel;backup=P.parent/'BeforeQuality93/Project/Content'/rel
        source.resolve().relative_to((P/'Content').resolve());assert source.exists()
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():shutil.copy2(source,backup)
        mesh.set_editor_property('static_materials',slots);assert assets.save_loaded_asset(mesh)
        report['rebound_existing_meshes'].append({'asset':path,'backup':str(backup),'backup_sha256':sha(backup),'materials':changed})
    del mesh,slots
    unreal.SystemLibrary.collect_garbage()
report['success']=True;report_path.write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('QUALITY93_IMPORT_COMPLETE '+str(len(report['meshes'])))
