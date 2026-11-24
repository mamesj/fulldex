from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class GbaPresentClockTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_ignored_vsync_and_working_high_refresh(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'present-clock'
            subprocess.run(['c++','-std=c++17',str(ROOT/'tests/test_gba_present_clock.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
