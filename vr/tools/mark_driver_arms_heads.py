"""Mark every native driver's arms/head explicitly in nfsmw_vr_edits.ini.

Mirrors what the triangle editor's "Merge fingers" and "Hide" do for one picked
model, using the same geometry classification the mod runs at load time:

  head: triangle centroid above (solid max Z - HeadSize) and behind
        (solid min X + 0.4 m)
  arms: forward of the head + 0.10 m, not more than 0.48 m below it and more
        than 0.06 m to either side; an entry that is mostly arms becomes all
        arms (the separate finger/hand submesh)

For every car except the three already hand-tuned (PUNTO, IS300, RX8) and the
two the user does by hand (MONARO, CLIO):
  Arms  += all arm triangles
  Skin  += the separate finger/hand entry
  Show  += all arm triangles
Hide += the head (original winding) for every car, including those five.

Dry run by default. Never edits GEOMETRY.BIN; backs up and verifies the INI.
"""
import argparse
import datetime
from pathlib import Path
import json
import re
import shutil
import struct
import subprocess
import sys

HEAD_SIZE = 0.25
HEAD_DEPTH = 0.4
ARMS_FORWARD = 0.10
ARMS_BELOW_HEAD = 0.48
ARMS_SIDEWAYS = 0.06
HAND_TUNED = {'PUNTO_KIT00_DRIVER_A', 'IS300_KIT00_DRIVER_A', 'RX8_KIT00_DRIVER_A'}
DO_BY_HAND = {'MONARO_KIT00_DRIVER_A', 'CLIO_KIT00_DRIVER_A'}


def chunks(data, start=0, end=None):
    end = len(data) if end is None else end
    while start + 8 <= end:
        tag, size = struct.unpack_from('<II', data, start)
        stop = start + 8 + size
        if stop > end:
            raise ValueError('invalid chunk size')
        yield tag, start + 8, stop
        start = stop


