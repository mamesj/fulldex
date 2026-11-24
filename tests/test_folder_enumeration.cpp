#include "folders.h"
#include <cassert>
#include <map>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif
struct Folders {std::string dir,roms,saves,states,cheats,settings,cfg;};
static int changes=0,resolves=0;
static std::map<std::string,std::string> config;
namespace Launcher {
struct Game {};
static std::string ConfigValue(const std::string&,const char* key){return config[key];}
static void LoadTabs(const std::string&){}
static void BuildRomList(const std::vector<Game>&,const std::string&){}
}
static bool OwnSavesFolder(const Folders&){return false;}
static void SetConfigValues(const std::string&,const std::vector<std::pair<std::string,std::string>>& values){++changes;for(auto& p:values)config[p.first]=p.second;}
static void ResolveFolders(Folders&,bool){++resolves;}
#include "switch_folders_function.h"
namespace fs=std::filesystem;
static void put(const fs::path& path,const char* text){std::ofstream(path)<<text;}
static std::string read(const fs::path& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);fs::path root=argv[1];
    fs::create_directories(root/"states/nested");fs::create_directories(root/"saves");
    put(root/"states/one.state","one");put(root/"states/nested/two.state","two");
    Folders fo{root.string(),"",(root/"saves").string(),"","","","unused.cfg"};
    config["folders_layout"]="tidy";
#ifndef _WIN32
    if(geteuid()!=0){
        fs::permissions(root/"states/nested",fs::perms::none);
        auto result=SwitchFolders(fo,{});
        if(changes)std::cerr<<"incorrectly switched layout after incomplete listing: "<<result<<std::endl;
        assert(changes==0&&resolves==0&&config["folders_layout"]=="tidy");
        fs::permissions(root/"states/nested",fs::perms::owner_all);
        assert(read(root/"states/one.state")=="one"&&!fs::exists(root/"saves/one.state"));
    }
#endif
    auto result=SwitchFolders(fo,{});
    assert(config["folders_layout"]=="classic"&&changes==1&&resolves==1);
    assert(read(root/"saves/one.state")=="one"&&read(root/"saves/nested/two.state")=="two");
    auto extra=root/"extra";fs::create_directories(extra);put(extra/"new.state","new");
    std::string why;const auto journal=(root/"folders-move.txt").string();
#ifndef _WIN32
    if(geteuid()!=0){
        fs::permissions(extra,fs::perms::none);
        assert(!FolderSet::Undo(journal,{{extra.string(),(root/"old-extra").string()}},{root.string()},why));
        fs::permissions(extra,fs::perms::owner_all);
        assert(fs::exists(journal)&&read(root/"saves/one.state")=="one"&&read(extra/"new.state")=="new");
    }
    // A failed collection must leave any earlier planned pairs alone.
    FolderSet::Pairs pairs{{"already-planned","destination"}};
    fs::create_symlink(root/"saves/one.state",extra/"linked.state");
    FolderSet::AddTree(extra.string(),(root/"elsewhere").string(),pairs);
    assert(pairs.size()==1&&pairs[0].first=="already-planned");
    fs::remove(extra/"linked.state");
#endif
    FolderSet::Pairs ordinary;FolderSet::AddTree(extra.string(),(root/"elsewhere").string(),ordinary);
    assert(ordinary.size()==1&&fs::u8path(ordinary[0].second).lexically_normal()==(root/"elsewhere/new.state").lexically_normal());
    std::cout<<"Actual layout action and undo stop on incomplete enumeration; sources, journal and prior plan preserved; ordinary migration succeeds\n";
}
