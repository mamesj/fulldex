#include "folders.h"
#include <cassert>
#include <map>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif
struct Folders {std::string saves,states,cheats,settings,cfg;};
static int changed=0,resolved=0,tabs=0,lists=0;
static std::map<std::string,std::string> config;
namespace Launcher {
struct Game {};
static int CopySavesTo(const std::string&,const std::string&,std::string&){assert(false);return -1;}
static void LoadTabs(const std::string&){++tabs;}
static void BuildRomList(const std::vector<Game>&,const std::string&){++lists;}
}
static void SetConfigValues(const std::string&,const std::vector<std::pair<std::string,std::string>>& values){++changed;for(auto& p:values)config[p.first]=p.second;}
static void ResolveFolders(Folders&,bool){++resolved;}
#include "own_folder_functions.h"
namespace fs=std::filesystem;
static void put(const fs::path& path,const char* contents){std::ofstream(path)<<contents;}
static std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
static void reset(){changed=resolved=tabs=lists=0;config.clear();}
static void failure(Folders& folders,const std::string& kind,const fs::path& destination){
    reset();auto message=UseOwnFolder(folders,kind,destination.string(),{});
    if(changed)std::cerr<<"incorrectly changed "<<kind<<" setting: "<<message<<std::endl;
    assert(changed==0&&resolved==0&&tabs==0&&lists==0&&config.empty());
    assert(message.find("IN USE NOW")==std::string::npos);
}
int main(int argc,char**argv){
    assert(argc==2);fs::path root=argv[1];
    for(const std::string kind:{"states","cheats","settings"}){
        auto source=root/(kind+"-source"),dest=root/(kind+"-dest");
        fs::create_directories(source/"nested");fs::create_directories(dest);
        put(source/"nested/file","original");put(dest/"nested","obstruction");
        Folders folders{(root/"other-saves").string(),source.string(),source.string(),source.string(),"unused.cfg"};
        failure(folders,kind,dest);assert(read(dest/"nested")=="obstruction");
        fs::remove(dest/"nested");
        reset();auto result=UseOwnFolder(folders,kind,dest.string(),{});
        assert(changed==1&&resolved==1&&tabs==1&&lists==1&&config[kind+"_folder"]==dest.string());
        assert(read(dest/"nested/file")=="original");
        put(dest/"nested/file","keep existing");reset();UseOwnFolder(folders,kind,dest.string(),{});
        assert(read(dest/"nested/file")=="keep existing"&&read(source/"nested/file")=="original");
        failure(folders,kind,source/"nested");
        fs::remove(dest/"nested/file");fs::create_directory(dest/"nested/file");
        failure(folders,kind,dest);fs::remove(dest/"nested/file");
#ifndef _WIN32
        auto outside=root/(kind+"-outside");fs::create_directories(outside);
        fs::remove(dest/"nested");fs::create_directory_symlink(outside,dest/"nested");
        failure(folders,kind,dest);assert(!fs::exists(outside/"file"));fs::remove(dest/"nested");
        fs::create_symlink(source/"nested/file",source/"link");failure(folders,kind,dest);fs::remove(source/"link");
        if(geteuid()!=0){
            fs::permissions(source/"nested",fs::perms::none);failure(folders,kind,dest);fs::permissions(source/"nested",fs::perms::owner_all);
            fs::permissions(dest,fs::perms::none);failure(folders,kind,dest);fs::permissions(dest,fs::perms::owner_all);
        }
#endif
    }
    // Classic layout copies just the selected kind; unrelated saves/settings stay.
    auto classic=root/"classic",states=root/"selected-states",settings=root/"selected-settings";
    fs::create_directories(classic/"artwork");fs::create_directories(states);fs::create_directories(settings);
    put(classic/"game.sav.state.slot1","state");put(classic/"game.sav","save");
    put(classic/"fulldex-ui.txt","setting");put(classic/"artwork/icon","art");
    Folders folders{classic.string(),"","",classic.string(),"unused.cfg"};
    reset();UseOwnFolder(folders,"states",states.string(),{});assert(changed==1);
    assert(read(states/"game.sav.state.slot1")=="state"&&!fs::exists(states/"game.sav")&&!fs::exists(states/"artwork"));
    reset();UseOwnFolder(folders,"settings",settings.string(),{});assert(changed==1);
    assert(read(settings/"fulldex-ui.txt")=="setting"&&read(settings/"artwork/icon")=="art"&&!fs::exists(settings/"game.sav"));
    // A genuinely absent optional source is allowed on a first install.
    folders.states=(root/"not-created").string();reset();UseOwnFolder(folders,"states",states.string(),{});assert(changed==1);
    std::cout<<"Actual folder action preserves settings on copy, containment, link and permission errors; successful copies/filtering still work\n";
}
