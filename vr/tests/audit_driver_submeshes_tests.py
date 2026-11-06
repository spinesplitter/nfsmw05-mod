from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from audit_driver_submeshes import inventory


def chunk(tag, data):
    return struct.pack('<II', tag, len(data))+data


def chunks(data, start=0, end=None):
    end = len(data) if end is None else end
    while start+8 <= end:
        tag, size = struct.unpack_from('<II', data, start)
        stop = start+8+size
        if stop > end:
            raise ValueError('invalid chunk size')
        yield tag, start+8, stop
        start = stop


class InventoryTests(unittest.TestCase):
    def test_all_lods_and_materials(self):
        entries = bytearray(2*0x68)
        struct.pack_into('<II', entries, 0x40, 1000, 0)
        struct.pack_into('<II', entries, 0x68+0x40, 168, 3000)
        mesh = chunk(0x134B02, b'\x11'*8+entries)
        mesh += chunk(0x134C02, b'DRIVER_PLAYER\0')*2
        def model(name):
            return chunk(0x80134010, chunk(0x134011, bytes(160)+name.encode()+b'\0')+
                         chunk(0x80134100, mesh))
        data = chunk(0x80134000, model('CAR_DRIVER_A')+model('CAR_DRIVER_B')+model('CAR_BODY_A'))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'GEOMETRY.BIN'
            path.write_bytes(data)
            models = inventory(path, chunks)
        self.assertEqual([m['model'] for m in models], ['CAR_DRIVER_A', 'CAR_DRIVER_B'])
        self.assertTrue(all(m['second_driver_player'] for m in models))
        self.assertEqual(models[0]['entries'][1]['triangles'], 168)

    def test_bad_chunk_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'bad.bin'
            path.write_bytes(struct.pack('<II', 0x80134000, 999))
            with self.assertRaises(ValueError):
                inventory(path, chunks)


if __name__ == '__main__':
    unittest.main()