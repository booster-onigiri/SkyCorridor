"""Remove personal source-file paths from FBX SceneInfo metadata only.

The replacement has identical encoded length. Every node offset, geometry array,
material value and topology byte therefore remains untouched. External texture
references are rejected, not silently rewritten.
"""
from pathlib import Path
import hashlib
import json
import argparse
from audit_fbx import inspect


def scrub(path: Path) -> dict | None:
    result = inspect(path)
    fields = result['external_absolute_strings']
    if not fields:
        return None
    before = path.read_bytes()
    after = bytearray(before)
    changes = []
    for field in fields:
        if not field['node'].startswith('/FBXHeaderExtension/SceneInfo/'):
            raise ValueError('Unreviewed external path node: ' + field['node'])
        filename = field['value'].rsplit('/', 1)[-1]
        replacement = ('SourceArt/' + filename).encode('utf-8')
        start, length = field['byte_offset'], field['byte_length']
        if len(replacement) > length:
            raise ValueError('Cannot preserve FBX metadata length')
        after[start:start+length] = replacement.ljust(length, b' ')
        changes.append({'node': field['node'], 'offset': start, 'bytes': length,
                        'replacement': replacement.decode('utf-8')})
    assert len(before) == len(after)
    intervals = [(x['offset'], x['offset'] + x['bytes']) for x in changes]
    # Compare all untouched spans, not only the final byte count.
    cursor = 0
    for start, end in sorted(intervals):
        assert before[cursor:start] == after[cursor:start]
        cursor = end
    assert before[cursor:] == after[cursor:]
    path.write_bytes(after)
    post = inspect(path)
    assert not post['external_absolute_strings']
    return {'file': path.name, 'before_sha256': hashlib.sha256(before).hexdigest(),
            'after_sha256': hashlib.sha256(after).hexdigest(),
            'before_md5': hashlib.md5(before).hexdigest(), 'after_md5': hashlib.md5(after).hexdigest(),
            'metadata_only': True, 'geometry_bytes_unchanged': True, 'changes': changes}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    art = repo / 'Project/SourceArt'
    records = []
    for p in sorted(art.rglob('*.fbx')):
        entry = scrub(p)
        if entry:
            entry['file'] = p.relative_to(repo).as_posix()
            records.append(entry)
    args.report.write_text(json.dumps({'success': True, 'files': records}, indent=2), encoding='utf-8')
    print('Metadata-only source cleanup:', len(records))


if __name__ == '__main__':
    main()
