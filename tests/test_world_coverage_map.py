from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class WorldCoverageMapTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL required')
    def test_mapped_pixels_match_original_sampling(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-world-map-') as folder:
            temp=Path(folder)
            # Restore the original per-pixel sampler as an independent pixel oracle.
            text=(ROOT/'frontend/gpucompose.h').read_text(encoding='utf-8')
            start=text.index('                bool worldMapReady = false;')
            end=text.index('                for (int y = 0; y < el.sh; y++)',start)
            text=text[:start]+text[end:]
            assert text.count('worldCovers(x, y)')==1
            text=text.replace('worldCovers(x, y)','MergedWideCovers(*wide, v, outW, outH, el.sx + x, sy)')
            text=text.replace('GpuCompositor','ReferenceGpuCompositor')
            (temp/'gpucompose_reference.h').write_text(text,encoding='utf-8')
            exe=temp/'world-map'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-O2','-std=c++17','-I'+str(ROOT/'frontend'),'-I'+folder,str(ROOT/'tests/test_world_coverage_map.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True,timeout=30)
