#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iterator>

static void Put(const std::filesystem::path& path, const char* data)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << data;
}
static std::string Read(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char** argv)
{
    assert(argc==2);
    namespace fs=std::filesystem;
    const fs::path root=fs::u8path(argv[1]);
    const fs::path kit=fs::u8path(N3ds::ProgramDir(root.string()));
    const fs::path sourceExe=fs::u8path(N3ds::Exe(kit.string()));
    Put(sourceExe,"master executable");
    Put(kit/"tools/w64devkit-x64.7z.exe","compiler archive");
    Put(kit/"tools/3dsrecomp","translator");
#ifndef _WIN32
    fs::permissions(kit/"tools/3dsrecomp",fs::perms::owner_all);
#endif
    Launcher::Game game;
    game.folder=(root/"3ds/000400000011C400").string();
    game.rom=(root/"synthetic.3ds").string();
    Put(fs::u8path(game.rom),"synthetic header only");
    const fs::path dest=fs::u8path(game.folder);
    const fs::path destExe=fs::u8path(N3ds::Exe(game.folder));
    Put(destExe,"existing executable"); // partial installation: executable arrived, tools did not
    Put(dest/"settings.ini","player settings");
    Put(dest/"title.so","prepared game");
    Put(dest/"data/user/main","player save");
    assert(N3ds::MakeFolder(root.string(),game));
    assert(Read(dest/"tools/w64devkit-x64.7z.exe")=="compiler archive");
    assert(Read(dest/"tools/3dsrecomp")=="translator");
    assert(Read(destExe)=="existing executable");
    assert(Read(dest/"settings.ini")=="player settings");
    assert(Read(dest/"title.so")=="prepared game");
    assert(Read(dest/"data/user/main")=="player save");
    assert(Read(fs::u8path(game.rom))=="synthetic header only");
#ifndef _WIN32
    assert((fs::status(dest/"tools/3dsrecomp").permissions()&fs::perms::owner_exec)!=fs::perms::none);
#endif
    assert(fs::remove(dest/"tools/3dsrecomp"));
    assert(N3ds::MakeFolder(root.string(),game));
    assert(Read(dest/"tools/3dsrecomp")=="translator"); // a later retry also repairs a removed tool
    assert(N3ds::MakeFolder((root/"no-master-kit").string(),game)); // prepared installs still work offline

    Launcher::Game broken=game;
    broken.folder=(root/"3ds/broken").string();
    Put(fs::u8path(N3ds::Exe(broken.folder)),"existing executable");
    Put(fs::u8path(broken.folder)/"tools","blocking file");
    assert(!N3ds::MakeFolder(root.string(),broken));
    assert(Read(fs::u8path(broken.folder)/"tools")=="blocking file");
    assert(fs::remove(fs::u8path(broken.folder)/"tools"));
    assert(N3ds::MakeFolder(root.string(),broken));
    assert(Read(fs::u8path(broken.folder)/"tools/w64devkit-x64.7z.exe")=="compiler archive");

    // Ultra Sun and Ultra Moon's add-on: the kit's SDL2.dll in front of the program's own (SDL2_real.dll).
    Put(kit/"SDL2.dll","add-on");
    Put(kit/"SDL2_real.dll","program's SDL2");
    Launcher::Game older=game;                     // made before the kit had the add-on
    older.folder=(root/"3ds/00040000001B5000").string();
    Put(fs::u8path(N3ds::Exe(older.folder)),"existing executable");
    Put(fs::u8path(older.folder)/"SDL2.dll","program's SDL2");
    assert(N3ds::MakeFolder(root.string(),older));
    assert(Read(fs::u8path(older.folder)/"SDL2.dll")=="add-on");
    assert(Read(fs::u8path(older.folder)/"SDL2_real.dll")=="program's SDL2");
    Launcher::Game fresh=game;                     // made now: a copy of the kit
    fresh.folder=(root/"3ds/00040000001B5100").string();
    assert(N3ds::MakeFolder(root.string(),fresh));
    assert(Read(fs::u8path(fresh.folder)/"SDL2.dll")=="add-on");
    assert(Read(fs::u8path(fresh.folder)/"SDL2_real.dll")=="program's SDL2");
    Launcher::Game byHand=game;                    // an add-on put in by hand is kept
    byHand.folder=(root/"3ds/000400000011C500").string();
    Put(fs::u8path(N3ds::Exe(byHand.folder)),"existing executable");
    Put(fs::u8path(byHand.folder)/"SDL2.dll","another add-on");
    Put(fs::u8path(byHand.folder)/"SDL2_real.dll","program's SDL2");
    assert(N3ds::MakeFolder(root.string(),byHand));
    assert(Read(fs::u8path(byHand.folder)/"SDL2.dll")=="another add-on");
    assert(N3ds::MakeFolder(root.string(),older));   // and once in, it stays
    assert(Read(fs::u8path(older.folder)/"SDL2.dll")=="add-on");

    // The add-on kept as the kit has it (Windows's SDL2.dll, Linux's usumfix.so; every build holds the name of the
    // file that turns it off): an older one in a game's folder gives way to the kit's, anything else is kept.
    const fs::path addon=N3ds::kAddon;
    Put(kit/addon,"add-on 2, usumfix_off.txt");
    Launcher::Game stale=game;                     // made by a FullDex with an older add-on
    stale.folder=(root/"3ds/0004000000055D00").string();
    Put(fs::u8path(N3ds::Exe(stale.folder)),"existing executable");
    Put(fs::u8path(stale.folder)/addon,"add-on 1, usumfix_off.txt");
    Put(fs::u8path(stale.folder)/"SDL2_real.dll","program's SDL2");
    assert(N3ds::MakeFolder(root.string(),stale));
    assert(Read(fs::u8path(stale.folder)/addon)=="add-on 2, usumfix_off.txt");
    Launcher::Game other=game;                     // something else in its place is kept
    other.folder=(root/"3ds/0004000000055E00").string();
    Put(fs::u8path(N3ds::Exe(other.folder)),"existing executable");
    Put(fs::u8path(other.folder)/addon,"something else");
    Put(fs::u8path(other.folder)/"SDL2_real.dll","program's SDL2");
    assert(N3ds::MakeFolder(root.string(),other));
    assert(Read(fs::u8path(other.folder)/addon)=="something else");
#ifndef _WIN32
    assert(N3ds::AddonPreload(older.folder,"/steam/overlay.so").empty()); // (a folder without it: nothing to load)
    assert(N3ds::MakeFolder(root.string(),older));   // a folder without it gets it
    assert(Read(fs::u8path(older.folder)/addon)=="add-on 2, usumfix_off.txt");
    // Linux: Play loads it first, in front of any others asked for (such as Steam's overlay); from a folder whose
    // path has a space in it (as this one has), as ./usumfix.so, the folder the program starts in
    const std::string mine="./usumfix.so";
    assert(N3ds::AddonPreload(older.folder,nullptr)==mine);
    assert(N3ds::AddonPreload(older.folder,"")==mine);
    assert(N3ds::AddonPreload(older.folder,"/steam/overlay.so")==mine+":/steam/overlay.so");
    const fs::path plain=fs::temp_directory_path()/("fulldex-addon-test-"+std::to_string(getpid()));
    Put(plain/"usumfix.so","add-on 2, usumfix_off.txt");
    assert(N3ds::AddonPreload(plain.string(),"/steam/overlay.so")==(plain/"usumfix.so").string()+":/steam/overlay.so");
    fs::remove_all(plain);
    setenv("LD_PRELOAD","/steam/overlay.so",1);
    setenv("LD_PRELOAD_TEST_KEPT","1",1);
    const std::vector<std::string> env=N3ds::EnvWith("LD_PRELOAD",N3ds::AddonPreload(older.folder,getenv("LD_PRELOAD")));
    size_t preloads=0, kept=0;
    for (const std::string& e:env)
    {
        if (e.rfind("LD_PRELOAD=",0)==0) { preloads++; assert(e=="LD_PRELOAD="+mine+":/steam/overlay.so"); }
        kept+=e=="LD_PRELOAD_TEST_KEPT=1";
    }
    assert(preloads==1 && kept==1);
#endif
#ifdef _WIN32
    // The #208 log's install directory is 219 characters. Its source compiler path
    // fits MAX_PATH; the per-title copy grows to 267 characters and must still copy.
    const size_t padding=219-root.string().size()-1;
    assert(padding>0 && padding<240);
    const fs::path longApp=root/std::string(padding,'x');
    const fs::path longKit=fs::u8path(N3ds::ProgramDir(longApp.string()));
    Put(fs::u8path(N3ds::Exe(longKit.string())),"master executable");
    Put(longKit/"tools/w64devkit-x64.7z.exe","compiler archive");
    assert(Read(longKit/"tools/w64devkit-x64.7z.exe")=="compiler archive");
    Launcher::Game longGame=game;
    longGame.folder=(longApp/"3ds/000400000011C400").string();
    Put(fs::u8path(N3ds::Exe(longGame.folder)),"existing executable");
    assert(N3ds::MakeFolder(longApp.string(),longGame));
    std::error_code ec;
    const auto copied=RuntimeKit::NativePath(fs::u8path(longGame.folder)/"tools/w64devkit-x64.7z.exe",ec);
    assert(!ec && Read(copied)=="compiler archive");
#endif
}
