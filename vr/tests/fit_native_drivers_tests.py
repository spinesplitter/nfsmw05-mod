import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('fitter',Path(__file__).resolve().parents[1]/'tools/fit_native_drivers.py')
fitter=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitter)


class FitTests(unittest.TestCase):
    def test_mirror_rig(self):
        data=bytearray(40+6*124+44)
        struct.pack_into('<4sIII6I',data,0,b'IKR1',1,6,1,*range(6))
        for i in range(6):
            offset=40+i*124
            struct.pack_into('<3f3f',data,offset+36,1,2,3,4,5,6)
            struct.pack_into('<16f',data,offset+60,1,0,0,1,0,1,0,2,0,0,1,3,0,0,0,1)
        struct.pack_into('<3f',data,40+6*124+32,7,8,9)
        result=fitter.mirror_rig(data)
        self.assertEqual(struct.unpack_from('<6I',result,16),(3,4,5,0,1,2))
        self.assertEqual(struct.unpack_from('<3f',result,76),(1,-2,3))
        self.assertEqual(struct.unpack_from('<3f',result,40+6*124+32),(7,-8,9))
        self.assertEqual(fitter.mirror_rig(result),data)
        with self.assertRaises(ValueError):
            fitter.mirror_rig(data[:-1])

    def test_hidden_only(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'edits.ini'
            original=b'; Hide=comment\r\n[CAR]\r\nHide=1.2.3,4.5.6\r\nBlack=4.5.6\r\nArms=7.8.9\r\nSkin=1.2.3\r\n[OTHER]\r\nhide=5.6.7'
            path.write_bytes(original)
            backup,count=fitter.reset_hidden(path)
            self.assertEqual(Path(backup).read_bytes(),original)
            self.assertEqual(count,2)
            self.assertEqual(path.read_bytes(),original.replace(b'Hide=1.2.3,4.5.6',b'Hide=').replace(b'hide=5.6.7',b'hide='))

    def test_translation_fit_and_row_major_matrix(self):
        data=bytearray(40+124+44)
        struct.pack_into('<II',data,8,1,1)
        struct.pack_into('<3f',data,40+36,0,0,0)
        struct.pack_into('<3f',data,40+48,1,0,0)
        struct.pack_into('<3f',data,40+124+32,1,0,0)
        result=fitter.fit(data,[(0,0,0),(1,0,0)],[(2,3,4),(3,3,4)])
        self.assertEqual(struct.unpack_from('<3f',result,40+36),(2,3,4))
        self.assertEqual(struct.unpack_from('<3f',result,40+48),(3,3,4))
        self.assertEqual([struct.unpack_from('<f',result,40+60+(a*4+3)*4)[0] for a in range(3)],[2,3,4])
        self.assertEqual(struct.unpack_from('<3f',result,40+124+32),(3,3,4))


if __name__=='__main__':
    unittest.main()