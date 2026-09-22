"""Fail-closed local release inventory. Never scans personal data or uploads files.

Only explicit source roots are exported; Content/SourceArt additionally require
the reviewed provenance ledger. Unknown creative files stop the release.
"""
from pathlib import Path
import argparse, hashlib, json, re

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = {'.gitignore','.gitattributes','LICENSE','ASSET-LICENSE','NOTICE',
    'README.md','README.ja.md','CONTRIBUTING.md','GAMEPLAY-VIDEO-PERMISSION.md',
    'THIRD-PARTY-NOTICES.md','PLAY.cmd','setup.ps1','build.ps1','asset-sources.json'}
SOURCE_ROOTS = ['Docs','Licenses','ThirdPartySources','Tools','Project/Source','Project/Config',
    'Project/WorldWorkshop','Project/Plugins/EWGamepad','Project/Plugins/EWEditorTools',
    'Project/CinemaOnline']
SKIP_PARTS = {'Binaries','Intermediate','Saved','DerivedDataCache','__pycache__',
    'Generated','node_modules','.git'}
FORBIDDEN_SUFFIXES = {'.blend','.blend1','.pyc','.pdb','.log','.db','.sqlite','.sqlite3','.mp4','.webm','.mov'}
PRIVATE_PATH = re.compile(rb'(?:[CF]:[/\\]+Users[/\\]+onigiri[/\\]|F:[/\\]+Codex[/\\])',re.I)
TOKEN = re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)')

def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding='utf-8')

def listed_tree(rel):
    folder = ROOT/rel
    if not folder.exists():
        raise ValueError('Missing required source root: '+rel)
    return [p for p in folder.rglob('*') if p.is_file() and not SKIP_PARTS.intersection(p.relative_to(folder).parts)]

def scan(path, rel):
    issues=[]
    if path.is_symlink() or not path.resolve().is_relative_to(ROOT.resolve()):
        issues.append('link or path outside public copy')
    if path.suffix.lower() in FORBIDDEN_SUFFIXES:
        issues.append('excluded file type')
    if path.name.lower() in {'eos.json','cookies','history','login data'}:
        issues.append('private runtime data')
    data=path.read_bytes()
    # ASCII and UTF-16 strings occur in UE metadata; strip NULs for the latter.
    for candidate in [data, data.replace(b'\x00',b'')]:
        if PRIVATE_PATH.search(candidate):
            issues.append('private absolute source path'); break
    if TOKEN.search(data):
        issues.append('credential-like token')
    # Diagnostic output contains expressions naming credentials, but no values
    # are ever emitted by this check.
    if path.suffix.lower()=='.json':
        try:
            obj=json.loads(data.decode('utf-8-sig'))
            def visit(value):
                if isinstance(value,dict):
                    for key,item in value.items():
                        if key.lower().replace('_','') in {'clientsecret','password','accesstoken','refreshtoken','apikey'} and item:
                            issues.append('nonempty credential field')
                        visit(item)
                elif isinstance(value,list):
                    for item in value: visit(item)
            visit(obj)
        except (ValueError,UnicodeError):
            issues.append('invalid JSON')
    return sorted(set(issues))

def source_audit():
    ledger=json.loads((ROOT/'Tools/Art/provenance.json').read_text(encoding='utf-8'))
    if ledger['unknown']:
        raise ValueError('Unreviewed creative origins remain')
    origin={r['file']:r for r in ledger['content_assets']+ledger['source_art']}
    audio=json.loads((ROOT/'Tools/Audio/provenance.json').read_text())
    if not audio['success'] or audio['recorded_samples_used'] or digest(ROOT/'Tools/Audio/compose_music89.py')!=audio['generator_sha256']:
        raise ValueError('Procedural audio provenance mismatch')
    origin.update({r['file']:r for r in audio['files']})
    for row in audio['files']:
        if row.get('sha256') and digest(ROOT/row['file'])!=row['sha256']:
            raise ValueError('Audio source changed after verification')
    if any(r['classification']=='delegated-audio' for r in origin.values()):
        raise ValueError('Unresolved audio ledger')
    imported=json.loads((ROOT/'asset-sources.json').read_text(encoding='utf-8'))['assets']
    editor=json.loads((ROOT/'Local/Reports/editor-asset-audit.json').read_text())
    if not editor['success'] or len(editor['rebound_sources']) != len(imported):
        raise ValueError('Editor import audit is incomplete')
    files={ROOT/r for r in ROOT_FILES}
    files.add(ROOT/'Project/EndlessWorld.uproject')
    for root in SOURCE_ROOTS: files.update(listed_tree(root))
    excluded=[]
    problems=[]
    for prefix in ['Project/Content','Project/SourceArt']:
        for path in listed_tree(prefix):
            rel=path.relative_to(ROOT).as_posix()
            row=origin.get(rel)
            if not row:
                problems.append({'file':rel,'issues':['creative file absent from reviewed provenance']});continue
            if row['classification']=='engine-generated-not-source-archive':
                excluded.append(rel);continue
            if prefix.endswith('SourceArt') and row.get('sha256') != digest(path):
                problems.append({'file':rel,'issues':['source changed since provenance audit']})
            files.add(path)
    rows=[]
    for path in sorted(files):
        rel=path.relative_to(ROOT).as_posix()
        if not path.is_file():
            problems.append({'file':rel,'issues':['required file missing']});continue
        issues=scan(path,rel)
        if issues: problems.append({'file':rel,'issues':issues})
        asset=rel.startswith(('Project/Content/','Project/SourceArt/'))
        info=origin.get(rel)
        license_name=info.get('license') if info else ('Third-party terms' if rel.startswith(('Licenses/','ThirdPartySources/','Project/Plugins/EWGamepad/ThirdParty/')) else 'PolyForm-Noncommercial-1.0.0')
        rows.append({'file':rel,'sha256':digest(path),'bytes':path.stat().st_size,
                     'delivery':'assets' if asset and not rel.startswith('Project/SourceArt/Scripts/') else 'git','license':license_name,
                     'origin':info.get('origin') if info else 'Reviewed exported project source or notices'})
    for row in imported:
        if digest(ROOT/row['source'])!=row['source_sha256']:
            problems.append({'file':row['source'],'issues':['import-source hash mismatch']})
    report={'version':1,'success':not problems,'files':rows,'excluded_engine_source_assets':excluded,
            'problems':problems,'scope':'Explicit file and provenance allowlist, metadata and secret-pattern audit; separate runtime and visual verification required.'}
    write(ROOT/'public-inventory.json',report)
    return report

