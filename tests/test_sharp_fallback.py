from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SharpFallbackTests(unittest.TestCase):
    def check(self,mode,sanitize=False):
        if not(shutil.which('c++') and shutil.which('sdl2-config')):self.skipTest('C++ and SDL required')
        source=Path(os.environ.get('FULLDEX_SHARP_SOURCE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-sharp-fallback-') as folder:
            exe=Path(folder)/'sharp'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            sanitizers=['-fsanitize=float-cast-overflow','-fno-sanitize-recover=float-cast-overflow'] if sanitize else []
            subprocess.run(['c++','-O1','-std=c++17',*sanitizers,'-I'+str(source),str(ROOT/'tests/test_sharp_fallback.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe),mode],check=True,timeout=20)
    def test_failed_intermediate_draw_uses_source(self):self.check('copy')
    def test_failed_size_query_avoids_invalid_arithmetic(self):self.check('query',True)
