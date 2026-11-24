from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class WideUploadReuseTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL required')
    def test_cached_world_matches_uncached_pixels(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-world-upload-') as folder:
            exe=Path(folder)/'world'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-O2','-std=c++17','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_wide_upload_reuse.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True,timeout=30)
