"""Import the three synthesis-only recordings into this project in Unreal Editor."""
from pathlib import Path
import hashlib, json
import unreal

ROOT = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/'Project').resolve()
folder = ROOT/'Project/SourceArt/Audio89'
provenance = json.loads((folder/'PROVENANCE.json').read_text(encoding='utf-8'))
assert provenance['recorded_samples_used'] is False
assert provenance['external_instrument_libraries_used'] is False
results=[]
for name in ('CanalAfterglow','WindowWithoutVoices','LampOnTheWayHome'):
    source=folder/'Masters-v1'/f'{name}.wav'
    task=unreal.AssetImportTask()
    task.filename=str(source)
    task.destination_path='/Game/EndlessWorld/Audio89'
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound=task.get_objects()[0]
    assert isinstance(sound,unreal.SoundWave)
    sound.set_editor_property('looping',False)
    sound.set_editor_property('compression_quality',85)
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    unreal.EditorAssetLibrary.set_metadata_tag(sound,'SkyCorridor.SourceSHA256',digest)
    unreal.EditorAssetLibrary.set_metadata_tag(sound,'SkyCorridor.AudioOrigin','procedural-synthesis-v1')
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound)
    results.append({'asset':sound.get_path_name(),'sha256':digest,'duration':sound.get_editor_property('duration')})
output=folder/'Generated'
output.mkdir(exist_ok=True)
(output/'public-audio-import.json').write_text(json.dumps({'success':True,'assets':results},indent=2),encoding='utf-8')
unreal.log('PUBLIC_AUDIO_IMPORT_COMPLETE')
