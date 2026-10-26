import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / f'{name}.py')
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

builder, reader = module('build_security'), module('catalog_locations')

class Coverage(unittest.TestCase):
    def setUp(self):
        self.coverage = json.loads((ROOT / 'data/security-coverage.json').read_text())
        self.economy = json.loads((ROOT / 'package/SKSE/Plugins/MarcoPolo/economy.json').read_text())

    def test_reviewed_profiles(self):
        profiles = builder.generate(self.coverage, self.economy)
        self.assertEqual(profiles, json.loads((ROOT / 'package/SKSE/Plugins/MarcoPolo/security.json').read_text()))
        self.assertEqual(len(profiles['sites']), 40)
        rows = {r['location']: r for r in self.coverage['candidates']}
        self.assertEqual(len(rows), len(self.coverage['candidates']))
        for site in profiles['sites']:
            row = rows[site['location']]
            self.assertEqual(reader.classify(row['keywords']), site['kind'])
            if site['kind'] in ('bandit', 'forsworn'):
                self.assertIn('LocTypeClearable', row['keywords'])
        self.assertEqual(rows['Skyrim.esm|01925F']['decision'], 'exclude')
        self.assertEqual(rows['Skyrim.esm|0D9079']['decision'], 'exclude')
        self.assertEqual(rows['Skyrim.esm|08E1B7']['marker_correction']['marker'], 'Skyrim.esm|08E03C')

    def test_road_budgets_and_symmetry(self):
        totals = {}
        for site in builder.generate(self.coverage, self.economy)['sites']:
            for road, exposure in site['routes'].items():
                self.assertGreater(exposure, 0)
                self.assertLessEqual(exposure, 1)
                totals[road] = totals.get(road, 0) + site['threat'] * exposure
        for road, budget in self.coverage['corridorThreatBudgets'].items():
            self.assertAlmostEqual(totals[road], budget, places=8)
            reverse = '-'.join(reversed(road.split('-')))
            self.assertAlmostEqual(totals[road], totals[reverse], places=8)
        self.assertLessEqual(max(totals.values()), 1.8 + 1e-8)

    def test_invalid_mapping_rejected(self):
        c = copy.deepcopy(self.coverage)
        row = next(r for r in c['candidates'] if r['decision'] == 'include' and r['kind'] == 'bandit')
        row['keywords'].remove('LocTypeClearable')
        with self.assertRaises(ValueError): builder.generate(c, self.economy)

    def test_reader_and_compressed_records(self):
        def field(tag, payload): return tag.encode() + struct.pack('<H', len(payload)) + payload
        def record(tag, fid, payload, flags=0):
            return tag.encode() + struct.pack('<IIIIHH', len(payload), flags, fid, 0, 44, 0) + payload
        body = field('EDID', b'VerifiedLocation\0') + field('MNAM', struct.pack('<I', 123))
        plugin = record('TES4', 0, field('HEDR', bytes(12)))
        # Hardcoded GMST IDs may have non-local high bytes; unrelated records
        # must not be mistaken for references into the master's form namespace.
        plugin += record('GMST', 0x123C00E, field('DATA', bytes(4)))
        plugin += record('LCTN', 42, struct.pack('<I', len(body)) + zlib.compress(body), 0x40000)
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'Skyrim.esm'; path.write_bytes(plugin)
            rows = reader.read_plugin(path, {'LCTN'})
            self.assertEqual(reader.string(rows['Skyrim.esm|00002A']['fields']['EDID']), 'VerifiedLocation')
            path.write_bytes(plugin[:-1])
            with self.assertRaises(ValueError): reader.read_plugin(path, {'LCTN'})

if __name__ == '__main__': unittest.main()
