from pathlib import Path
import os, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class DesktopEscapesTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform=='linux' and shutil.which('c++'),'Linux compiler required')
    def test_escaped_paths_create_and_remove_in_the_right_directory(self):
        include=Path(os.environ.get('SSR_ESCAPES_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-desktop-escapes-') as folder:
            root=Path(folder);binary=root/'test'
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_linux_desktop_escapes.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],env=dict(os.environ,HOME=folder,XDG_CONFIG_HOME=str(root/'.config'),XDG_DATA_HOME=str(root/'data'),APPIMAGE=str(binary)),check=True,timeout=20)
