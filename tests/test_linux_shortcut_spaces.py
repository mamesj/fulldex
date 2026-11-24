from pathlib import Path
import ctypes
import os, shutil, subprocess, sys, tempfile, time, unittest
ROOT=Path(__file__).resolve().parents[1]
class ShortcutSpaceTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform=='linux' and shutil.which('c++') and shutil.which('gio'),'Linux compiler and GIO required')
    def test_launch_preserves_working_directory_and_display_name_spaces(self):
        include=Path(os.environ.get('SSR_SPACE_SHORTCUT_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-shortcut-spaces-') as folder:
            home=Path(folder)/'Data with a trailing space ';home.mkdir()
            binary=home/'launcher';result=home/'result'
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_linux_shortcut_spaces.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(binary)],check=True)
            env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'data'),APPIMAGE=str(binary),FULLDEX_SPACE_RESULT=str(result),LC_ALL='C')
            subprocess.run([str(binary)],env=env,check=True)
            entries=list((home/'data/applications').glob('*.desktop'));self.assertEqual(len(entries),1)
            subprocess.run(['gio','launch',str(entries[0])],env=env,check=True,timeout=10)
            for _ in range(100):
                if result.exists():break
                time.sleep(.02)
            self.assertEqual(result.read_text().splitlines(),[str(home),'--rom',str(home/'synthetic.nds')])
            gio=ctypes.CDLL('libgio-2.0.so.0')
            gio.g_desktop_app_info_new_from_filename.argtypes=[ctypes.c_char_p];gio.g_desktop_app_info_new_from_filename.restype=ctypes.c_void_p
            gio.g_app_info_get_display_name.argtypes=[ctypes.c_void_p];gio.g_app_info_get_display_name.restype=ctypes.c_char_p
            gio.g_object_unref.argtypes=[ctypes.c_void_p];gio.g_object_unref.restype=None
            info=gio.g_desktop_app_info_new_from_filename(os.fsencode(entries[0]));self.assertTrue(info)
            try:self.assertEqual(gio.g_app_info_get_display_name(info).decode(),'  Custom Name  ')
            finally:gio.g_object_unref(info)
