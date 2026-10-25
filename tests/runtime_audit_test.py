"""Exercise desktop Address Library decoding without distributing game data."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('runtime_audit', ROOT/'tools/audit-runtime.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AddressFormats(unittest.TestCase):
    def decode(self, blob):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'fixture.bin'
            path.write_bytes(blob)
            return audit.addresses(path)

    def test_se_and_ae_compressed_formats(self):
        for fmt, version in [(1, (1, 5, 97, 0)), (2, (1, 6, 353, 0)), (2, (1, 6, 1170, 0))]:
            header = struct.pack('<6I', fmt, *version, 0) + struct.pack('<2I', 8, 2)
            # Absolute entry, then delta-encoded ID and scaled pointer offset.
            entries = struct.pack('<BQQ', 0, 10, 0x1000) + struct.pack('<B', 0x91)
            actual_version, rows = self.decode(header + entries)
            self.assertEqual(actual_version, version)
            self.assertEqual(rows, {10: 0x1000, 11: 0x1008})

    def test_ae_17_dense_format(self):
        for version in [(1, 7, 99, 0), (1, 7, 104, 0)]:
            header = struct.pack('<5I', 5, *version) + bytes(64) + struct.pack('<3I', 8, 1, 4)
            blob = header + struct.pack('<4I', 0, 0x1000, 0, 0x2000)
            self.assertEqual(self.decode(blob), (version, {1: 0x1000, 3: 0x2000}))
            with self.assertRaises(ValueError):
                self.decode(blob[:-4])

    def test_family_specific_reset_id(self):
        hooks = audit.targets()
        se = {ident: 0x1000 for ident in [hooks['ResetInventorySE'], 235511, 267991, 519283]}
        ae = {ident: 0x2000 for ident in [hooks['ResetInventoryAE'], 190259, 214926, 405823]}
        audit.check_table(se, False, hooks)
        audit.check_table(ae, True, hooks)
        with self.assertRaises(ValueError):
            audit.check_table(ae, False, hooks)
        with self.assertRaises(ValueError):
            audit.check_table(se, True, hooks)

    def test_popup_id_renumbering(self):
        for version, ident in [((1,5,97,0),51421), ((1,6,640,0),52270),
                               ((1,6,1130,0),442726), ((1,7,104,0),442726)]:
            self.assertEqual(audit.check_information({ident:0x1000},version)[0]['id'],ident)
            with self.assertRaises(ValueError):audit.check_information({},version)


if __name__ == '__main__':
    unittest.main()
