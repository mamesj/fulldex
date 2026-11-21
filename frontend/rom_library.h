#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace RomLibrary
{
// A file name the game list could use: a ROM, a ROM hack's patch, a 3DS dump or a 3DS game's
// library (.so, Android). Told by its name only, so a folder of thousands of other files costs
// no more than reading its list once (#232: a phone's whole /roms with 3DS folders inside).
inline bool Wanted(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return false;
    const std::string ext = name.substr(dot);
    for (const char* e : { ".nds", ".gba", ".agb", ".ips", ".ups", ".bps", ".xdelta", ".xdelta3", ".vcdiff", ".delta",
                           ".3ds", ".cci", ".cia", ".so", ".zip" })
        if (ext == e) return true;
    return false;
}

// Folders inside a 3DS or Switch game taken apart, or a console's own system folders: never ROMs,
// and often thousands of files each, so they are not looked into.
inline bool Skipped(const std::string& lower)
{
    for (const char* n : { "romfs", "exefs", "exheader", "nintendo 3ds", "nintendo", "luma", "gm9", "sdmc", "nand", "sysdata", "shaders", "shadercache" })
        if (lower == n) return true;
    return false;
}

// Shared libraries often group ROMs by console or game. Do not scan saves, generated
// hack caches, hidden folders or directory symlinks back into the same library. The root comes
// first, then (sorted) only the folders that hold a file the list could use: the other passes
// over these folders (DS, GBA, hacks, 3DS) then never open the rest. A very large tree is cut off
// (kMaxEntries) rather than keep the list from showing.
inline std::vector<std::string> Folders(const std::string& root, const std::string& cache = "")
{
    namespace fs = std::filesystem;
    constexpr size_t kMaxEntries = 200000;
    std::vector<std::string> out;
    if (root.empty()) return out;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(root), ec)) return out;
    const fs::path cachePath = cache.empty() ? fs::path() : fs::weakly_canonical(fs::u8path(cache), ec);
    ec.clear();
    out.push_back(root);
    std::set<std::string> wanted;
    size_t entries = 0;
    fs::recursive_directory_iterator it(fs::u8path(root), fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec))
    {
        if (++entries > kMaxEntries)
        {
            fprintf(stderr, "[launcher] ROM folder has over %zu files: the rest are not looked at\n", kMaxEntries);
            break;
        }
        std::error_code entryError;
        std::string name = it->path().filename().u8string();
        if (!it->is_directory(entryError))
        {
            if (Wanted(name)) wanted.insert(it->path().parent_path().u8string());
            continue;
        }
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (it.depth() >= 8 || name.empty() || name[0] == '.' || name == "saves" || name == "backups" || Skipped(name) ||
            it->is_symlink(entryError))
        {
            it.disable_recursion_pending();
            continue;
        }
        if (!cachePath.empty() && fs::weakly_canonical(it->path(), entryError) == cachePath)
        {
            it.disable_recursion_pending();
            continue;
        }
    }
    auto trim = [](std::string s) { while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) s.pop_back(); return s; };
    for (const std::string& dir : wanted)
        if (trim(dir) != trim(root)) out.push_back(dir);
    std::sort(out.begin() + 1, out.end());
    out.erase(std::unique(out.begin() + 1, out.end()), out.end());
    return out;
}
}
