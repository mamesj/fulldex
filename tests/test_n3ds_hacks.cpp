// 3DS ROM hacks with a title id of their own (Pokemon X Legacy): listed beside their game, played from a
// copy that carries the game's title id, with a save of their own (n3ds.h). Synthetic dumps and a
// synthetic program only: no real ROM, kit, user data or game process is used.
#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void Put64(std::string& d, size_t at, uint64_t v) { for (int i = 0; i < 8; i++) d[at + i] = (char)(v >> (8 * i)); }
static void Put32(std::string& d, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) d[at + i] = (char)(v >> (8 * i)); }
static uint64_t Get64(const std::string& d, size_t at) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | (unsigned char)d[at + i]; return v; }

// (Layout: NCSD header; the first partition at 0x4000: NCCH header, extended header (0x800), ExeFS at 0xA00
// into the partition (.code 0x200, icon 0x36C0), then 0x2000 bytes standing for the RomFS.)
const size_t kNcch = 0x4000, kExefs = kNcch + 0xA00, kCode = kExefs + 0x200, kIcon = kCode + 0x200, kData = kIcon + 0x36C0;
static std::string MakeDump(uint64_t id, const char* product, const std::u16string& title, char code, char data)
{
    std::string d(kData + 0x2000, '\0');
    memcpy(&d[0x100], "NCSD", 4);
    Put64(d, 0x108, id);
    Put32(d, 0x120, (uint32_t)(kNcch / 0x200));
    Put32(d, 0x124, (uint32_t)((d.size() - kNcch) / 0x200));
    Put64(d, 0x190, id);
    memcpy(&d[kNcch + 0x100], "NCCH", 4);
    Put64(d, kNcch + 0x108, id);
    Put64(d, kNcch + 0x118, id);
    memcpy(&d[kNcch + 0x150], product, strlen(product));
    Put32(d, kNcch + 0x180, 0x400);
    d[kNcch + 0x18F] = 4; // (decrypted, marked so)
    Put32(d, kNcch + 0x1A0, (uint32_t)((kExefs - kNcch) / 0x200));
    Put64(d, kNcch + 0x200 + 0x1C8, id);
    Put64(d, kNcch + 0x200 + 0x200, id);
    Put64(d, kNcch + 0x200 + 0x600, id);
    {
        Sha256 s;
        s.Add((const uint8_t*)&d[kNcch + 0x200], 0x400);
        uint8_t h[32];
        s.Digest(h);
        memcpy(&d[kNcch + 0x160], h, 32);
    }
    memcpy(&d[kExefs], ".code", 5);
    Put32(d, kExefs + 8, 0);
    Put32(d, kExefs + 12, 0x200);
    if (!title.empty())
    {
        memcpy(&d[kExefs + 16], "icon", 4);
        Put32(d, kExefs + 16 + 8, 0x200);
        Put32(d, kExefs + 16 + 12, 0x36C0);
        memcpy(&d[kIcon], "SMDH", 4);
        for (size_t i = 0; i < title.size(); i++) { d[kIcon + 0x208 + 2 * i] = (char)(title[i] & 0xFF); d[kIcon + 0x209 + 2 * i] = (char)(title[i] >> 8); }
    }
    memset(&d[kCode], code, 0x200);
    d[kExefs + 0x1E0] = code; // (the header's hash of .code changes with it)
    memset(&d[kData], data, 0x2000);
    return d;
}
static void Write(const fs::path& p, const std::string& d) { std::ofstream(p, std::ios::binary).write(d.data(), (std::streamsize)d.size()); }
static std::string Read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }
static size_t Lines(const fs::path& p) { return N3ds::ReadLines(p.string()).size(); }
static void Touch(const fs::path& p, int seconds) { fs::last_write_time(p, fs::last_write_time(p) + std::chrono::seconds(seconds)); }
static void PrepareAndWait(const std::string& appDir, const Launcher::Game& g)
{
    N3ds::StartPrepare(appDir, g);
    N3ds::P().th.join();
    const auto s = N3ds::Peek();
    if (s.failed) { std::cerr << "preparing failed: " << s.error << "\n"; std::exit(1); }
    N3ds::Ack();
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const fs::path root = fs::absolute(argv[1]);
    // The program beside the test's own binary, as beside FullDex's: it records what it was given.
    const fs::path program = root / "3ds/program/pokemon-oras";
    fs::create_directories(program.parent_path());
    std::ofstream(program) << "#!/bin/sh\n"
                              "if [ \"$1\" = --prepare ]; then echo \"$2\" >> prepare-calls.txt; printf 'step translate\\nprogress 1 1\\n'; : > title.so; exit 0; fi\n"
                              "cat rom.txt >> play-calls.txt\n";
    fs::permissions(program, fs::perms::owner_all);
    const fs::path roms = root / "roms";
    fs::create_directories(roms);
    const uint64_t kX = 0x0004000000055D00ull, kLegacy = 0x000400000E4C3100ull;
    const std::string x = MakeDump(kX, "CTR-P-EKJA", u"Pokémon X", 1, 1);
    const std::string legacy = MakeDump(kLegacy, "CTR-P-EKJA", u"Pokémon X Legacy", 2, 2);
    Write(roms / "Pokemon X.3ds", x);
    Write(roms / "Pokemon_X_Legacy.3ds", legacy);

    // Recognised: the game as itself, the hack as its game with its own id and name; where hacks are not
    // played (no hack asked for), it is not listed and the list says why.
    {
        std::string p = (roms / "Pokemon X.3ds").string();
        N3ds::Hack h;
        const N3ds::Title* t = N3ds::Check(p, true, &h);
        assert(t && std::string(t->code) == "EKJA" && h.id.empty());
        p = (roms / "Pokemon_X_Legacy.3ds").string();
        t = N3ds::Check(p, true, &h);
        assert(t && std::string(t->id) == "0004000000055D00" && h.id == "000400000E4C3100" && h.name == "POKEMON X LEGACY");
        N3ds::SkipNote().clear(); N3ds::SkipRank() = 0;
        assert(!N3ds::Check(p));
        assert(N3ds::SkipNote() == "Pokemon_X_Legacy.3ds: A ROM HACK: 3DS ROM HACKS PLAY ON WINDOWS AND LINUX FOR NOW");
        UiLang::Set("es");
        assert(UiLang::Tr(N3ds::SkipNote()) == "Pokemon_X_Legacy.3ds: UN ROM HACK: POR AHORA, LOS ROM HACKS DE 3DS SE JUEGAN EN WINDOWS Y LINUX");
        UiLang::Set("en");
    }
    // (No title in its icon, or the game's own: its file's name.)
    {
        Write(roms / "My_Hack.3ds", MakeDump(0x000400000E4C3900ull, "CTR-P-EK2A", u"Pokémon Y", 3, 3));
        std::string p = (roms / "My_Hack.3ds").string();
        N3ds::Hack h;
        const N3ds::Title* t = N3ds::Check(p, true, &h);
        assert(t && std::string(t->code) == "EK2A" && h.name == "MY HACK");
        fs::remove(roms / "My_Hack.3ds");
    }
    // (A title id of another game's, or a product code of none of the six: not a hack of them.)
    {
        Write(roms / "Other.3ds", MakeDump(0x0004000000123400ull, "CTR-P-ABCD", u"Other", 4, 4));
        std::string p = (roms / "Other.3ds").string();
        N3ds::Hack h;
        assert(!N3ds::Check(p, true, &h));
        fs::remove(roms / "Other.3ds");
    }

    // Listed side by side, each in a folder of its own; the hack's save is its own.
    std::vector<Launcher::Game> games;
    N3ds::AddPc({ roms.string() }, root.string(), games);
    assert(games.size() == 2);
    const Launcher::Game* gx = nullptr;
    const Launcher::Game* gl = nullptr;
    for (const Launcher::Game& g : games) (g.hack ? gl : gx) = &g;
    assert(gx && gl && gx->name == "POKEMON X" && gl->name == "POKEMON X LEGACY" && gl->hackId == "000400000E4C3100");
    assert(gx->folder == (root / "3ds/0004000000055D00").string() && gl->folder == (root / "3ds/000400000E4C3100").string());
    assert(N3ds::SaveName(*gx) == "pokemonx.sav" && N3ds::SaveName(*gl) == "hack-pokemon-x-legacy.sav");
    assert(N3ds::IdOf(*gl) == "0004000000055D00" && N3ds::GameSave(*gl) == gl->folder + "/data/user/savedata/0004000000055D00/main");
    // (A hack does not take its game's tile: Pokemon X's stays until its own dump is found.)
    {
        std::vector<Launcher::Game> onlyHack = { *gl };
        Launcher::AddComingSoon(onlyHack);
        N3ds::DropPlaceholders(onlyHack);
        bool xTile = false;
        for (const Launcher::Game& g : onlyHack) xTile |= g.comingSoon && g.code == "EKJA";
        assert(xTile);
        std::vector<Launcher::Game> both = games;
        Launcher::AddComingSoon(both);
        N3ds::DropPlaceholders(both);
        for (const Launcher::Game& g : both) assert(!(g.comingSoon && g.code == "EKJA"));
    }
    const Launcher::Game hack = *gl;
    const fs::path folder = hack.folder;

    // Prepared from a copy that carries Pokemon X's title id; the player's dump untouched.
    assert(!N3ds::Prepared(hack));
    assert(N3ds::Play(root.string(), hack) == -1 && !fs::exists(folder / "play-calls.txt")); // (never before it is prepared)
    PrepareAndWait(root.string(), hack);
    assert(Read(roms / "Pokemon_X_Legacy.3ds") == legacy);
    const std::string copy = Read(folder / "game.3ds");
    assert(copy.size() == legacy.size());
    for (size_t at : { (size_t)0x108, (size_t)0x190, kNcch + 0x108, kNcch + 0x118, kNcch + 0x200 + 0x1C8, kNcch + 0x200 + 0x200, kNcch + 0x200 + 0x600 })
        assert(Get64(copy, at) == kX);
    {
        Sha256 s;
        s.Add((const uint8_t*)&copy[kNcch + 0x200], 0x400);
        uint8_t h[32];
        s.Digest(h);
        assert(memcmp(h, &copy[kNcch + 0x160], 32) == 0);
        // (Nothing else changed: the hack's own bytes, its new ids and hash put back, are the copy.)
        std::string back = legacy;
        for (size_t at : { (size_t)0x108, (size_t)0x190, kNcch + 0x108, kNcch + 0x118, kNcch + 0x200 + 0x1C8, kNcch + 0x200 + 0x200, kNcch + 0x200 + 0x600 }) Put64(back, at, kX);
        memcpy(&back[kNcch + 0x160], h, 32);
        assert(back == copy);
    }
    assert(Lines(folder / "prepare-calls.txt") == 1 && N3ds::ReadLines((folder / "prepare-calls.txt").string())[0] == (folder / "game.3ds").string());
    assert(N3ds::Prepared(hack));
    assert(N3ds::ReadLines((folder / "game-code.txt").string())[0] == N3ds::CodePrint((roms / "Pokemon_X_Legacy.3ds").string()));
    // (Played: the program is told the copy.)
    assert(N3ds::Play(root.string(), hack) == 0);
    assert(N3ds::ReadLines((folder / "play-calls.txt").string()).back() == (folder / "game.3ds").string());

    // A new version whose data alone changed: copied again, its code not built again.
    Write(roms / "Pokemon_X_Legacy.3ds", MakeDump(kLegacy, "CTR-P-EKJA", u"Pokémon X Legacy", 2, 7));
    Touch(roms / "Pokemon_X_Legacy.3ds", 10);
    assert(!N3ds::Prepared(hack));
    assert(N3ds::Play(root.string(), hack) == -1);
    PrepareAndWait(root.string(), hack);
    assert(Lines(folder / "prepare-calls.txt") == 1 && N3ds::Prepared(hack));
    assert(Read(folder / "game.3ds")[kData] == 7);

    // A new version whose code changed: the old code goes, and it is built again.
    Write(roms / "Pokemon_X_Legacy.3ds", MakeDump(kLegacy, "CTR-P-EKJA", u"Pokémon X Legacy", 9, 7));
    Touch(roms / "Pokemon_X_Legacy.3ds", 20);
    assert(!N3ds::Prepared(hack));
    PrepareAndWait(root.string(), hack);
    assert(Lines(folder / "prepare-calls.txt") == 2 && N3ds::Prepared(hack));
    assert(Read(folder / "game.3ds")[kCode] == 9);

    // Pokemon X itself: played from the player's own dump, as before.
    PrepareAndWait(root.string(), *gx);
    assert(N3ds::ReadLines((fs::path(gx->folder) / "prepare-calls.txt").string())[0] == (roms / "Pokemon X.3ds").string());
    assert(!fs::exists(fs::path(gx->folder) / "game.3ds") && N3ds::Prepared(*gx));
    std::cout << "3DS ROM hacks: listed beside their game, played from a retitled copy, rebuilt only when their code changes.\n";
}
