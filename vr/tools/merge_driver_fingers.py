"""Mark verified secondary native finger submeshes without manual picking.

Dry-run by default. Uses the existing local geometry reader, never edits meshes.
Only fitted full-detail drivers matching the reference finger topology qualify.
"""
import argparse
import datetime
import json
from pathlib import Path
import re
import shutil
import struct
import sys


def finger_meshes(path, chunks):
    data = path.read_bytes()
    for tag, start, end in chunks(data):
        if tag != 0x80134000:
            continue
        for tag, start, end in chunks(data, start, end):
            if tag != 0x80134010:
                continue
            parts = list(chunks(data, start, end))
            header = next((data[a:b] for t, a, b in parts if t == 0x134011), b'')
            name = header[160:].split(b'\0')[0].decode('ascii', errors='strict')
            if not name.endswith('_DRIVER_A'):
                continue
            mesh = next((list(chunks(data, a, b)) for t, a, b in parts
                         if t == 0x80134100), [])
            materials = [data[a:b].split(b'\0')[0] for t, a, b in mesh if t == 0x134C02]
            payload = {}
            for t, a, b in mesh:
                if t not in (0x134B02, 0x134B03):
                    continue
                while data[a:a+4] == b'\x11'*4:
                    a += 4
                payload[t] = data[a:b]
            entries, indices = payload.get(0x134B02, b''), payload.get(0x134B03, b'')
            if len(entries) != 2*0x68 or materials != [b'DRIVER_PLAYER']*2:
                raise ValueError(name + ': unexpected submesh/material layout')
            count, first = struct.unpack_from('<II', entries, 0x68+0x40)
            index_count = struct.unpack_from('<I', entries, 0x68+0x5C)[0]
            main_count, main_first = struct.unpack_from('<II', entries, 0x40)
            if count != 168 or index_count != count*3 or first != main_first+main_count*3:
                raise ValueError(name + ': unexpected finger range')
            if (first+count*3)*2 > len(indices):
                raise ValueError(name + ': finger indices outside buffer')
            values = struct.unpack_from('<'+'H'*(count*3), indices, first*2)
            triangles = {tuple(sorted(values[i:i+3])) for i in range(0, len(values), 3)}
            if len(triangles) != count or any(len(set(t)) != 3 for t in triangles):
                raise ValueError(name + ': duplicate or degenerate finger triangles')
            yield name, triangles


def parse_triangles(value):
    return {tuple(sorted(map(int, match))) for match in
            re.findall(r'(\d+)\.(\d+)\.(\d+)', value)}


def compatible_fingers(triangles, canonical):
    # Some stock cars triangulate ten faces differently but use the same hand
    # vertex island. Require both that island and at least 90% identical faces.
    return ({v for t in triangles for v in t} == {v for t in canonical for v in t}
            and len(triangles) == len(canonical)
            and len(triangles & canonical) >= len(canonical)*0.9)


