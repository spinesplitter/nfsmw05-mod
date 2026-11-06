"""Read-only inventory of every installed car and every driver LOD/material."""
import argparse
import json
from pathlib import Path
import struct
import sys

from merge_driver_fingers import compatible_fingers, finger_meshes


def inventory(path, chunks):
    data = path.read_bytes()
    models = []
    for tag, start, end in chunks(data):
        if tag != 0x80134000:
            continue
        for tag, start, end in chunks(data, start, end):
            if tag != 0x80134010:
                continue
            parts = list(chunks(data, start, end))
            header = next((data[a:b] for t, a, b in parts if t == 0x134011), b'')
            name = header[160:].split(b'\0')[0].decode('ascii', errors='replace')
            if '_DRIVER_' not in name:
                continue
            mesh = next((list(chunks(data, a, b)) for t, a, b in parts
                         if t == 0x80134100), [])
            materials = [data[a:b].split(b'\0')[0].decode('ascii', errors='replace')
                         for t, a, b in mesh if t == 0x134C02]
            raw = next((data[a:b] for t, a, b in mesh if t == 0x134B02), b'')
            while raw.startswith(b'\x11'*4):
                raw = raw[4:]
            if len(raw) % 0x68:
                raise ValueError(name+': unexpected draw-entry record size')
            entries = []
            for i in range(len(raw)//0x68):
                count, first = struct.unpack_from('<II', raw, i*0x68+0x40)
                entries.append({'entry': i, 'material': materials[i] if i < len(materials) else None,
                                'triangles': count, 'first_index': first})
            models.append({'model': name, 'entries': entries,
                           'second_driver_player': len(entries) > 1 and
                           entries[1]['material'] == 'DRIVER_PLAYER'})
    return models


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--reader-dir', required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.reader_dir)
    from inspect_cockpit_gaps import chunks
    canonical = list(finger_meshes(args.game/'CARS/GALLARDO/GEOMETRY.BIN', chunks))[0][1]
    cars = []
    for folder in sorted((args.game/'CARS').iterdir()):
        if not folder.is_dir():
            continue
        row = {'car': folder.name, 'rig': (args.game/'DRIVERMOD/rigs'/(folder.name+'.bin')).is_file(),
               'models': []}
        path = folder/'GEOMETRY.BIN'
        try:
            if not path.is_file():
                row['status'] = 'no_geometry'
            else:
                row['models'] = inventory(path, chunks)
                row['status'] = 'checked' if row['models'] else 'no_driver_models'
                for model in row['models']:
                    model['reference_fingers'] = False
                try:
                    verified = {}
                    for name, triangles in finger_meshes(path, chunks):
                        if name in verified and verified[name] != triangles:
                            raise ValueError(name+': duplicate models have different finger triangles')
                        verified[name] = triangles
                    for model in row['models']:
                        if model['model'] in verified:
                            model['reference_fingers'] = compatible_fingers(verified[model['model']], canonical)
                except (ValueError, struct.error) as error:
                    row['finger_verification_note'] = str(error)
        except (ValueError, OSError, struct.error) as error:
            row['status'] = 'error'
            row['error'] = str(error)
        cars.append(row)
    models = [model for car in cars for model in car['models']]
    summary = {'car_folders': len(cars), 'geometry_files': sum(c['status'] != 'no_geometry' for c in cars),
               'cars_with_drivers': sum(bool(c['models']) for c in cars), 'driver_models': len(models),
               'second_driver_player_models': sum(m['second_driver_player'] for m in models),
               'reference_finger_models': sum(m['reference_fingers'] for m in models),
               'errors': sum(c['status'] == 'error' for c in cars)}
    args.report.write_text(json.dumps({'summary': summary, 'cars': cars}, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()