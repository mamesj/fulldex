from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class StateDateTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_calendar_labels_across_daylight_saving(self):
        include=Path(os.environ.get('SSR_STATE_DATE_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-state-dates-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_state_date_dst.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=10)
            if os.name!='nt':
                subprocess.run(['c++','-std=c++17','-D__SWITCH__','-DFULLDEX_SWITCH_CALENDAR_TEST','-I'+str(include),str(ROOT/'tests/test_state_date_dst.cpp'),'-o',str(binary)],check=True)
                subprocess.run([str(binary),folder],check=True,timeout=10)
