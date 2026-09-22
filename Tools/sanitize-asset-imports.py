"""Validate all imported creative assets and rebase their editor source filenames.

Run with the PythonScript commandlet after public audio/art imports. Source files
must exist in the exported project and match the published origin map.
"""
from pathlib import Path
import hashlib,json
import unreal

ROOT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/'Project').resolve()
mapping={row['asset']:row for row in json.loads((ROOT/'asset-sources.json').read_text())['assets']}
results=[]
unknown=[]
for row in mapping.values():
    source=(ROOT/row['source']).resolve()
    assert source.is_relative_to(ROOT/'Project/SourceArt')
    assert hashlib.sha256(source.read_bytes()).hexdigest()==row['source_sha256'],row['source']
    asset=unreal.EditorAssetLibrary.load_asset(row['asset'])
    assert asset,row['asset']
    data=asset.get_editor_property('asset_import_data')
    assert data,row['asset']
    names=data.extract_filenames()
    assert len(names)==1,(row['asset'],len(names))
    data.scripted_add_filename(str(source),0,'Public project source')
    unreal.EditorAssetLibrary.set_metadata_tag(asset,'SkyCorridor.SourceSHA256',row['source_sha256'])
    unreal.EditorAssetLibrary.set_metadata_tag(asset,'SkyCorridor.SourcePath',row['source'])
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    assert Path(data.extract_filenames()[0]).resolve()==source
    results.append({'asset':row['asset'],'source':row['source'],'sha256':row['source_sha256']})

registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
counts={}
for item in registry.get_assets_by_path('/Game',recursive=True):
    kind=str(item.asset_class_path.asset_name)
    path=str(item.package_name)
    counts[kind]=counts.get(kind,0)+1
    if kind in ('StaticMesh','SkeletalMesh','Texture2D','SoundWave') and path not in mapping:
        unknown.append({'asset':path,'class':kind})
output=ROOT/'Local/Reports'
output.mkdir(parents=True,exist_ok=True)
(output/'editor-asset-audit.json').write_text(json.dumps({
    'success':not unknown,'asset_classes':counts,'rebound_sources':results,'unknown_imports':unknown,
    'scope':'Source-file provenance and editor import references; visual originality review is separate.'
},indent=2),encoding='utf-8')
assert not unknown,unknown
unreal.log('PUBLIC_ASSET_AUDIT_COMPLETE')
