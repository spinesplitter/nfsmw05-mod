"""Fit private per-car rigs from installed geometry; never modify game meshes.

Uses the existing offline geometry reader via --reader-dir. Outputs proprietary
derived rigs locally, not source-controlled assets. Hide reset is byte-preserving
for every line other than Hide=, with a verified timestamped backup first.
"""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import sys


def reset_hidden(path):
    original = path.read_bytes()
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup = path.with_name(path.name + '.before-all-car-ik-' + stamp + '.bak')
    shutil.copy2(path, backup)
    if backup.read_bytes() != original:
        raise RuntimeError('INI backup verification failed')
    lines = original.splitlines(keepends=True)
    changed = 0
    for i, line in enumerate(lines):
        match = re.match(rb'([ \t]*Hide[ \t]*=)[^\r\n]*(\r?\n)?$', line, re.I)
        if match:
            lines[i] = match[1] + (match[2] or b'')
            changed += lines[i] != line
    result = b''.join(lines)
    keep = lambda data: [line for line in data.splitlines(keepends=True)
                         if not re.match(rb'[ \t]*Hide[ \t]*=', line, re.I)]
    if keep(original) != keep(result):
        raise RuntimeError('Non-Hide INI content changed')
    path.write_bytes(result)
    if path.read_bytes() != result:
        raise RuntimeError('INI verification failed')
    return str(backup), changed


def fit(data, reference, target):
    result = bytearray(data)
    count, controls = struct.unpack_from('<II', data, 8)
    def mapped(p):
        # Corresponding topology supplies local displacement, not bounding-box
        # scaling (which incorrectly moves wrists when only legs change).
        nearest = sorted(range(len(reference)), key=lambda i:
                         sum((reference[i][a]-p[a])**2 for a in range(3)))[:12]
        weights = [1 / max(0.000025, sum((reference[i][a]-p[a])**2 for a in range(3)))**2
                   for i in nearest]
        total = sum(weights)
        return tuple(p[a] + sum(w*(target[i][a]-reference[i][a])
                     for i,w in zip(nearest,weights))/total for a in range(3))
    for b in range(count):
        offset = 40 + b*124
        for delta in (36,48):
            p = struct.unpack_from('<3f',data,offset+delta)
            struct.pack_into('<3f',result,offset+delta,*mapped(p))
        # Preserve the exported rest basis; update its translation for tooling.
        head = struct.unpack_from('<3f',result,offset+36)
        for axis in range(3):
            struct.pack_into('<f',result,offset+60+(axis*4+3)*4,head[axis])
    for c in range(controls):
        offset = 40+count*124+c*44+32
        struct.pack_into('<3f',result,offset,*mapped(struct.unpack_from('<3f',data,offset)))
    return result


def mirror_rig(data):
    """Reflect across model Y, preserving proper rest bases and anatomical sides."""
    magic, version, count, controls = struct.unpack_from('<4sIII', data)
    if magic != b'IKR1' or version != 1 or len(data) != 40+count*124+controls*44:
        raise ValueError('Invalid rig payload')
    result = bytearray(data)
    arms = struct.unpack_from('<6I', data, 16)
    struct.pack_into('<6I', result, 16, *(arms[3:]+arms[:3]))
    signs = (1, -1, 1, 1)
    for i in range(count):
        offset = 40+i*124
        for delta in (36, 48):
            y = struct.unpack_from('<f', data, offset+delta+4)[0]
            struct.pack_into('<f', result, offset+delta+4, -y)
        for row in range(4):
            for col in range(4):
                pos = offset+60+(row*4+col)*4
                value = struct.unpack_from('<f', data, pos)[0]
                struct.pack_into('<f', result, pos, value*signs[row]*signs[col])
        name = data[offset:offset+32].split(b'\0')[0]
        if name.endswith((b'.L', b'.R')):
            name = name[:-1]+(b'R' if name.endswith(b'.L') else b'L')
            result[offset:offset+32] = name.ljust(32, b'\0')
    for i in range(controls):
        pos = 40+count*124+i*44+36
        struct.pack_into('<f', result, pos, -struct.unpack_from('<f', data, pos)[0])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--reader-dir', required=True)
    parser.add_argument('--reset-hidden', action='store_true')
    args = parser.parse_args()
    sys.path.insert(0,args.reader_dir)
    import inspect_cockpit_gaps as reader
    reader.ROOT = args.game
    models = {}
    for folder in sorted((args.game/'CARS').iterdir()):
        if not (folder/'GEOMETRY.BIN').exists():
            continue
        for name, vertices, triangles in reader.interiors(folder.name,lambda n:n.endswith('_DRIVER_A')):
            models[folder.name]=(name,vertices,triangles)
    _, reference, reftris = models['GALLARDO']
    canonical = {tuple(sorted(t)) for t in reftris}
    data = (args.game/'DRIVERMOD/driver-ik.bin').read_bytes()
    output = args.game/'DRIVERMOD/rigs'
    output.mkdir(exist_ok=True)
    report = {'fitted':[], 'unsupported':[]}
    for car,(name,vertices,triangles) in models.items():
        overlap=len(canonical & {tuple(sorted(t)) for t in triangles})/len(canonical)
        if len(vertices)!=len(reference) or overlap<0.98:
            report['unsupported'].append({'car':car,'reason':'incompatible native topology','overlap':overlap})
            continue
        fitted = data if car=='GALLARDO' else fit(data,reference,vertices)
        path=output/(car+'.bin')
        if path.exists():
            backup=output/('backup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
            backup.mkdir()
            shutil.copy2(path,backup/path.name)
        path.write_bytes(fitted)
        report['fitted'].append({'car':car,'model':name,'control_hand':'left' if car in ('CLIO','MONARO') else 'right',
                                'sha256':hashlib.sha256(fitted).hexdigest(),'topology_overlap':overlap})
    if args.reset_hidden:
        backup,changed=reset_hidden(args.game/'scripts/nfsmw_vr_edits.ini')
        report['ini_backup']=backup
        report['hide_lines_cleared']=changed
    (output/'fit-report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()