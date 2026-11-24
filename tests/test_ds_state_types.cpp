#include "state_slots.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);const auto root=fs::absolute(argv[1]);
    for(bool targetBlocked:{true,false}){
        auto slot=root/(targetBlocked?"blocked-slot":"blocked-previous");
        auto blocked=targetBlocked?slot:fs::path(slot.string()+".previous");
        fs::create_directory(blocked);
        if(targetBlocked)std::ofstream(blocked/"keep")<<"directory contents";
        else std::ofstream(slot)<<"old successful state";
        int writes=0;
        auto write=[&](const std::string& p){++writes;std::ofstream out(fs::u8path(p));out<<"new successful state";return bool(out);};
        if(StateSlots::Save(slot.u8string(),write)){std::cerr<<"Non-file state path was moved or removed during a successful save\n";return 1;}
        assert(writes==0);assert(fs::is_directory(blocked));
        if(targetBlocked){assert(Read(blocked/"keep")=="directory contents");assert(fs::remove(blocked/"keep"));}
        else assert(Read(slot)=="old successful state");
        assert(fs::remove(blocked));assert(StateSlots::Save(slot.u8string(),write));
        assert(Read(slot)=="new successful state");
        if(!targetBlocked)assert(Read(fs::path(slot.string()+".previous"))=="old successful state");
    }
    std::cout<<"DS state path obstructions are preserved; writing is refused and retry works.\n";
}
