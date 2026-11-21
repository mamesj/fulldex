"""Memory Link (frontend/memory_link.h): Black and White saves read and linked into Black 2 and
White 2's key data, on synthetic saves (tests/test_memory_link.cpp)."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MemoryLinkTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_link_black_white_into_black2_white2(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'memory-link'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_memory_link.cpp'), '-o', str(exe)], check=True)
            out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
            self.assertIn('PASS', out)


if __name__ == '__main__':
    unittest.main()
