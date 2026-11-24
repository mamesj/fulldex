from pathlib import Path
import os, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class RuntimeKitOverlapTests(unittest.TestCase):
    @unittest.skipUnless((sys.platform.startswith('linux') or os.name=='nt') and shutil.which('c++'),'Windows/Linux compiler required')
    def test_overlapping_copy_is_refused_before_writing(self):
        include=Path(os.environ.get('SSR_KIT_OVERLAP_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-kit-overlap-') as folder:
            root=Path(folder);binary=root/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-pthread','-I'+str(include),str(ROOT/'tests/test_runtime_kit_overlap.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=15)
