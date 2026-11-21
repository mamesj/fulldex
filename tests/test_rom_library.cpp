#include "../frontend/rom_library.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs = std::filesystem;
    const fs::path root = fs::u8path(argv[1]);
    auto touch = [](const fs::path& p) { fs::create_directories(p.parent_path()); std::ofstream(p) << "x"; };
    touch(root / "DS" / "Platinum" / "Platinum.nds");
    touch(root / "DS" / "notes.txt");                 // (DS itself: no ROM directly in it)
    touch(root / "GBA" / "Emerald.GBA");               // (any letter case)
    touch(root / "GBA" / "Emerald" / "Emerald.gba");
    touch(root / "hacks" / "Renegade" / "Renegade.xdelta");
    fs::create_directories(root / "Empty" / "nested");
    for (const char* name : {"saves", "backups", ".hidden"}) touch(root / name / "nested" / "Game.nds");
    touch(root / "generated-cache" / "nested" / "Game.nds");
    // (#232: a 3DS game taken apart, thousands of files; its romfs is not looked into.)
    for (int i = 0; i < 50; i++) touch(root / "3DS" / "Game" / "romfs" / ("file" + std::to_string(i) + ".nds"));
    touch(root / "3DS" / "Pokemon X.3ds");
    const auto folders = RomLibrary::Folders(root.u8string(), (root / "generated-cache").u8string());
    assert(folders.front() == root.u8string());
    for (const auto& p : {root / "DS" / "Platinum", root / "GBA", root / "GBA" / "Emerald", root / "hacks" / "Renegade", root / "3DS"})
        assert(std::find(folders.begin(), folders.end(), p.u8string()) != folders.end());
    assert(folders.size() == 6);
    assert(RomLibrary::Folders("").empty());
    assert(RomLibrary::Folders((root / "missing").u8string()).empty());
    assert(RomLibrary::Wanted("a.NDS") && RomLibrary::Wanted("b.3ds") && !RomLibrary::Wanted("c.bin") && !RomLibrary::Wanted("nds"));
    std::error_code ec;
    fs::create_directory_symlink(root, root / "loop", ec);
    if (!ec) assert(RomLibrary::Folders(root.u8string(), (root / "generated-cache").u8string()).size() == 6);
}
