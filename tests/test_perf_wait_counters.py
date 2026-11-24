from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PerfWaitCounterTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_summary_leaves_csv_counters_monotonic(self):
        source=Path(os.environ.get('FULLDEX_PERF_MAIN',ROOT/'frontend/main.cpp')).read_text(encoding='utf-8')
        start=source.index('                SsrThreadStats& ts = SsrStats();',source.index('if (++perfN == 300)'))
        end=source.index('                char extra[200];',start)
        summary=source[start:end]
        initial=next((line.strip() for line in source.splitlines() if line.startswith("    uint64_t perfWait3D =")),"uint64_t perfWait3D=0,perfWaitWide=0;")
        # Compile the actual frontend summary statements and interleave a CSV reader.
        code="""#include <atomic>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <iostream>
struct SsrThreadStats {std::atomic<uint64_t>Wait3DNs{0},WaitWideNs{0};};
SsrThreadStats stats;SsrThreadStats&SsrStats(){return stats;}
int RunSession(){"""+initial+"""uint64_t previous3=stats.Wait3DNs.load(),previousWide=stats.WaitWideNs.load();int perfN=300;
for(int frame=1;frame<=1200;frame++){stats.Wait3DNs+=1000000;stats.WaitWideNs+=2000000;auto w3=stats.Wait3DNs.load(),ww=stats.WaitWideNs.load();
double csv3=(w3-previous3)/1e6,csvWide=(ww-previousWide)/1e6;if(csv3!=1||csvWide!=2){std::cerr<<"CSV wait counter jumped at frame "<<frame<<" to "<<csv3<<", "<<csvWide<<" ms\\n";return 1;}previous3=w3;previousWide=ww;
if(frame%300==0){"""+summary+"""assert(std::abs(w3d-1)<1e-9&&std::abs(wwide-2)<1e-9);}}
return 0;}
int main(){if(RunSession()||RunSession())return 1;std::cout<<"Actual summary preserves CSV/adaptive-detail counters through two sessions with four 300-frame boundaries each.\\n";return 0;}
"""
        with tempfile.TemporaryDirectory(prefix='fulldex-perf-count-') as folder:
            cpp=Path(folder)/'counter.cpp';cpp.write_text(code,encoding='utf-8');exe=Path(folder)/'counter'
            subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
