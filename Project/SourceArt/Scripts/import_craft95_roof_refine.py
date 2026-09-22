"""Parent-run UE import of seven rooftop visuals. -EWImportReport=<fresh JSON>.
Backs up each current uasset before replacement. No actor/collider/light changes.
"""
from pathlib import Path
import json, hashlib, shutil, re, math
import unreal

P=Path(__file__).resolve().parents[2];S=P/'SourceArt'
source_manifest=S/'craft95-roof-refine.json'
spec=json.loads(source_manifest.read_text(encoding='utf-8'))
cmd=unreal.SystemLibrary.get_command_line();match=re.search(r'-EWImportReport=(?:"([^"]+)"|(\S+))',cmd)
assert match,'Provide a fresh -EWImportReport= path'
report_path=Path(match.group(1) or match.group(2));assert not report_path.exists()
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
backup=P.parent/'BeforeCraft95RoofRefine'/sha(source_manifest)[:12]/'Project/Content'
report={'success':False,'revision':95,'scope':'Seven existing roof visual meshes only','meshes':[],'backups':[],
        'lighting_changed':False,'actors_changed':False,'runtime_colliders_changed':False,'materials_created':False}
def persist():
    report_path.parent.mkdir(parents=True,exist_ok=True)
    report_path.write_text(json.dumps(report,indent=2),encoding='utf-8')
def collision(mesh):
    # Editor subsystems are absent in Python commandlets. Read the reflected
    # BodySetup data directly; the deprecated wrapper returns -1 in this mode.
    body=mesh.get_editor_property('body_setup')
    if body is None:return {'body_setup':False,'simple_count':0,'convex_count':0,'complexity':'no body'}
    shapes=body.get_editor_property('agg_geom')
    counts={k:len(shapes.get_editor_property(k)) for k in ('box_elems','sphere_elems','sphyl_elems','convex_elems')}
    return {'body_setup':True,'simple_count':sum(counts[k] for k in ('box_elems','sphere_elems','sphyl_elems')),
            'convex_count':counts['convex_elems'],'counts':counts,
            'complexity':str(body.get_editor_property('collision_trace_flag'))}
def mesh_bounds(mesh):
    desc=mesh.get_static_mesh_description(0);lo=[math.inf]*3;hi=[-math.inf]*3
    for i in range(desc.get_vertex_count()):
        v=desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j,k in enumerate((v.x,v.y,v.z)):
            assert math.isfinite(k);lo[j]=min(lo[j],k);hi[j]=max(hi[j],k)
    return [lo,hi]

assert spec['preservation']['unchanged'] and len(spec['assets'])==7
protected=spec['preservation']['before']
family_overrides={family:path for item in spec['assets'] for family,path in item['material_overrides'].items()}
target_paths={str(Path('Content/EndlessWorld/Kit')/('SM_'+a['target_name']+'.uasset')) for a in spec['assets']}
try:
    # Refuse stale source/collision evidence, while allowing unrelated room work.
    for rel,expected in protected.items():
        if rel not in target_paths:assert sha(P/rel)==expected,('Protected source changed since generation',rel)
    for item in spec['assets']:
        name=item['target_name'];assert name.startswith(('UrbanGarden_','UrbanCrown_'))
        dest='/Game/EndlessWorld/Kit/SM_'+name;old=assets.load_asset(dest);assert old
        rel=Path('Content/EndlessWorld/Kit')/('SM_'+name+'.uasset');src_old=P/rel
        baseline_sha=protected[str(rel)]
        # An already imported matching replacement can be validated on a retry.
        already=assets.get_metadata_tag(old,'EndlessWorld.SourceSHA256')==item['sha256']
        if not already:assert sha(src_old)==baseline_sha,('Unexpected pre-import roof asset',src_old)
        old_collision=collision(old);old_bounds=mesh_bounds(old)
        for sidecar in src_old.parent.glob(src_old.stem+'.*'):
            if sidecar.suffix not in {'.uasset','.uexp','.ubulk','.uptnl'}:continue
            destination=backup/sidecar.relative_to(P/'Content')
            destination.parent.mkdir(parents=True,exist_ok=True)
            if not destination.exists():
                assert not already,'Original backup missing for an already imported mesh'
                shutil.copy2(sidecar,destination)
            if sidecar.suffix=='.uasset':assert sha(destination)==baseline_sha
            report['backups'].append({'file':str(destination),'sha256':sha(destination)})
        source=S/'Meshes'/item['file'];assert sha(source)==item['sha256']
        if not already:
            opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
            opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            d=opt.static_mesh_import_data;d.convert_scene_unit=True;d.combine_meshes=True;d.reorder_material_to_fbx_order=True
            d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.remove_degenerates=True;d.build_nanite=True
            d.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
            d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
            task=unreal.AssetImportTask();task.filename=str(source);task.destination_path='/Game/EndlessWorld/Kit';task.destination_name='SM_'+name
            task.automated=True;task.replace_existing=True;task.replace_existing_settings=False;task.save=False;task.options=opt
            tools.import_asset_tasks([task]);objects=task.get_objects();assert len(objects)==1;mesh=objects[0]
        else:mesh=old
        slots=list(mesh.get_editor_property('static_materials'));bindings=[]
        for slot in slots:
            family=str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
            if family not in item['materials']:family=str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
            assert family in item['materials'],(name,family)
            target=family_overrides.get(family)
            if not target:
                q='/Game/EndlessWorld/Materials/Quality93Final/M_'+family
                target=q if assets.does_asset_exist(q) else '/Game/EndlessWorld/Materials/M_'+family
            material=assets.load_asset(target);assert material,(name,family,target)
            slot.set_editor_property('material_interface',material);bindings.append({'slot':family,'material':target})
        mesh.set_editor_property('static_materials',slots)
        ns=mesh.get_editor_property('nanite_settings');ns.enabled=True;ns.fallback_relative_error=.05
        mesh.set_editor_property('nanite_settings',ns)
        current_bounds=mesh_bounds(mesh)
        assert all(abs(current_bounds[a][k]-item['ue_bounds_cm'][a][k])<.2 for a in range(2) for k in range(3)),(name,current_bounds,item['ue_bounds_cm'])
        new_collision=collision(mesh);assert new_collision==old_collision,(name,old_collision,new_collision)
        assets.set_metadata_tag(mesh,'EndlessWorld.SourceSHA256',item['sha256'])
        assets.set_metadata_tag(mesh,'EndlessWorld.Craft95Roof','refinement-r2')
        assert assets.save_loaded_asset(mesh)
        report['meshes'].append({'name':name,'source':item['file'],'source_sha256':item['sha256'],'triangles':item['triangles'],
            'old_bounds_cm':old_bounds,'bounds_cm':current_bounds,'materials':bindings,'nanite':True,
            'collision_before':old_collision,'collision_after':new_collision,'already_imported':already})
        persist();unreal.log('CRAFT95_ROOF_REFINED '+name)
        del old,mesh,slots;unreal.SystemLibrary.collect_garbage()
    for rel,expected in protected.items():
        if rel not in target_paths:assert sha(P/rel)==expected,('Protected source changed during import',rel)
    report['protected_non_target_sources_unchanged']=True;report['success']=True;persist()
    unreal.log('CRAFT95_ROOF_REFINE_COMPLETE 7')
except BaseException as error:
    report['error']=str(error);persist();raise
