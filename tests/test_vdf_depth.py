from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class VdfDepthTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_corrupt_nesting_cannot_exhaust_stack(self):
        include=Path(os.environ.get('SSR_VDF_DEPTH_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-vdf-depth-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_vdf_depth.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=10)