def merge_marks(original, models):
    # latin-1 is a reversible byte mapping: unrelated lines stay byte-identical.
    text = original.decode('latin-1')
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.splitlines(keepends=True)
    report = []
    for name, triangles in sorted(models.items()):
        sections = [(i, m[1]) for i, line in enumerate(lines)
                    if (m := re.match(r'\s*\[([^\]]+)\]', line))]
        matches = [i for i, section in sections if section.casefold() == name.casefold()]
        if len(matches) > 1:
            raise ValueError('Duplicate INI section: ' + name)
        if matches:
            start = matches[0]
            end = next((i for i, _ in sections if i > start), len(lines))
        else:
            if lines and not lines[-1].endswith(('\r', '\n')):
                lines[-1] += newline
            start = len(lines)
            lines.append('['+name+']'+newline)
            end = len(lines)
        keys = {}
        for i in range(start+1, end):
            match = re.match(r'([ \t]*)(Arms|Skin|Show|Native|Black|Hide)([ \t]*=)([^\r\n]*)(\r?\n)?$', lines[i], re.I)
            if match:
                key = match[2].lower()
                if key in keys:
                    raise ValueError('Duplicate INI key: '+name+'/'+key)
                keys[key] = (i, match)
        native = parse_triangles(keys['native'][1][4]) if 'native' in keys else set()
        eligible = triangles - native
        changes = 0
        additions = []
        for key in ('arms', 'skin', 'show', 'black', 'hide'):
            old = parse_triangles(keys[key][1][4]) if key in keys else set()
            new = old | eligible if key in ('arms', 'skin', 'show') else old - eligible
            if new == old:
                continue
            changes += len(new ^ old)
            if key == 'hide':
                # Hide stores original winding. Remove only matched triangles.
                value = keys[key][1][4]
                value = re.sub(r'\d+\.\d+\.\d+', lambda m:
                               '' if tuple(sorted(map(int, m[0].split('.')))) in eligible
                               else m[0], value)
            else:
                value = ' '.join('.'.join(map(str, t)) for t in sorted(new))
            if key in keys:
                i, match = keys[key]
                lines[i] = match[1]+match[2]+match[3]+value+(match[5] or '')
            else:
                additions.append(key.capitalize()+'='+value+newline)
        if additions:
            if end and not lines[end-1].endswith(('\r', '\n')):
                lines[end-1] += newline
            lines[end:end] = additions
        report.append({'model': name, 'triangles': len(triangles),
                       'native_opt_outs': len(triangles & native), 'mark_changes': changes})
    return ''.join(lines).encode('latin-1'), report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--reader-dir', required=True)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, args.reader_dir)
    from inspect_cockpit_gaps import chunks
    reference = list(finger_meshes(args.game/'CARS/GALLARDO/GEOMETRY.BIN', chunks))
    if len(reference) != 1:
        raise ValueError('Expected one reference full-detail driver')
    canonical = reference[0][1]
    models, skipped = {}, []
    for rig in sorted((args.game/'DRIVERMOD/rigs').glob('*.bin')):
        car = rig.stem
        try:
            found = list(finger_meshes(args.game/'CARS'/car/'GEOMETRY.BIN', chunks))
            if len(found) != 1:
                raise ValueError('Expected one full-detail driver')
            if not compatible_fingers(found[0][1], canonical):
                overlap = len(found[0][1] & canonical)/len(canonical)
                vertices = {v for t in found[0][1] for v in t}
                reference_vertices = {v for t in canonical for v in t}
                raise ValueError('finger topology differs from reference: overlap='+str(overlap)+
                                 ', same vertices='+str(vertices == reference_vertices))
            models[found[0][0]] = found[0][1]
        except (ValueError, OSError, struct.error) as error:
            skipped.append({'car': car, 'reason': str(error)})
    path = args.game/'scripts/nfsmw_vr_edits.ini'
    original = path.read_bytes()
    result, entries = merge_marks(original, models)
    report = {'applied': False, 'models': entries, 'skipped': skipped}
    if args.apply and result != original:
        # Refuse a live game: cached edit state could overwrite the new marks.
        import subprocess
        state = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
            "$ErrorActionPreference='Stop'; $g='"+str(args.game).replace("'", "''")+"'; @(Get-Process | Where-Object { $_.ProcessName -match '^(speed|speedblack|nfsmw)$' -or ($_.Path -and $_.Path.StartsWith($g,[StringComparison]::OrdinalIgnoreCase)) }).Count"],
            check=True, capture_output=True, text=True)
        if state.stdout.strip() != '0':
            raise RuntimeError('Game may be running; close it before applying marks')
        backup = path.with_name(path.name+'.before-submesh-merge-'+
                               datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'.bak')
        shutil.copy2(path, backup)
        if backup.read_bytes() != original or path.read_bytes() != original:
            raise RuntimeError('Backup verification failed or INI changed during audit')
        path.write_bytes(result)
        if path.read_bytes() != result:
            raise RuntimeError('INI verification failed')
        report.update(applied=True, backup=str(backup))
    report['changed'] = result != original
    args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps({'models': len(entries), 'triangles': sum(x['triangles'] for x in entries),
                      'skipped': len(skipped), 'applied': report['applied'],
                      'changed': report['changed']}))


if __name__ == '__main__':
    main()