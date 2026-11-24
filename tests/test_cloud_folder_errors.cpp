#include "cloud_saves.h"
#include "net_http.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif
namespace Achievements {bool HttpRequest(const HttpCall&,int&,std::string&,std::string&){assert(false&&"network must not be used");return false;}const char* SystemName(){return "synthetic";}}
namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(argv[1]);auto local=root/"local",remote=root/"remote";fs::create_directories(local);fs::create_directories(remote);
    std::ofstream(local/"game.sav")<<"local progress";std::ofstream(remote/"game.sav")<<"other device progress";
#ifndef _WIN32
    if(geteuid()!=0){
        fs::permissions(remote/"game.sav",fs::perms::none);
        CloudSaves::Sync sync(CloudSaves::FolderBackend(remote.string()),(local/"state").string(),"synthetic");
        auto result=sync.BeforePlay((local/"game.sav").string());
        fs::permissions(remote/"game.sav",fs::perms::owner_read|fs::perms::owner_write);
        if(result.outcome!=CloudSaves::Outcome::Failed || Read(remote/"game.sav")!="other device progress"){
            std::cerr<<"Unreadable remote treated as missing: "<<result.message<<"; remote="<<Read(remote/"game.sav")<<'\n';return 1;
        }
        assert(Read(local/"game.sav")=="local progress");
        std::cout<<"Unreadable remote refused without replacing either device's progress.\n";
    }else std::cout<<"Root can read permission-denied files; permission case unavailable.\n";
#else
    HANDLE lock=CreateFileW((remote/"game.sav").c_str(),GENERIC_WRITE,FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(lock!=INVALID_HANDLE_VALUE);
    auto probe=CloudSaves::FolderBackend(remote.string());std::string probeError;bool probeExists=true;CloudSaves::Meta probeMeta;
    const bool readable=probe->Stat("game.sav",probeExists,probeMeta,probeError);
    CloudSaves::Sync sync(CloudSaves::FolderBackend(remote.string()),(local/"state").string(),"synthetic");
    auto result=sync.BeforePlay((local/"game.sav").string());CloseHandle(lock);
    if(readable||result.outcome!=CloudSaves::Outcome::Failed||Read(remote/"game.sav")!="other device progress"){
        std::cerr<<"Locked existing remote was not rejected: "<<result.message<<"; remote="<<Read(remote/"game.sav")<<'\n';return 1;
    }
    std::cout<<"Windows sharing-denied remote refused without replacing either save.\n";
#endif
    auto backend=CloudSaves::FolderBackend(remote.string());std::string error;bool exists=true;CloudSaves::Meta meta;
    fs::create_directory(remote/"blocked.sav");assert(!backend->Stat("blocked.sav",exists,meta,error));assert(!error.empty());
    assert(backend->Stat("missing.sav",exists,meta,error)&&!exists);
    assert(backend->Stat("game.sav",exists,meta,error)&&exists);
    std::cout<<"Folder backend distinguishes unreadable, directory-obstructed and missing saves.\n";
}
