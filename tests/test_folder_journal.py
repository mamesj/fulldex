from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class FolderJournalTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ required')
    def test_journal_roundtrip_and_failed_close(self):
        include=Path(os.environ.get('SSR_FOLDER_JOURNAL_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'test';path=root/'journal'
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_folder_journal.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(path),'success'],check=True)
            if os.name=='posix':
                import resource,signal
                path.write_text('existing-good-journal\n')
                def limit_child_only():
                    resource.setrlimit(resource.RLIMIT_FSIZE,(1,1))
                    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
                    signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
                result=subprocess.run([str(exe),str(path),'failure'],preexec_fn=limit_child_only,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(path.read_text(),'existing-good-journal\n')
