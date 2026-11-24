from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class StateRecoveryTypeTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_pending_recovery_preserves_directories(self):
        source=Path(os.environ.get('SSR_RECOVERY_TYPES_ROOT',ROOT))
        with tempfile.TemporaryDirectory(prefix='fulldex-state-recovery-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-I'+str(source/'frontend'),'-I'+str(source/'gba/frontend'),str(ROOT/'tests/test_state_recovery_types.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=10)
