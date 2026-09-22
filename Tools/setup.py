"""Install version-pinned project assets, never a personal configuration or engine."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, shutil, subprocess, urllib.request, zipfile

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def install(entry, cache, local):
    name = entry['name']
    if Path(name).name != name:
        raise ValueError('Invalid archive name')
    archive = local / name if local else cache / name
    if not archive.exists():
        if local:
            raise FileNotFoundError(archive)
        url = entry['url']
        if not url.startswith('https://github.com/booster-onigiri/SkyCorridor/releases/download/'):
            raise ValueError('Unexpected asset host')
        temp = archive.with_suffix('.download')
        print('Downloading', name, flush=True)
        urllib.request.urlretrieve(url, temp)
        if sha(temp) != entry['sha256']:
            raise ValueError('Download hash mismatch: ' + name)
        temp.replace(archive)
    if sha(archive) != entry['sha256']:
        raise ValueError('Archive hash mismatch: ' + name)
    with zipfile.ZipFile(archive) as z:
        members = [i.filename for i in z.infolist() if not i.is_dir()]
        if len({n.casefold() for n in members}) != len(members) or set(members) != set(entry['files']):
            raise ValueError('Archive inventory differs from manifest')
        for info in z.infolist():
            rel = PurePosixPath(info.filename)
            if rel.is_absolute() or '..' in rel.parts or '\\' in info.filename or ':' in info.filename:
                raise ValueError('Unsafe archive member')
            if info.is_dir():
                continue
            if not (info.filename.startswith('Project/Content/') or info.filename.startswith('Project/SourceArt/')):
                raise ValueError('Unexpected archive member: ' + info.filename)
            target = ROOT.joinpath(*rel.parts)
            if not target.resolve().is_relative_to(ROOT.resolve()):
                raise ValueError('Asset path escapes checkout')
            expected = entry['files'].get(info.filename)
            if expected is None:
                raise ValueError('Unlisted asset: ' + info.filename)
            if target.is_file():
                if sha(target) == expected:
                    continue
                raise ValueError('Preserving edited asset; move it aside before setup: ' + str(target))
            target.parent.mkdir(parents=True, exist_ok=True)
            temp = target.with_name(target.name + '.setup-part')
            with z.open(info) as src, temp.open('wb') as dst:
                shutil.copyfileobj(src, dst)
            if sha(temp) != expected:
                raise ValueError('Asset hash mismatch: ' + info.filename)
            temp.replace(target)
    print('Verified', name, flush=True)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--assets-dir', type=Path, help='Use local Release archives instead of downloading')
    p.add_argument('--engine', type=Path, required=True, help='Installed Unreal Engine 5.8.2 root')
    p.add_argument('--graphics', choices=['baseline','nvidia'], default='baseline')
    p.add_argument('--skip-assets', action='store_true', help='Configure an already-populated project')
    a = p.parse_args()
    version = json.loads((a.engine/'Engine/Build/Build.version').read_text())
    if (version['MajorVersion'], version['MinorVersion'], version['PatchVersion']) != (5,8,2):
        raise ValueError('This release is verified against Unreal Engine 5.8.2')
    if not a.skip_assets:
        entries = json.loads((ROOT/'release-assets.json').read_text())['assets']
        if not entries:
            raise ValueError('Asset manifest is not finalized')
        cache = ROOT/'Downloads'
        cache.mkdir(exist_ok=True)
        for entry in entries:
            install(entry, cache, a.assets_dir)
    subprocess.run([os.sys.executable, str(ROOT/'Tools/configure-graphics.py'), a.graphics,
                    '--engine-dir', str(a.engine/'Engine')], check=True)
    for required in ['Project/Content/EndlessWorld/Maps/EndlessWorld.umap',
                     'Project/Plugins/EWGamepad/ThirdParty/SDL3/SDL3.dll']:
        if not (ROOT/required).is_file():
            raise FileNotFoundError(required)
    print('Ready. Build with: .\\build.ps1 -EngineRoot "' + str(a.engine) + '" -Target Editor')

if __name__ == '__main__':
    main()
