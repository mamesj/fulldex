from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ZoomStepsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_camera_steps_and_automatic_close(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'zoom-steps'
            subprocess.run(['c++','-std=c++17','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_zoom_steps.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()
