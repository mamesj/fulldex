#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static void put(const fs::path& p,const std::string& text){std::ofstream(p)<<text;}
static std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
static void directory(const fs::path& config,const std::string& value){fs::create_directories(config);put(config/"user-dirs.dirs","XDG_DESKTOP_DIR=\""+value+"\"\n");}
int main(int argc,char**argv){
    assert(argc==3);auto root=fs::absolute(fs::u8path(argv[1]));const std::string mode=argv[2];assert(std::string(getenv("HOME"))==root.string());
    const bool disabled=mode=="disabled"||mode=="home-slash"||mode=="empty";
    fs::path expected=root/"Desktop";
    if(mode=="disabled")directory(root/".config","$HOME");
    else if(mode=="home-slash")directory(root/".config","$HOME/");
    else if(mode=="empty")directory(root/".config","");
    else if(mode=="custom"){
        directory(root/".config","$HOME/Wrong Desktop");fs::create_directories(root/"Wrong Desktop");
        expected=root/"Chosen Desktop";directory(root/"xdg-config",expected.u8string());
    }else if(mode=="relative-config"||mode=="relative-data")directory(root/".config","$HOME/Desktop");
    else assert(mode=="default");
    if(!disabled)fs::create_directories(expected);
    Launcher::Game g;g.code="IRBO";g.known=true;g.rom=(root/"fictional.nds").u8string();g.hasIcon=true;
    std::fill(std::begin(g.icon),std::end(g.icon),0xff4080c0);
    const auto file="pokemon-ds-wide-"+Shortcuts::Lower(Shortcuts::LinuxShortcutId(g))+".desktop";
    const auto homeEntry=root/file;put(homeEntry,"KEEP HOME ENTRY");
    const auto expectedApps=(mode=="relative-data"?root/".local/share/applications":root/"data/applications");
    assert(fs::u8path(Shortcuts::LinuxAppsDir())==expectedApps);
    std::string where;assert(Shortcuts::Make(root.string(),{g},where)==1);
    if(read(homeEntry)!="KEEP HOME ENTRY")std::cerr<<"disabled desktop overwrote an existing HOME entry"<<std::endl;
    assert(read(homeEntry)=="KEEP HOME ENTRY");
    assert((fs::status(homeEntry).permissions()&fs::perms::owner_exec)==fs::perms::none);
    const auto app=fs::u8path(Shortcuts::LinuxAppsDir())/file;assert(fs::exists(app));
    if(disabled){assert(Shortcuts::LinuxDesktopDir().empty());assert(where=="IN YOUR APPS MENU");}
    else{assert(fs::exists(expected/file));assert(Shortcuts::LinuxDesktopDir()==expected.string());}
    assert(Shortcuts::Exists(root.string(),g));assert(Shortcuts::Remove(root.string(),g,where));
    assert(!fs::exists(app)&&!Shortcuts::Exists(root.string(),g));assert(read(homeEntry)=="KEEP HOME ENTRY");
    if(disabled)assert(where=="FROM YOUR APPS MENU");else assert(!fs::exists(expected/file));
    std::cout<<"XDG desktop case "<<mode<<": correct directory, applications-menu create/remove and protected HOME entry passed\n";
}