def windows_audit(folder,stage):
    problems=[];rows=[]
    if not stage:raise ValueError('--stage-root is required for runtime allowlist verification')
    expected={}
    for name in ['Manifest_NonUFSFiles_Win64.txt','Manifest_UFSFiles_Win64.txt']:
        manifest=stage/name
        if not manifest.is_file():raise FileNotFoundError(manifest)
        # UAT archives its own manifests beside the runtime; verify them against
        # the stage just like their enumerated files.
        expected['Windows/'+name]=manifest
        for line in manifest.read_text(encoding='utf-8-sig').splitlines():
            rel=line.split('\t',1)[0]
            source=(stage/rel).resolve()
            if not source.is_relative_to(stage.resolve()):raise ValueError('Invalid UAT manifest path')
            if source.is_file():expected['Windows/'+rel.replace('\\','/')]=source
    for name in ['EndlessWorld-Windows.pak','EndlessWorld-Windows.ucas','EndlessWorld-Windows.utoc','global.ucas','global.utoc']:
        rel='EndlessWorld/Content/Paks/'+name
        if not (stage/rel).is_file():raise FileNotFoundError(stage/rel)
        expected['Windows/'+rel]=stage/rel
    for name in ['PLAY.cmd','NOTICE','LICENSE','ASSET-LICENSE','THIRD-PARTY-NOTICES.md','GAMEPLAY-VIDEO-PERMISSION.md']:
        expected[name]=ROOT/name
    for src,dst in [('PLAY-JA.md','README-JA.md'),('PLAY-EN.md','README-EN.md'),('END-USER-TERMS.md','END-USER-TERMS.md')]:
        expected[dst]=ROOT/'Docs'/src
    for name in ['Licenses','ThirdPartySources']:
        for source in listed_tree(name):expected[source.relative_to(ROOT).as_posix()]=source
    required=['PLAY.cmd','NOTICE','LICENSE','ASSET-LICENSE','THIRD-PARTY-NOTICES.md','Windows/EndlessWorld.exe']
    for rel in required:
        if not (folder/rel).is_file():problems.append({'file':rel,'issues':['required file missing']})
    for path in sorted(folder.rglob('*')):
        if not path.is_file():continue
        rel=path.relative_to(folder).as_posix()
        forbidden = {'Saved','Intermediate','DerivedDataCache','__pycache__','.git','PlayData'}.intersection(path.relative_to(folder).parts) or path.suffix.lower() in FORBIDDEN_SUFFIXES or path.name.lower() in {'eos.json','cookies','history'}
        if forbidden:problems.append({'file':rel,'issues':['cache, source, media or private data in runtime']})
        value=digest(path)
        if rel not in expected:problems.append({'file':rel,'issues':['loose file absent from UAT and notice allowlists']})
        elif value!=digest(expected[rel]):problems.append({'file':rel,'issues':['staged file changed after build']})
        if path.suffix.lower() in {'.json','.ini','.mjs','.txt','.cfg'}:
            data=path.read_bytes()
            if TOKEN.search(data):problems.append({'file':rel,'issues':['credential-like token']})
        rows.append({'file':rel,'sha256':value,'bytes':path.stat().st_size})
    actual={r['file'] for r in rows}
    for rel in set(expected)-actual:problems.append({'file':rel,'issues':['UAT or notice allowlist member missing']})
    report={'success':not problems,'root':str(folder.resolve()),'files':rows,'problems':problems,
            'scope':'Fresh staged runtime inventory; cooked package contents require the separate cook manifest comparison.'}
    write(ROOT/'Local/Reports/windows-audit.json',report)
    return report

def main():
    p=argparse.ArgumentParser();p.add_argument('--windows-root',type=Path);p.add_argument('--stage-root',type=Path);a=p.parse_args()
    report=windows_audit(a.windows_root,a.stage_root) if a.windows_root else source_audit()
    print(json.dumps({'success':report['success'],'file_count':len(report['files']),'problems':report['problems']},ensure_ascii=False,indent=2))
    if not report['success']:raise SystemExit(1)

if __name__=='__main__': main()
