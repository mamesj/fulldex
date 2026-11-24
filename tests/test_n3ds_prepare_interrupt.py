from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PrepareInterruptTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++'), 'Linux compiler required')
    def test_signal_does_not_lose_preparation_result(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-prepare-') as folder:
            binary = Path(folder) / 'prepare-interrupt'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_n3ds_prepare_interrupt.cpp'), '-o', str(binary)], check=True)
            env = dict(os.environ, HOME=folder, XDG_DATA_HOME=folder, XDG_CONFIG_HOME=folder)
            subprocess.run([str(binary), folder], check=True, timeout=10, env=env)
