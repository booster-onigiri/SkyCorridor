"""Run with Unreal Editor's PythonScript commandlet on this exported project only.

Reimports two public-release exteriors and verifies the existing sky-theatre deck
against its procedural source. Retains collision, interiors, materials, vehicle
code and docking data. No reference imagery is imported.
"""
from pathlib import Path
import hashlib
import json
import unreal

REPO = Path(__file__).resolve().parents[2]
PROJECT = REPO / 'Project'
assert Path(unreal.Paths.project_dir()).resolve() == PROJECT.resolve(), 'Wrong project'
ART = PROJECT / 'SourceArt'
catalog = json.loads((ART / 'aero87-catalog.json').read_text(encoding='utf-8'))
assert catalog.get('public_design_revision') == 'observatory-barge-v1'
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
results = []
def bounds(mesh):
    desc = mesh.get_static_mesh_description(0)
    lo = [float('inf')] * 3
    hi = [-float('inf')] * 3
    for i in range(desc.get_vertex_count()):
        v = desc.get_vertex_position(unreal.VertexID(id_value=i))
        for j, value in enumerate((v.x, v.y, v.z)):
            lo[j] = min(lo[j], value)
            hi[j] = max(hi[j], value)
    return [lo, hi]

for item in catalog['assets']:
    if item['name'] not in ('Aero87Hull', 'Aero87Fins', 'SkyTheatre87Deck'):
        continue
    old_bounds = None
    if item['name'] == 'SkyTheatre87Deck':
        existing = library.load_asset('/Game/EndlessWorld/Kit/SM_' + item['name'])
        assert existing
        old_bounds = bounds(existing)
    source = ART / 'Meshes' / item['file']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == item['sha256']
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.convert_scene_unit = True
    data.combine_meshes = True
    data.reorder_material_to_fbx_order = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.remove_degenerates = True
    data.build_nanite = True
    data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/Game/EndlessWorld/Kit'
    task.destination_name = 'SM_' + item['name']
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    task.options = options
    tools.import_asset_tasks([task])
    objects = task.get_objects()
    assert len(objects) == 1
    mesh = objects[0]
    slots = list(mesh.get_editor_property('static_materials'))
    for slot in slots:
        family = str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if family not in item['materials']:
            family = str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        assert family in item['materials'], family
        if family not in item['used_materials']:
            family = 'Aero87Hull'
        if family == 'Glow':
            family = 'Aero87Glow'
        material = library.load_asset('/Game/EndlessWorld/Materials/M_' + family)
        assert material, family
        assert material.get_editor_property('blend_mode') in (
            unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED)
        slot.set_editor_property('material_interface', material)
    mesh.set_editor_property('static_materials', slots)
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.enabled = True
    nanite.fallback_relative_error = 0
    nanite.fallback_percent_triangles = 1.0
    mesh.set_editor_property('nanite_settings', nanite)
    actual_bounds = bounds(mesh)
    assert all(abs(actual_bounds[i][j] - item['ue_bounds_cm'][i][j]) < .15
               for i in range(2) for j in range(3)), (item['name'], actual_bounds)
    if old_bounds is not None:
        assert all(abs(actual_bounds[i][j] - old_bounds[i][j]) < .15
                   for i in range(2) for j in range(3)), ('Deck envelope changed', old_bounds, actual_bounds)
    library.set_metadata_tag(mesh, 'EndlessWorld.SourceSHA256', item['sha256'])
    library.set_metadata_tag(mesh, 'EndlessWorld.PublicDesign', 'observatory-barge-v1')
    assert library.save_loaded_asset(mesh)
    results.append({'asset': mesh.get_path_name(), 'fbx_sha256': item['sha256'],
                    'materials': item['used_materials'], 'bounds_cm': actual_bounds,
                    'previous_bounds_cm': old_bounds})
assert len(results) == 3
output = ART / 'Generated'
output.mkdir(exist_ok=True)
(output / 'public-aero-import.json').write_text(json.dumps(
    {'success': True, 'replaced': results, 'collision_reimported': False}, indent=2), encoding='utf-8')
unreal.log('PUBLIC_AERO_IMPORT_COMPLETE')
