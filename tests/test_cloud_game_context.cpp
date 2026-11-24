#include "cloud_autosync.h"
#include "cloud_saves.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
static std::atomic<bool> hold{true},started{false},finished{false};
static std::mutex logMutex;static std::vector<std::string> jobs;
namespace CloudSaves {
struct Sync::Impl {};
Sync::Sync(std::unique_ptr<Backend>,const std::string&,const std::string&):d(new Impl){}
Sync::~Sync(){}
Result Sync::BeforePlay(const std::string&){Result r;r.outcome=Outcome::InSync;return r;}
Result Sync::AfterPlay(const std::string& path){
    {std::lock_guard<std::mutex> lock(logMutex);jobs.push_back(path);}started=true;
    while(hold)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    finished=true;Result r;r.outcome=Outcome::InSync;return r;
}
Settings LoadSettings(const std::string&){Settings s;s.provider="folder";return s;}
std::unique_ptr<Sync> FromSettings(const Settings&,const std::string& p,const std::string& d){return std::make_unique<Sync>(nullptr,p,d);}
}
int main(int argc,char**argv){assert(argc==2);const std::string root=argv[1];
    assert(CloudAuto::Start(root,root+"/first.sav").empty());CloudAuto::Saved();
    std::this_thread::sleep_for(std::chrono::milliseconds(8100));CloudAuto::Tick();
    for(int i=0;i<500&&!started;i++)std::this_thread::sleep_for(std::chrono::milliseconds(10));assert(started);
    CloudAuto::Finish(); // the in-progress request times out and is detached
    const auto warning=CloudAuto::Start(root,root+"/second.sav"); // cannot wait for the old request
    hold=false;for(int i=0;i<500&&!finished;i++)std::this_thread::sleep_for(std::chrono::milliseconds(10));assert(finished);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CloudAuto::Saved();CloudAuto::Finish();
    std::lock_guard<std::mutex> lock(logMutex);
    if(jobs.size()!=1||warning.empty()){std::cerr<<"Busy game switch retained old context or omitted warning: jobs="<<jobs.size()<<" warning="<<warning<<'\n';return 1;}
    assert(jobs[0]==root+"/first.sav");
    std::cout<<"Busy cloud transition clears old game context and warns; no old-game retry is scheduled.\n";
}