def driver_models(path):
    """Yield (name, vertices, entry_tris, min xyz, max xyz)."""
    data = path.read_bytes()
    for tag, begin, end in chunks(data):
        if tag != 0x80134000:
            continue
        for tag, begin, end in chunks(data, begin, end):
            if tag != 0x80134010:
                continue
            parts = list(chunks(data, begin, end))
            header = next((data[a:b] for t, a, b in parts if t == 0x134011), b'')
            name = header[160:].split(b'\0')[0].decode('ascii', errors='replace')
            if not name.endswith('_DRIVER_A'):
                continue
            mesh = next((list(chunks(data, a, b)) for t, a, b in parts
                         if t == 0x80134100), [])
            payload = {}
            for t, a, b in mesh:
                if t not in (0x134B01, 0x134B02, 0x134B03):
                    continue
                while data[a:a + 4] == b'\x11' * 4:
                    a += 4
                payload[t] = data[a:b]
            if not all(k in payload for k in (0x134B01, 0x134B02, 0x134B03)):
                continue
            vb, records, ib = payload[0x134B01], payload[0x134B02], payload[0x134B03]
            verts = [struct.unpack_from('<fff', vb, i * 36) for i in range(len(vb) // 36)]
            if len(records) % 0x68:
                raise ValueError(name + ': unexpected draw-entry records')
            entry_tris = []
            for i in range(len(records) // 0x68):
                count, first = struct.unpack_from('<II', records, i * 0x68 + 0x40)
                entry_tris.append([struct.unpack_from('<HHH', ib, (first + t * 3) * 2)
                                   for t in range(count)])
            min_xyz = struct.unpack_from('<fff', header, 32)
            max_xyz = struct.unpack_from('<fff', header, 48)
            yield name, verts, entry_tris, min_xyz, max_xyz


def classify(verts, entry_tris, min_xyz, max_xyz):
    top_z = max_xyz[2] - HEAD_SIZE
    back_x = min_xyz[0] + HEAD_DEPTH

    def centroid(tri):
        return tuple(sum(verts[i][a] for i in tri) / 3 for a in range(3))

    def is_head(c):
        return c[2] > top_z and c[0] < back_x

    head = []
    sx = sy = sz = 0
    head_verts = 0
    for tris in entry_tris:
        for tri in tris:
            c = centroid(tri)
            if not is_head(c):
                continue
            head.append(tri)
            for i in tri:
                sx += verts[i][0]
                sy += verts[i][1]
                sz += verts[i][2]
                head_verts += 1
    hx, hy, hz = (sx / head_verts, sy / head_verts, sz / head_verts) if head_verts else (0, 0, 0)

    body, arms, finger = [], [], []
    for entry_index, tris in enumerate(entry_tris):
        eb, ea = [], []
        for tri in tris:
            c = centroid(tri)
            if is_head(c):
                continue
            if (c[0] > hx + ARMS_FORWARD and c[2] > hz - ARMS_BELOW_HEAD and
                    abs(c[1] - hy) > ARMS_SIDEWAYS):
                ea.append(tri)
            else:
                eb.append(tri)
        if ea and len(ea) * 4 >= (len(eb) + len(ea)) * 3:
            ea += eb
            eb = []
        arms += ea
        body += eb
        # The mostly-arm entry after the first is the separate finger/hand submesh.
        if entry_index > 0 and ea and len(ea) * 4 >= (len(eb) + len(ea)) * 3:
            finger += ea
    return body, arms, head, finger


def tri_set(value):
    return {tuple(sorted(map(int, m))) for m in re.findall(r'(\d+)\.(\d+)\.(\d+)', value)}


def tri_order(value):
    return [tuple(map(int, m.split('.'))) for m in re.findall(r'\d+\.\d+\.\d+', value)]


def text(ordered):
    return ','.join('.'.join(map(str, t)) for t in ordered)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    ini_path = args.game / 'scripts/nfsmw_vr_edits.ini'
    original = ini_path.read_bytes()
    eol = '\r\n' if b'\r\n' in original else '\n'
    lines = original.decode('latin-1').splitlines(keepends=True)
    headers = {}
    for i, line in enumerate(lines):
        m = re.match(r'\s*\[([^\]]+)\]', line)
        if m:
            headers.setdefault(m.group(1), i)
    order = sorted(headers.items(), key=lambda kv: kv[1])

    def section_end(name):
        start = headers[name]
        return next((i for _, i in order if i > start), len(lines))

    def get_key(name, key):
        start, end = headers[name], section_end(name)
        for i in range(start + 1, end):
            m = re.match(r'([ \t]*)([A-Za-z]+)([ \t]*=)([^\r\n]*)(\r?\n)?$', lines[i])
            if m and m.group(2).casefold() == key.casefold():
                return i, m
        return None, None

    def set_key(name, key, value):
        i, m = get_key(name, key)
        if i is not None:
            lines[i] = m.group(1) + key + m.group(3) + value + (m.group(5) or eol)
            return
        at = section_end(name)
        if at and not lines[at - 1].endswith(('\r', '\n')):
            lines[at - 1] += eol
        lines.insert(at, key + '=' + value + eol)
        for section, header in list(headers.items()):
            if header >= at:
                headers[section] = header + 1
        order[:] = sorted(headers.items(), key=lambda kv: kv[1])

    report = {'details': [], 'applied': False}
    for car in sorted((args.game / 'CARS').iterdir()):
        path = car / 'GEOMETRY.BIN'
        if not path.is_file():
            continue
        for name, verts, entry_tris, min_xyz, max_xyz in driver_models(path):
            if name not in headers:
                break
            body, arms, head, finger = classify(verts, entry_tris, min_xyz, max_xyz)
            row = {'model': name, 'body': len(body), 'arms': len(arms),
                   'head': len(head), 'finger': len(finger)}
            if head:
                i, m = get_key(name, 'Hide')
                old = tri_order(m.group(4)) if m else []
                old_set = {tuple(sorted(t)) for t in old}
                hide = old + [t for t in head if tuple(sorted(t)) not in old_set]
                set_key(name, 'Hide', text(hide))
                row['hide_added'] = len({tuple(sorted(t)) for t in hide}) - len(old_set)
            if name not in HAND_TUNED and name not in DO_BY_HAND:
                for key, generated in (('Arms', arms), ('Skin', finger), ('Show', arms)):
                    i, m = get_key(name, key)
                    old = tri_set(m.group(4)) if m else set()
                    wanted = old | {tuple(sorted(t)) for t in generated}
                    set_key(name, key, text(sorted(wanted, key=lambda t: (t[0] << 32) | (t[1] << 16) | t[2])))
                    row[key.lower() + '_added'] = len(wanted - old)
            report['details'].append(row)
            break
    result = ''.join(lines).encode('latin-1')
    report['changed'] = result != original
    report['models'] = len(report['details'])
    if args.apply and result != original:
        running = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
                                  'if (Get-Process speed -ErrorAction SilentlyContinue) { 1 } else { 0 }'],
                                 capture_output=True, text=True, check=True).stdout.strip()
        if running != '0':
            raise RuntimeError('Close the game before writing the edits INI')
        stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        backup = ini_path.with_name(ini_path.name + '.before-driver-marks-' + stamp + '.bak')
        shutil.copy2(ini_path, backup)
        if backup.read_bytes() != original or ini_path.read_bytes() != original:
            raise RuntimeError('Backup verification failed or INI changed during classification')
        ini_path.write_bytes(result)
        if ini_path.read_bytes() != result:
            raise RuntimeError('INI verification failed')
        report['applied'] = True
        report['backup'] = str(backup)
    args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps({k: report[k] for k in ('models', 'changed', 'applied')}))


if __name__ == '__main__':
    main()
