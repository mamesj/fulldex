#include "state_slots.h"
#include "state_file.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){assert(argc==2);auto root=fs::absolute(argv[1]);
    for(bool gba:{false,true})for(bool recoveryDir:{false,true}){
        auto slot=root/((gba?"gba":"ds")+std::string(recoveryDir?"-recovery":"-pending"));
        std::ofstream(slot)<<"last good state";
        auto pending=fs::path(slot.string()+".pending"),failed=fs::path(slot.string()+".pending.failed");
        auto blocked=recoveryDir?failed:pending;fs::create_directory(blocked);
        if(recoveryDir)std::ofstream(pending)<<"interrupted bytes";
        else std::ofstream(blocked/"keep")<<"directory data";
        int writes=0;auto writer=[&](const std::string& p){++writes;std::ofstream f(fs::u8path(p));f<<"new state bytes";return bool(f);};
        auto save=[&](){return gba?GbaStateFile::Save(slot.u8string(),writer):StateSlots::Save(slot.u8string(),writer);};
        if(save()){std::cerr<<(gba?"GBA":"DS")<<" moved or removed a directory during interrupted-state recovery\n";return 1;}
        assert(writes==0);assert(Read(slot)=="last good state");assert(fs::is_directory(blocked));
        if(!recoveryDir){assert(Read(blocked/"keep")=="directory data");fs::remove(blocked/"keep");}
        else assert(Read(pending)=="interrupted bytes");
        assert(fs::remove(blocked));assert(save());assert(Read(slot)=="new state bytes");
        if(recoveryDir)assert(Read(failed)=="interrupted bytes");
    }
    std::cout<<"DS/GBA pending and recovery directory obstructions are preserved, with successful retry.\n";
}
