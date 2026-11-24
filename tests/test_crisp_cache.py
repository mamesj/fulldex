from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CrispCacheTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL required')
    def test_changed_masks_match_fresh_rendering(self):
        source=Path(os.environ.get('FULLDEX_CRISP_SOURCE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-crisp-cache-') as folder:
            exe=Path(folder)/'crisp'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-O2','-std=c++17','-I'+str(source),str(ROOT/'tests/test_crisp_cache.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True,timeout=30)
