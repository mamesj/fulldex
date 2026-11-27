// Linux: a 3DS game's program started by N3ds::Play gets the add-on (tools/pc/usum-addon) first in LD_PRELOAD,
// in front of what was there (such as Steam's overlay), and starts in the game's folder; without the add-on it
// starts as before. The program here is a stand-in that writes down what it was given.
#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iterator>

static void Put(const std::filesystem::path& path, const std::string& data)
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
    assert(argc == 2);
    namespace fs = std::filesystem;
    const fs::path root = fs::u8path(argv[1]);
    // (the kit is beside this program, as it is beside FullDex's)
    const fs::path kit = fs::u8path(N3ds::ProgramDir(root.string()));
    Put(kit / "pokemon-oras", "#!/bin/sh\nprintf '%s\\n%s\\n' \"$LD_PRELOAD\" \"$(pwd)\" > seen.txt\n");
    fs::permissions(kit / "pokemon-oras", fs::perms::owner_all);
    Put(kit / "usumfix.so", "add-on, usumfix_off.txt");
    Put(root / "x.3ds", std::string(0x400, '\0'));
    Launcher::Game g;
    g.platform = Launcher::N3DS;
    g.name = "Pokemon X";
    g.folder = (root / "3ds/0004000000055D00").string();
    g.rom = (root / "x.3ds").string();
    // (ld.so says it cannot load these two, which are not libraries, and starts the program anyway)
    setenv("LD_PRELOAD", "/steam/overlay.so", 1);
    assert(N3ds::Play(root.string(), g) == 0);
    const std::string mine = g.folder.find_first_of(" :") == std::string::npos ? g.folder + "/usumfix.so" : "./usumfix.so";
    assert(Read(fs::u8path(g.folder) / "seen.txt") == mine + ":/steam/overlay.so\n" + g.folder + "\n");
    assert(Read(fs::u8path(g.folder) / "usumfix.so") == "add-on, usumfix_off.txt");
    // without the add-on: as before, LD_PRELOAD as it was
    fs::remove(kit / "usumfix.so");
    fs::remove(fs::u8path(g.folder) / "usumfix.so");
    unsetenv("LD_PRELOAD");
    assert(N3ds::Play(root.string(), g) == 0);
    assert(Read(fs::u8path(g.folder) / "seen.txt") == "\n" + g.folder + "\n");
}
