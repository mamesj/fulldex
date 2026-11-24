from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class PortraitOrientationTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_ios_window_creation_preserves_chosen_orientation(self):
        main = (ROOT / 'frontend/main.cpp').read_text()
        start = main.index('IosOrientGame(', main.index('if (window) SDL_RaiseWindow(window);'))
        after_window = main[start:main.index(';', start)+1]
        policy = method((ROOT / 'frontend/ios_orient.mm').read_text(), 'void IosOrientGame(')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'SDL.h').write_text('#pragma once\n#define SDL_HINT_ORIENTATIONS "orientation"\ninline void SDL_SetHint(const char*,const char*){}\n')
            (root / 'ios.cpp').write_text('''#include "mobile_orientation.h"
#include <cassert>
enum class LayoutMode { Vertical, Merged };
struct { LayoutMode mode; } layout;
static bool landscape=true,free_rotation=true;
static void Set(bool sideways,bool free){landscape=sideways;free_rotation=free;}
''' + policy + '''
int main(){
 for(auto mode:{LayoutMode::Vertical,LayoutMode::Merged}){
  layout.mode=mode;MobileOrientation::Game(mode==LayoutMode::Vertical);
''' + after_window + '''
  assert(landscape==(mode==LayoutMode::Merged)&&!free_rotation);
 }
}
''')
            exe=root/'ios'
            subprocess.run(['c++','-std=c++17','-DSSR_IOS','-include','initializer_list','-I'+str(root),'-I'+str(ROOT/'frontend'),str(root/'ios.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    @unittest.skipUnless(shutil.which('javac') and shutil.which('java'), 'Java compiler required')
    def test_actual_android_orientation_policy(self):
        source = (ROOT / 'tools/android/allgames/java/io/github/mamesj/pokemonds/GameActivity.java').read_text()
        methods = '\n'.join(method(source, signature) for signature in [
            'private void applyScreenOrientation()', 'public static void showLauncherOrientation()',
            'public static void showGameOrientation()', 'public static void showPortraitGameOrientation()',
            'public static void showPageOrientation()'])
        harness = '''
class ActivityInfo { static final int SCREEN_ORIENTATION_SENSOR_LANDSCAPE=6, SCREEN_ORIENTATION_USER_PORTRAIT=12, SCREEN_ORIENTATION_FULL_USER=13; }
class Settings { static class System { static int auto=1; static final String ACCELEROMETER_ROTATION="rotate"; static int getInt(Object resolver,String key,int fallback){return auto;} } }
public class PortraitOrientation {
 static boolean playingGame,portraitGame,landscapeOnly;
 static PortraitOrientation self=new PortraitOrientation();
 int requested=-1;
 void applyCutoutMode(){} void runOnUiThread(Runnable action){action.run();}
 Object getContentResolver(){return null;} void setRequestedOrientation(int value){requested=value;}
 static void setDsRunning(boolean value){}
''' + methods + '''
 public static void main(String[] args) {
  showLauncherOrientation();assert self.requested==13;
  showGameOrientation();assert playingGame && !portraitGame && self.requested==6;
  showPortraitGameOrientation();assert playingGame && portraitGame && self.requested==12;
  Settings.System.auto=0;self.applyScreenOrientation();assert self.requested==12;
  showLauncherOrientation();assert !playingGame && !portraitGame && self.requested==6;
  Settings.System.auto=1;showLauncherOrientation();assert self.requested==13;
  showPageOrientation();assert !playingGame && landscapeOnly && self.requested==6;
  showPortraitGameOrientation();assert self.requested==12;
  showGameOrientation();assert !portraitGame && self.requested==6;
  java.lang.System.out.println("Android portrait game, landscape game, launcher and page orientation policies passed.");
 }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'PortraitOrientation.java').write_text(harness)
            subprocess.run(['javac', str(root / 'PortraitOrientation.java')], check=True)
            subprocess.run(['java', '-ea', '-cp', str(root), 'PortraitOrientation'], check=True)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_actual_native_android_bridge(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'jni.h').write_text('''#pragma once
#include <string>
using jobject=void*; using jclass=void*; using jmethodID=void*;
struct JNIEnv {
 std::string selected; bool missing=false,exception=false; int calls=0,deleted=0,cleared=0;
 jclass GetObjectClass(jobject object){return object;}
 jmethodID GetStaticMethodID(jclass,const char* name,const char*){selected=name;if(missing){exception=true;return nullptr;}return this;}
 void CallStaticVoidMethod(jclass,jmethodID){calls++;}
 bool ExceptionCheck(){return exception;}void ExceptionClear(){exception=false;cleared++;}
 void DeleteLocalRef(jobject){deleted++;}
};
''')
            (root / 'SDL.h').write_text('''#pragma once
#include "jni.h"
#define SDL_HINT_ORIENTATIONS "orientations"
inline JNIEnv bridge;inline std::string hint;inline bool attached=true;
inline void SDL_SetHint(const char*,const char* value){hint=value;}
inline void* SDL_AndroidGetJNIEnv(){return attached?&bridge:nullptr;}
inline void* SDL_AndroidGetActivity(){return attached?&bridge:nullptr;}
''')
            (root / 'bridge.cpp').write_text('''#include "mobile_orientation.h"
#include <cassert>
int main(){
 MobileOrientation::Game(true);assert(hint=="Portrait PortraitUpsideDown");assert(bridge.selected=="showPortraitGameOrientation"&&bridge.calls==1&&bridge.deleted==2);
 MobileOrientation::Game(false);assert(hint=="LandscapeLeft LandscapeRight");assert(bridge.selected=="showGameOrientation"&&bridge.calls==2);
 bridge.missing=true;MobileOrientation::Game(true);assert(bridge.calls==2&&!bridge.exception&&bridge.cleared==1);
 attached=false;MobileOrientation::Game(false);assert(bridge.calls==2);
}
''')
            exe = root / 'bridge'
            subprocess.run(['c++', '-std=c++17', '-D__ANDROID__', '-I' + str(root), '-I' + str(ROOT / 'frontend'), str(root / 'bridge.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
