"""Build verified, bounded Release archives from an explicit audited inventory.

This does not upload anything. Generate public-inventory.json with audit-release.py
first. Existing output folders are never replaced.
"""
from pathlib import Path
import argparse, hashlib, json, re, zipfile

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 2 * 1024**3
CHUNK = 1700 * 1024**2

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def archive(output, files, root):
    expected = {}
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=5, allowZip64=True) as z:
        for row in files:
            rel = row['file']
            path = root / rel
            if sha(path) != row['sha256']:
                raise ValueError('File changed after audit: ' + rel)
            info = zipfile.ZipInfo(rel, (2026, 9, 23, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            with path.open('rb') as src, z.open(info, 'w', force_zip64=True) as dst:
                import shutil
                shutil.copyfileobj(src, dst, 1024*1024)
            expected[rel] = row['sha256']
    if output.stat().st_size >= LIMIT:
        raise ValueError('Release archive is not under 2 GiB: ' + output.name)
    with zipfile.ZipFile(output) as z:
        if set(z.namelist()) != set(expected):
            raise ValueError('Archive inventory mismatch')
        for rel, digest in expected.items():
            with z.open(rel) as f:
                if hashlib.file_digest(f, 'sha256').hexdigest() != digest:
                    raise ValueError('Archived file hash mismatch: ' + rel)
    return {'name': output.name, 'bytes': output.stat().st_size,
            'sha256': sha(output), 'files': expected}

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--kind', choices=['assets', 'source', 'windows'], required=True)
    p.add_argument('--windows-root', type=Path)
    p.add_argument('--version', default='v0.1.2')
    a = p.parse_args()
    if not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+', a.version):
        p.error('--version must be a release tag such as v0.1.2')
    a.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((ROOT/'public-inventory.json').read_text(encoding='utf-8'))
    if not manifest['success']:
        raise ValueError('Public audit did not pass')
    rows = manifest['files']
    result = []
    if a.kind == 'assets':
        selected = [r for r in rows if r['delivery'] == 'assets']
        groups, group, size = [], [], 0
        for row in selected:
            if row['bytes'] > CHUNK:
                raise ValueError('One asset exceeds archive chunk size')
            if group and size + row['bytes'] > CHUNK:
                groups.append(group); group, size = [], 0
            group.append(row); size += row['bytes']
        if group:
            groups.append(group)
        for i, group in enumerate(groups, 1):
            name = f'SkyCorridor-{a.version}-DevelopmentAssets-{i:02}.zip'
            result.append(archive(a.output/name, group, ROOT))
        for entry in result:
            entry['url'] = f'https://github.com/booster-onigiri/SkyCorridor/releases/download/{a.version}/' + entry['name']
        (ROOT/'release-assets.json').write_text(json.dumps({'version':1,'assets':result}, indent=2), encoding='utf-8')
    elif a.kind == 'source':
        selected = [r for r in rows if r['delivery'] == 'git']
        for name in ['public-inventory.json', 'release-assets.json']:
            file = ROOT/name
            selected.append({'file':name, 'sha256':sha(file), 'bytes':file.stat().st_size})
        result.append(archive(a.output/f'SkyCorridor-{a.version}-Source.zip', selected, ROOT))
    else:
        if not a.windows_root:
            p.error('--windows-root required')
        # Staging is separately checked by audit-release.py --windows-root.
        receipt = json.loads((ROOT/'Local/Reports/windows-audit.json').read_text())
        if not receipt['success'] or Path(receipt['root']).resolve() != a.windows_root.resolve():
            raise ValueError('Missing matching Windows audit')
        result.append(archive(a.output/f'SkyCorridor-{a.version}-Windows.zip', receipt['files'], a.windows_root))
    (a.output/'archive-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    (a.output/'SHA256SUMS.txt').write_text(''.join(e['sha256']+'  '+e['name']+'\n' for e in result), encoding='utf-8')
    print(json.dumps([{k:v for k,v in e.items() if k!='files'} for e in result], indent=2))

if __name__ == '__main__':
    main()
