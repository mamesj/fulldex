#include "state_slots.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
static void Zone(const char* zone){
#ifdef _WIN32
    _putenv_s("TZ",zone);_tzset();
#else
    setenv("TZ",zone,1);tzset();
#endif
}
#ifdef FULLDEX_SWITCH_CALENDAR_TEST
static bool serviceFailed=false;
namespace SwitchPlatform {bool LocalCalendar(int64_t value,std::tm& result){if(serviceFailed)return false;Zone("EST5EDT");auto time=static_cast<std::time_t>(value);auto local=std::localtime(&time);if(!local)return false;result=*local;Zone("UTC0");return true;}}
#endif
static std::time_t Local(int month,int day,int hour,int minute){Zone("EST5EDT");std::tm t{};t.tm_year=126;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_sec=15;t.tm_isdst=-1;return std::mktime(&t);}
int main(int argc,char**argv){assert(argc==2);
#ifdef _WIN32
    _putenv_s("TZ","EST5EDT");_tzset();
#else
    setenv("TZ","EST5EDT",1);tzset();
#endif
    auto before=Local(11,1,0,15),after=Local(11,2,12,0);
    auto first=*std::localtime(&before),second=*std::localtime(&after);assert(first.tm_isdst==1&&second.tm_isdst==0);
    namespace fs=std::filesystem;auto state=fs::path(argv[1])/"synthetic.state";std::ofstream(state)<<"synthetic state bytes";
    struct Case{int sm,sd,sh,smin,nm,nd;const char* expected;};
    for(const auto c:{Case{11,1,0,15,11,1,"Today, 00:15"},Case{11,1,0,15,11,2,"Yesterday, 00:15"},Case{3,7,23,30,3,8,"Yesterday, 23:30"},Case{3,7,23,30,3,9,"7 Mar, 23:30"},Case{11,22,18,30,11,24,"22 Nov, 18:30"},Case{11,25,18,30,11,24,"25 Nov, 18:30"}}){
        auto saved=std::chrono::system_clock::from_time_t(Local(c.sm,c.sd,c.sh,c.smin));
        auto stamp=std::chrono::time_point_cast<fs::file_time_type::duration>(saved-std::chrono::system_clock::now()+fs::file_time_type::clock::now());
        fs::last_write_time(state,stamp);
        const auto now=Local(c.nm,c.nd,12,0);
#ifdef FULLDEX_SWITCH_CALENDAR_TEST
        Zone("UTC0");
#endif
        auto label=StateSlots::FriendlyAt(state.u8string(),now);
        if(label!=c.expected){std::cerr<<"Calendar label mismatch: expected "<<c.expected<<", got "<<label<<'\n';return 1;}
    }
    assert(StateSlots::FriendlyAt((state.string()+".missing"),after).empty());
#ifdef FULLDEX_SWITCH_CALENDAR_TEST
    serviceFailed=true;Zone("UTC0");std::tm fallback{};assert(StateCalendar::Local(42,fallback));assert(fallback.tm_year==70&&fallback.tm_mon==0&&fallback.tm_mday==1);
#endif
    std::cout<<"State dates respect spring/fall clock changes and normal calendar days.\n";
}
