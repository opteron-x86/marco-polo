"""Ensure the packaged menu matches its source and relies on game fonts."""
import importlib.util
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ledger_build', ROOT/'tools/build-ledger.py')
ledger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ledger)

class LedgerMovie(unittest.TestCase):
    def test_shipped_movie_and_source(self):
        ledger.verify()

    def test_native_loader_points_to_packaged_movie(self):
        source = (ROOT/'src/runtime/Ledger.cpp').read_text()
        name = re.search(r'constexpr char MoviePath\[\] = "([^"]+)";', source).group(1)
        path = re.search(r'constexpr char MovieFile\[\] = "([^"]+)";', source).group(1)
        self.assertNotIn('/', name)
        self.assertNotIn('\\', name)
        self.assertEqual(path, f'Interface/{name}.swf')
        self.assertEqual(ROOT/'package'/path, ledger.OUTPUT)
        self.assertTrue(ledger.OUTPUT.is_file())

    def test_invalid_movie(self):
        data = ledger.OUTPUT.read_bytes()
        with self.assertRaises(ValueError):
            ledger.validate(data[:-1])
        with self.assertRaises(ValueError):
            ledger.validate(data.replace(b'$EverywhereFont', b'$MissingFont___'))

if __name__ == '__main__':
    unittest.main()
