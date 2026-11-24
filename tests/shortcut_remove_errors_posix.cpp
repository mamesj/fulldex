#define STB_IMAGE_IMPLEMENTATION
#include "steam_shortcuts.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static void put(const fs::path& p){std::ofstream(p)<<"dummy shortcut";}
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(fs::u8path(argv[1]));assert(std::string(getenv("HOME"))==root.string());
    Launcher::Game g;g.code="IRBO";g.known=true;g.rom="fictional.nds";
#ifdef __APPLE__
    auto parent=root/"Applications",app=parent/(Shortcuts::Title(g)+".app"),desk=root/"Desktop"/(Shortcuts::Title(g)+".app");
    fs::create_directories(app);fs::create_directories(desk);put(app/"dummy");put(desk/"dummy");
#else
    const std::string id=Shortcuts::LinuxShortcutId(g),name="pokemon-ds-wide-"+Shortcuts::Lower(id)+".desktop";
    auto parent=fs::u8path(Shortcuts::LinuxAppsDir()),app=parent/name,desk=fs::u8path(Shortcuts::LinuxDesktopDir())/name;
    fs::create_directories(parent);fs::create_directories(desk.parent_path());put(app);put(desk);
    auto icon=root/"shortcut-icons"/(id+".png");fs::create_directories(icon.parent_path());put(icon);
#endif
    std::string where;
    SteamShortcuts::ActivePath().clear(); // Exercise the actual menu fallback, no account discovery.
    if(geteuid()!=0){
        fs::permissions(parent,fs::perms::owner_read|fs::perms::owner_exec);
#ifdef __APPLE__
        fs::permissions(app,fs::perms::owner_read|fs::perms::owner_exec);
#endif
        const auto message=SteamShortcuts::PlatformEntryDo(root.string(),g);
        const bool removed=message.rfind("REMOVED",0)==0;
        fs::permissions(parent,fs::perms::owner_all);
#ifdef __APPLE__
        fs::permissions(app,fs::perms::owner_all);
#endif
        if(removed)std::cerr<<"shortcut removal incorrectly reported success after permission denial"<<std::endl;
        assert(!removed&&message.rfind("COULD NOT",0)==0&&fs::exists(app)&&fs::exists(desk));
#ifndef __APPLE__
        assert(fs::exists(icon));
#endif
        std::cout<<"Permission-denied case exercised as non-root\n";
    }
    assert(Shortcuts::Remove(root.string(),g,where));assert(!fs::exists(app)&&!fs::exists(desk));
    assert(Shortcuts::Remove(root.string(),g,where));
    std::cout<<"Actual removal reports permission errors, stops later cleanup and allows retry/missing shortcuts\n";
}
