"""Replace the four reviewed interior meshes without changing their asset paths.

Run in Unreal with -EWImportReport=<new absolute JSON path>. All sources and
backups are checked first; all imported bounds are checked before any mesh is
saved. Reports retain partial progress if an import or save fails. Existing
BeforeCraft95 originals are never overwritten, including on repeat imports.
"""
from pathlib import Path
import hashlib
import json
import math
import os
import re
import shutil
import traceback

import unreal


PROJECT = Path(__file__).resolve().parents[2]
SOURCE_ART = PROJECT / 'SourceArt'
BACKUP_ROOT = PROJECT.parent / 'BeforeCraft95' / 'Project' / 'Content'
KIT = '/Game/EndlessWorld/Kit'
MATERIALS = '/Game/EndlessWorld/Materials'
TARGETS = {
    'Craft95InteriorShell0': 'Explore85Shell0',
    'Craft95InteriorFurniture0': 'Explore85Furniture0',
    'Craft95InteriorShell2': 'Explore85Shell2',
    'Craft95InteriorFurniture2': 'Explore85Furniture2',
}
BOUNDS_TOLERANCE_CM = 0.3
# Preserve the current mesh's reduction/fallback policy rather than resetting it
# to FBX defaults. Quality93 authored fallback_relative_error=0.05; reading the
# actual asset also retains any later deliberately reviewed value.
NANITE_FIELDS = (
    'enabled', 'keep_percent_triangles', 'trim_relative_error',
    'fallback_target', 'fallback_percent_triangles', 'fallback_relative_error',
)
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def copy_new_verified(source, destination, expected_sha):
    """Use exclusive creation so even a concurrent run cannot replace a backup."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        with source.open('rb') as src, destination.open('xb') as dst:
            shutil.copyfileobj(src, dst, length=1024 * 1024)
            dst.flush()
            os.fsync(dst.fileno())
    require(sha256(destination) == expected_sha,
            'Backup hash mismatch; preserve and inspect: ' + str(destination))
    require(sha256(source) == expected_sha,
            'Source changed during backup: ' + str(source))


def preserve_target(target_name):
    """Keep the first original and a verified current copy when they differ."""
    records = []
    stem = Path('EndlessWorld') / 'Kit' / ('SM_' + target_name)
    for suffix in ('.uasset', '.uexp', '.ubulk'):
        relative = stem.with_suffix(suffix)
        source = PROJECT / 'Content' / relative
        if suffix != '.uasset' and not source.is_file():
            continue
        require(source.is_file(), 'Existing target is missing: ' + str(source))
        current_sha = sha256(source)
        original = BACKUP_ROOT / relative
        original_existed = original.exists()
        if original_existed:
            require(original.is_file(), 'Backup is not a file: ' + str(original))
            original_sha = sha256(original)
        else:
            copy_new_verified(source, original, current_sha)
            original_sha = current_sha
        # A repeat import may legitimately have a different current uasset.
        # Keep both versions; never demand that the first backup equals it.
        current_copy = original
        if original_sha != current_sha:
            current_copy = BACKUP_ROOT / '_PreviousImports' / current_sha / relative
        copy_new_verified(source, current_copy, current_sha)
        require(sha256(original) == original_sha,
                'Original backup changed: ' + str(original))
        records.append({
            'source': str(source), 'source_sha256': current_sha,
            'original_backup': str(original), 'original_sha256': original_sha,
            'original_already_existed': original_existed,
            'original_matches_current': original_sha == current_sha,
            'current_backup': str(current_copy), 'current_backup_sha256': current_sha,
            'verified': True,
        })
    return records


def nanite_snapshot(mesh):
    settings = mesh.get_editor_property('nanite_settings')
    values = {name: settings.get_editor_property(name) for name in NANITE_FIELDS}
    # UE versions exposing this setting must preserve it too. Older versions
    # use their platform default implicitly.
    try:
        values['generate_fallback'] = settings.get_editor_property('generate_fallback')
    except Exception:
        pass
    return values


def serializable_settings(values):
    return {key: value if isinstance(value, (bool, int, float, str)) else str(value)
            for key, value in values.items()}


def import_options():
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
    return options


def current_material_bindings(mesh):
    """Retain authored emitter/glass bindings as well as Quality93 finishes."""
    bindings = {}
    for slot in mesh.get_editor_property('static_materials'):
        material = slot.get_editor_property('material_interface')
        if material is None:
            continue
        for field in ('imported_material_slot_name', 'material_slot_name'):
            family = str(slot.get_editor_property(field)).removeprefix('M_')
            if family in ('', 'None'):
                continue
            target = material.get_path_name()
            require(family not in bindings or bindings[family] == target,
                    'Ambiguous existing material binding: ' + family)
            bindings[family] = target
    return bindings


def bind_materials(mesh, item, previous_bindings):
    slots = list(mesh.get_editor_property('static_materials'))
    require(bool(slots), 'Imported mesh has no materials: ' + item['name'])
    families = item.get('materials')
    bindings = []
    for slot in slots:
        family = str(slot.get_editor_property('imported_material_slot_name')).removeprefix('M_')
        if families is not None and family not in families:
            family = str(slot.get_editor_property('material_slot_name')).removeprefix('M_')
        require(re.fullmatch(r'[A-Za-z0-9_]+', family) is not None,
                'Invalid imported material family: ' + repr(family))
        require(families is None or family in families,
                'Unexpected material family for ' + item['name'] + ': ' + family)
        # Existing Explore85 emitters use this time-controlled material alias.
        material_family = 'NightFixtureGlow' if family == 'Glow' else family
        quality_path = MATERIALS + '/Quality93Final/M_' + material_family
        target = previous_bindings.get(family)
        if target is None:
            target = quality_path if ASSETS.does_asset_exist(quality_path) else MATERIALS + '/M_' + material_family
        material = ASSETS.load_asset(target)
        require(material is not None, 'Missing existing material: ' + target)
        slot.set_editor_property('material_interface', material)
        bindings.append({'family': family, 'asset': target,
                         'preserved_existing_binding': family in previous_bindings})
    mesh.set_editor_property('static_materials', slots)
    return bindings


def checked_bounds(mesh, expected, name):
    description = mesh.get_static_mesh_description(0)
    require(description is not None and description.get_vertex_count() > 0,
            'Imported mesh has no source vertices: ' + name)
    lower, upper = [math.inf] * 3, [-math.inf] * 3
    for index in range(description.get_vertex_count()):
        vertex = description.get_vertex_position(unreal.VertexID(id_value=index))
        for axis, value in enumerate((vertex.x, vertex.y, vertex.z)):
            require(math.isfinite(value), 'Nonfinite vertex in ' + name)
            lower[axis] = min(lower[axis], value)
            upper[axis] = max(upper[axis], value)
    actual = [lower, upper]
    error = max(abs(actual[side][axis] - expected[side][axis])
                for side in range(2) for axis in range(3))
    require(error < BOUNDS_TOLERANCE_CM,
            'Bounds mismatch for {}: actual={}, expected={}, max_error_cm={}'.format(
                name, actual, expected, error))
    return actual, error


def run(report, persist):
    manifest_path = SOURCE_ART / 'craft95-interiors.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf8'))
    items = manifest['assets']
    require(isinstance(items, list) and len(items) == len(TARGETS),
            'Manifest must contain exactly the four reviewed interior meshes')
    require({item['name'] for item in items} == set(TARGETS),
            'Manifest contains missing, duplicate or unreviewed mesh names')
    report['manifest'] = str(manifest_path)
    report['manifest_sha256'] = sha256(manifest_path)
    prepared = []
    for item in items:
        name = item['name']
        require(item['target_name'] == TARGETS[name], 'Unexpected target for ' + name)
        require(item['file'] == 'SM_' + name + '.fbx', 'Unexpected source filename for ' + name)
        require(re.fullmatch(r'[0-9a-fA-F]{64}', item['sha256']) is not None,
                'Invalid SHA256 for ' + name)
        source = SOURCE_ART / 'Meshes' / item['file']
        require(sha256(source) == item['sha256'].lower(), 'Source SHA256 mismatch: ' + str(source))
        bounds = item['ue_bounds_cm']
        require(len(bounds) == 2 and all(len(side) == 3 for side in bounds)
                and all(math.isfinite(value) for side in bounds for value in side)
                and all(bounds[0][axis] <= bounds[1][axis] for axis in range(3)),
                'Invalid expected bounds for ' + name)
        require(isinstance(item['triangles'], int) and item['triangles'] > 0,
                'Invalid source triangle count for ' + name)
        destination = KIT + '/SM_' + item['target_name']
        existing = ASSETS.load_asset(destination)
        require(isinstance(existing, unreal.StaticMesh), 'Existing static mesh is missing: ' + destination)
        before_nanite = nanite_snapshot(existing)
        previous_bindings = current_material_bindings(existing)
        prepared.append((item, source, destination, before_nanite, previous_bindings))
    # Finish every backup before the first asset is changed in memory.
    for item, source, destination, before_nanite, previous_bindings in prepared:
        report['backups'].extend(preserve_target(item['target_name']))
        persist()

    staged = []
    for item, source, destination, before_nanite, previous_bindings in prepared:
        report['active_mesh'] = item['name']
        report['phase'] = 'import_and_validate'
        persist()
        require(sha256(source) == item['sha256'].lower(), 'Source changed after preflight: ' + str(source))
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = KIT
        task.destination_name = 'SM_' + item['target_name']
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = False
        task.options = import_options()
        TOOLS.import_asset_tasks([task])
        objects = task.get_objects()
        require(len(objects) == 1 and isinstance(objects[0], unreal.StaticMesh),
                'Expected one imported static mesh for ' + item['name'])
        mesh = objects[0]
        require(mesh.get_path_name().split('.')[0] == destination,
                'Importer returned an unexpected destination: ' + mesh.get_path_name())
        bindings = bind_materials(mesh, item, previous_bindings)
        settings = mesh.get_editor_property('nanite_settings')
        for field, value in before_nanite.items():
            settings.set_editor_property(field, value)
        settings.set_editor_property('enabled', True)
        mesh.set_editor_property('nanite_settings', settings)
        after_nanite = nanite_snapshot(mesh)
        for field, value in before_nanite.items():
            require(field == 'enabled' or after_nanite[field] == value,
                    'Nanite setting changed unexpectedly: ' + field)
        require(after_nanite['enabled'], 'Nanite is disabled: ' + destination)
        bounds, error = checked_bounds(mesh, item['ue_bounds_cm'], item['name'])
        ASSETS.set_metadata_tag(mesh, 'EndlessWorld.SourceSHA256', item['sha256'].lower())
        ASSETS.set_metadata_tag(mesh, 'EndlessWorld.Craft95', 'interior-geometry-v1')
        row = {
            'name': item['name'], 'target_name': item['target_name'], 'asset': destination,
            'source_sha256': item['sha256'].lower(), 'source_triangles': item['triangles'],
            'bounds_cm': bounds, 'bounds_max_error_cm': error,
            'bounds_tolerance_cm': BOUNDS_TOLERANCE_CM, 'materials': bindings,
            'nanite_before': serializable_settings(before_nanite),
            'nanite_after': serializable_settings(after_nanite),
            'automatic_collision': False, 'validated': True, 'saved': False,
        }
        report['meshes'].append(row)
        staged.append((mesh, row))
        persist()

    require(len(staged) == len(TARGETS), 'Not every target was validated')
    report['phase'] = 'save_validated_meshes'
    persist()
    for mesh, row in staged:
        report['active_mesh'] = row['name']
        persist()
        require(ASSETS.save_loaded_asset(mesh), 'Asset save failed: ' + row['asset'])
        row['saved'] = True
        persisted_asset = PROJECT / 'Content' / 'EndlessWorld' / 'Kit' / ('SM_' + row['target_name'] + '.uasset')
        row['saved_uasset_sha256'] = sha256(persisted_asset)
        persist()
        unreal.log('CRAFT95_INTERIOR_IMPORTED ' + row['target_name'])
    require(all(row['saved'] for row in report['meshes']), 'Not every target was saved')
    report['active_mesh'] = None
    report['phase'] = 'complete'
    report['success'] = True
    persist()


def main():
    command_line = unreal.SystemLibrary.get_command_line()
    matches = re.findall(r'(?:^|\s)-EWImportReport=(?:"([^"]+)"|(\S+))', command_line)
    require(len(matches) == 1, 'Provide exactly one -EWImportReport=<new absolute JSON path>')
    report_path = Path(matches[0][0] or matches[0][1])
    require(report_path.is_absolute(), 'EWImportReport must be an absolute path')
    require(report_path.suffix.lower() == '.json', 'EWImportReport must end in .json')
    require(not report_path.exists(), 'Preserve previous import evidence: ' + str(report_path))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {
        'revision': 95, 'success': False, 'phase': 'preflight',
        'scope': 'Four existing Explore85 interior meshes; existing material assets reused.',
        'backups': [], 'meshes': [], 'active_mesh': None,
    }
    # Exclusive creation reserves this evidence path. Every subsequent update
    # belongs to this invocation, and starts from a truthful success=False state.
    with report_path.open('x', encoding='utf8') as output:
        def persist():
            payload = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False)
            output.seek(0)
            output.write(payload + '\n')
            output.truncate()
            output.flush()
            os.fsync(output.fileno())

        persist()
        try:
            run(report, persist)
        except BaseException as exc:
            report['success'] = False
            report['failed_phase'] = report['phase']
            report['phase'] = 'failed'
            report['error_type'] = type(exc).__name__
            report['error'] = str(exc)
            report['traceback'] = traceback.format_exc()
            report['saved_assets'] = [row['asset'] for row in report['meshes'] if row['saved']]
            report['recovery_note'] = 'Original and current pre-import backups are retained; no automatic rollback was attempted.'
            try:
                persist()
            except Exception as report_error:
                unreal.log_error('CRAFT95_INTERIORS_REPORT_WRITE_FAILED ' + repr(report_error))
            unreal.log_error('CRAFT95_INTERIORS_FAILED ' + str(exc))
            raise
    unreal.log('CRAFT95_INTERIORS_COMPLETE ' + str(report_path))


if __name__ == '__main__':
    main()
