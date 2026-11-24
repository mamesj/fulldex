from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class NarrowBarWindowTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL headers required')
    def test_exact_blur_output(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-bars-') as folder:
            exe=Path(folder)/'bars'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-O2','-std=c++17','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_narrow_bar_window.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True)
