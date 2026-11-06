import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('merge', Path(__file__).resolve().parents[1]/'tools/merge_driver_fingers.py')
merge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merge)


class MergeTests(unittest.TestCase):
    def test_topology_guard(self):
        canonical = {(i, i+1, i+2) for i in range(100)}
        self.assertTrue(merge.compatible_fingers(canonical, canonical))
        self.assertFalse(merge.compatible_fingers({(a+1000,b+1000,c+1000) for a,b,c in canonical}, canonical))
        self.assertFalse(merge.compatible_fingers(set(), canonical))

    def test_marks_opt_out_and_winding(self):
        original = (b'; keep\r\n[CAR_DRIVER_A]\r\nNative=3.4.5\r\n'
                    b'Hide=2.0.1 9.7.8\r\nBlack=0.1.2 7.8.9\r\n'
                    b'Wheel=7.8.9\r\n[OTHER]\r\nSkin=8.9.10\r\n')
        result, report = merge.merge_marks(original, {'CAR_DRIVER_A': {(0,1,2),(3,4,5)}})
        self.assertIn(b'Native=3.4.5\r\n', result)
        self.assertIn(b'Hide= 9.7.8\r\n', result)
        self.assertIn(b'Black=7.8.9\r\n', result)
        for key in (b'Arms', b'Skin', b'Show'):
            self.assertIn(key+b'=0.1.2\r\n', result)
        self.assertIn(b'Wheel=7.8.9\r\n', result)
        self.assertTrue(result.endswith(b'[OTHER]\r\nSkin=8.9.10\r\n'))
        self.assertEqual(report[0]['native_opt_outs'], 1)
        self.assertEqual(merge.merge_marks(result, {'CAR_DRIVER_A': {(0,1,2),(3,4,5)}})[0], result)

    def test_missing_section_and_no_final_newline(self):
        result, _ = merge.merge_marks(b'[OTHER]\nWheel=1.2.3', {'NEW': {(4,5,6)}})
        self.assertTrue(result.startswith(b'[OTHER]\nWheel=1.2.3\n[NEW]\n'))
        self.assertIn(b'Show=4.5.6\n', result)

    def test_duplicates_rejected(self):
        for original in (b'[CAR]\n[CAR]\n', b'[CAR]\nArms=\narms=\n'):
            with self.assertRaises(ValueError):
                merge.merge_marks(original, {'CAR': {(0,1,2)}})


if __name__ == '__main__':
    unittest.main()