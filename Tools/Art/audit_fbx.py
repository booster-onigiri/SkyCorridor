"""Read binary FBX node/property tables without Blender or third-party parsers.

Arrays are skipped at their recorded byte offsets, never executed or decoded.
Reports embedded-media payloads and image texture path nodes conservatively.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct


def inspect(path: Path) -> dict:
    data = path.read_bytes()
    if not data.startswith(b'Kaydara FBX Binary  \x00\x1a\x00'):
        raise ValueError('Only binary FBX is accepted: ' + str(path))
    version = struct.unpack_from('<I', data, 23)[0]
    header = '<QQQB' if version >= 7500 else '<IIIB'
    header_size = struct.calcsize(header)
    result = {'file': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
              'version': version, 'nodes': 0, 'embedded_media': [], 'texture_paths': [],
              'external_absolute_strings': []}

    def node(offset: int, parent: str = '') -> int:
        end, count, size, name_size = struct.unpack_from(header, data, offset)
        if end == 0:
            return offset + header_size
        if not offset < end <= len(data):
            raise ValueError('Invalid FBX node extent')
        cursor = offset + header_size
        name = data[cursor:cursor+name_size].decode('utf-8', 'replace')
        cursor += name_size
        prop_end = cursor + size
        trail = parent + '/' + name
        result['nodes'] += 1
        for _ in range(count):
            kind = chr(data[cursor]); cursor += 1
            if kind in {'Y', 'C', 'I', 'F', 'D', 'L'}:
                cursor += {'Y': 2, 'C': 1, 'I': 4, 'F': 4, 'D': 8, 'L': 8}[kind]
            elif kind in 'fdlibc':
                _, _, length = struct.unpack_from('<III', data, cursor)
                cursor += 12 + length
            elif kind in 'SR':
                length = struct.unpack_from('<I', data, cursor)[0]; cursor += 4
                value = data[cursor:cursor+length]
                if kind == 'R' and length and ('Video' in trail or name == 'Content'):
                    result['embedded_media'].append({'node': trail, 'bytes': length})
                if kind == 'S':
                    text = value.decode('utf-8', 'replace').replace('\\', '/')
                    if name in {'FileName', 'Filename', 'RelativeFilename'}:
                        result['texture_paths'].append({'node': trail, 'value': text})
                    if len(text) > 3 and text[1:3] == ':/':
                        result['external_absolute_strings'].append({'node': trail, 'value': text,
                                                                    'byte_offset': cursor, 'byte_length': length})
                cursor += length
            else:
                raise ValueError('Unknown FBX property type ' + kind)
        if cursor != prop_end:
            raise ValueError('FBX property-table length mismatch')
        while cursor < end - header_size:
            cursor = node(cursor, trail)
        return end

    cursor = 27
    while cursor + header_size < len(data):
        end = struct.unpack_from('<Q' if version >= 7500 else '<I', data, cursor)[0]
        if not end:
            break
        cursor = node(cursor)
    result['no_embedded_media'] = not result['embedded_media']
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    records = [inspect(path) for path in sorted(args.directory.rglob('*.fbx'))]
    result = {'success': bool(records) and all(x['no_embedded_media'] and not x['texture_paths'] for x in records),
              'scope': 'Static binary-media audit, not a legal originality determination or visual acceptance.',
              'files': records}
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'success': result['success'], 'files': len(records),
                      'embedded_files': sum(not x['no_embedded_media'] for x in records),
                      'texture_path_files': sum(bool(x['texture_paths']) for x in records)}))
    if not result['success']:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
