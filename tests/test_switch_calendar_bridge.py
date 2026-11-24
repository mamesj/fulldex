from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SwitchCalendarBridgeTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_actual_bridge_maps_console_calendar_fields(self):
        text=(ROOT/'frontend/switch_platform.cpp').read_text()
        start=text.index('bool LocalCalendar(');end=text.index('\nstatic Thread g_emuThread;',start)
        functions=text[start:end]
        prefix=r'''
#include <ctime>
#include <cstdint>
#include <cassert>
#include <iostream>
using u64=uint64_t;
struct TimeCalendarTime{uint16_t year;uint8_t month,day,hour,minute,second,pad;};
struct TimeCalendarAdditionalInfo{uint32_t wday,yday;char timezoneName[8];uint32_t DST;int32_t offset;};
static int failure=0,calls=0;static u64 seen=0;
#define R_FAILED(result) ((result)!=0)
constexpr int TimeType_UserSystemClock=0;
int timeGetCurrentTime(int,u64* now){*now=42;return failure;}
int timeToCalendarTimeWithMyRule(u64 value,TimeCalendarTime* cal,TimeCalendarAdditionalInfo* info){++calls;seen=value;if(failure)return failure;*cal={2026,11,1,0,15,15,0};*info={0,304,{},1,-14400};return 0;}
namespace SwitchPlatform {
'''
        suffix=r'''
}
int main(){std::tm cal{};assert(SwitchPlatform::LocalCalendar(42,cal));assert(seen==42);assert(cal.tm_year==126&&cal.tm_mon==10&&cal.tm_mday==1&&cal.tm_hour==0&&cal.tm_min==15&&cal.tm_sec==15&&cal.tm_yday==304&&cal.tm_wday==0&&cal.tm_isdst==1);int prior=calls;assert(!SwitchPlatform::LocalCalendar(-1,cal)&&calls==prior);int y,m,d,h,min,s;assert(SwitchPlatform::LocalTime(y,m,d,h,min,s));assert(y==2026&&m==11&&d==1&&h==0&&min==15&&s==15);failure=1;assert(!SwitchPlatform::LocalCalendar(42,cal));assert(!SwitchPlatform::LocalTime(y,m,d,h,min,s));std::cout<<"Actual Switch bridge maps service fields, current time and failures; SDK declarations simulated, not a console build.\n";}
'''
        with tempfile.TemporaryDirectory(prefix='fulldex-switch-clock-') as folder:
            cpp=Path(folder)/'bridge.cpp';cpp.write_text(prefix+functions+suffix);binary=Path(folder)/'bridge'
            subprocess.run(['c++','-std=c++17',str(cpp),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
