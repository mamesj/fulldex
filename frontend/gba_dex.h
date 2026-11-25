#pragma once
// The Pokedex page's data for the GBA games (Ruby, Sapphire, Emerald, FireRed, LeafGreen and the
// hacks built on them), read from the player's own ROM at run time. The tables are found by what
// they hold, not by fixed addresses, so translations and hacks that keep their layout work too:
// base stats by Bulbasaur's and Ivysaur's, names by "BULBASAUR", the pictures by their tables of
// {picture, size 0x800, species} and {palette, tag} entries (pokeemerald's gMonFrontPicTable,
// gMonPaletteTable and gMonShinyPaletteTable). No game graphics or data are bundled or written.
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <set>
#include <vector>
#include <map>
#include "gba_smol.h"

namespace GbaDex
{
using Bytes = std::vector<uint8_t>;
inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
inline bool Range(size_t size, size_t at, size_t n) { return at <= size && n <= size - at; }

// The accented letters of the European games (pokeemerald's charmap.txt), as UTF-8.
inline const char* Accented(uint8_t c)
{
    switch (c)
    {
    case 0x01: return "\xC3\x80"; case 0x02: return "\xC3\x81"; case 0x03: return "\xC3\x82"; case 0x04: return "\xC3\x87";
    case 0x05: return "\xC3\x88"; case 0x06: return "\xC3\x89"; case 0x07: return "\xC3\x8A"; case 0x08: return "\xC3\x8B";
    case 0x09: return "\xC3\x8C"; case 0x0B: return "\xC3\x8E"; case 0x0C: return "\xC3\x8F"; case 0x0D: return "\xC3\x92";
    case 0x0E: return "\xC3\x93"; case 0x0F: return "\xC3\x94"; case 0x10: return "\xC5\x92"; case 0x11: return "\xC3\x99";
    case 0x12: return "\xC3\x9A"; case 0x13: return "\xC3\x9B"; case 0x14: return "\xC3\x91"; case 0x15: return "\xC3\x9F";
    case 0x16: return "\xC3\xA0"; case 0x17: return "\xC3\xA1"; case 0x19: return "\xC3\xA7"; case 0x1A: return "\xC3\xA8";
    case 0x1B: return "\xC3\xA9"; case 0x1C: return "\xC3\xAA"; case 0x1D: return "\xC3\xAB"; case 0x1E: return "\xC3\xAC";
    case 0x20: return "\xC3\xAE"; case 0x21: return "\xC3\xAF"; case 0x22: return "\xC3\xB2"; case 0x23: return "\xC3\xB3";
    case 0x24: return "\xC3\xB4"; case 0x25: return "\xC5\x93"; case 0x26: return "\xC3\xB9"; case 0x27: return "\xC3\xBA";
    case 0x28: return "\xC3\xBB"; case 0x29: return "\xC3\xB1"; case 0x2D: return "&"; case 0x2E: return "+";
    case 0x5A: return "\xC3\x8D"; case 0x68: return "\xC3\xA2"; case 0x6F: return "\xC3\xAD";
    case 0xF1: return "\xC3\x84"; case 0xF2: return "\xC3\x96"; case 0xF3: return "\xC3\x9C";
    case 0xF4: return "\xC3\xA4"; case 0xF5: return "\xC3\xB6"; case 0xF6: return "\xC3\xBC";
    }
    return nullptr;
}
// The games' own characters (the Latin ones; pokeemerald's charmap.txt).
inline std::string Text(const uint8_t* p, size_t max)
{
    std::string out;
    for (size_t i = 0; i < max; i++)
    {
        const uint8_t c = p[i];
        if (c == 0xFF) break;
        // (A control code and its argument, as Ruby's two-line place names have: a space.)
        if (c == 0xFC) { if (out.empty() || out.back() != ' ') out += ' '; i++; continue; }
        if (c == 0x00 && !out.empty() && out.back() == ' ') continue;
        if (c >= 0xBB && c <= 0xD4) out += (char)('A' + c - 0xBB);
        else if (c >= 0xD5 && c <= 0xEE) out += (char)('a' + c - 0xD5);
        else if (c >= 0xA1 && c <= 0xAA) out += (char)('0' + c - 0xA1);
        else if (c == 0x00) out += ' ';
        else if (c == 0xAD) out += '.';
        else if (c == 0xAE) out += '-';
        else if (c == 0xB4) out += '\'';
        else if (c == 0xB5) out += "\xE2\x99\x82"; // (male sign)
        else if (c == 0xB6) out += "\xE2\x99\x80"; // (female sign)
        else if (const char* a = Accented(c)) out += a;
        else if (c == 0xB8) out += ',';
        else if (c == 0xAB) out += '!';
        else if (c == 0xAC) out += '?';
        else if (c == 0xBA) out += '/';
        else if (c == 0xB0) out += "...";
        else if (c == 0x5C) out += '(';
        else if (c == 0x5D) out += ')';
        else out += '?';
    }
    return out;
}
inline Bytes Encode(const char* s)
{
    Bytes out;
    for (; *s; s++)
    {
        const char c = *s;
        if (c >= 'A' && c <= 'Z') out.push_back((uint8_t)(0xBB + c - 'A'));
        else if (c >= 'a' && c <= 'z') out.push_back((uint8_t)(0xD5 + c - 'a'));
        else if (c >= '0' && c <= '9') out.push_back((uint8_t)(0xA1 + c - '0'));
        else if (c == ' ') out.push_back(0);
        else if (c == '-') out.push_back(0xAE);
    }
    return out;
}
inline size_t Find(const Bytes& rom, const Bytes& what, size_t from = 0, size_t align = 1)
{
    if (what.empty() || rom.size() < what.size()) return std::string::npos;
    for (auto it = std::search(rom.begin() + std::min(from, rom.size()), rom.end(), what.begin(), what.end()); it != rom.end();
         it = std::search(it + 1, rom.end(), what.begin(), what.end()))
        if ((size_t)(it - rom.begin()) % align == 0) return (size_t)(it - rom.begin());
    return std::string::npos;
}
// A ROM pointer (0x08000000 on), as an offset; npos if it is not one.
inline size_t Pointer(const Bytes& rom, size_t at)
{
    if (!Range(rom.size(), at, 4)) return std::string::npos;
    const uint32_t p = U32(rom.data() + at);
    if (p < 0x08000000u || p >= 0x08000000u + rom.size()) return std::string::npos;
    return p - 0x08000000u;
}
// The BIOS's LZ77 (type 0x10), as the pictures and palettes are packed.
inline Bytes Lz77(const Bytes& rom, size_t at, size_t limit = 0x10000)
{
    if (!Range(rom.size(), at, 4) || rom[at] != 0x10) return {};
    const size_t size = rom[at + 1] | rom[at + 2] << 8 | rom[at + 3] << 16;
    if (!size || size > limit) return {};
    Bytes out(size);
    size_t p = at + 4, o = 0;
    while (o < size)
    {
        if (p >= rom.size()) return {};
        const uint8_t flags = rom[p++];
        for (int bit = 7; bit >= 0 && o < size; bit--)
        {
            if (!(flags >> bit & 1)) { if (p >= rom.size()) return {}; out[o++] = rom[p++]; continue; }
            if (p + 1 >= rom.size()) return {};
            const unsigned v = rom[p] << 8 | rom[p + 1];
            p += 2;
            const size_t n = (v >> 12) + 3, disp = (v & 0xFFF) + 1;
            if (disp > o) return {};
            for (size_t k = 0; k < n && o < size; k++, o++) out[o] = out[o - disp];
        }
    }
    return out;
}

struct Stats
{
    bool ok = false;
    int base[6] = {}; // HP, Attack, Defense, Sp. Atk, Sp. Def, Speed
    int ev[6] = {};
    int type1 = 0, type2 = 0, catchRate = 0, baseExp = 0, gender = 0, hatchCycles = 0, friendship = 0, growth = 0;
    int eggGroup1 = 0, eggGroup2 = 0, ability1 = 0, ability2 = 0, hidden = 0;
};
struct Picture { int width = 0, height = 0; std::vector<uint32_t> pixels; bool Ready() const { return width > 0 && pixels.size() == (size_t)width * height; } };

struct Data
{
    size_t stats = std::string::npos, statsStride = 28;
    size_t names = std::string::npos, nameLength = 11;
    size_t abilities = std::string::npos, abilityLength = 13;
    size_t national = std::string::npos; // (u16 a species from species 1: its National Pokedex number)
    size_t frontPics = std::string::npos, palettes = std::string::npos, shinyPalettes = std::string::npos;
    size_t moveNames = std::string::npos, levelMoves = std::string::npos, tmMoves = std::string::npos, tmBits = std::string::npos, eggMoves = std::string::npos;
    size_t items = std::string::npos, battleMoves = std::string::npos;
    size_t wildHeaders = std::string::npos, mapGroups = std::string::npos, mapsecNames = std::string::npos;
    uint32_t mapGroupsKey = 0; // (what gMapGroups' words are XORed with: Unbound's; 0 for none)
    // (gWildMonHeaders' shape: four encounter tables a header (land, water, rock smash, fishing), five in the
    // expansion (and hidden ones), and in its newer versions a set of them for each time of day.)
    int wildTables = 4, wildSets = 1;
    size_t WildStride() const { return 4 + (size_t)wildTables * wildSets * 4; }
    int mapsecFirst = 0;      // (the first region map section the names table has: 0x58 in FireRed and LeafGreen)
    bool mapsecPointers = false; // (FireRed and LeafGreen: a table of name pointers; the others: {x, y, w, h, name})
    int pictureCount = 0; // (species the picture tables cover)
    size_t picRun = 0, palRun = 0, shinyRun = 0;
    int count = 0; // species with a stats record (the table's length, as far as it could be told)
    const Bytes* rom = nullptr;
    // pokeemerald-expansion (Emerald Imperium, Heart and Soul 2, Wishes of Tomorrow, Generation 3 Evolved): every
    // Pokemon's data in one gSpeciesInfo entry, every move's in gMovesInfo, item's in gItemsInfo and ability's in
    // gAbilitiesInfo. Found by Game Freak's ROM header (0x100: gSpeciesInfo at 0xBC, gItemsInfo 0xC8, gMovesInfo 0xCC)
    // and the expansion's after it ("RHHEXP": its version, its counts and gAbilitiesInfo); the fields inside an entry
    // by what Bulbasaur's, Pound's and the first item's hold, as they move from version to version.
    struct Expansion
    {
        bool on = false;
        int version = 0; // (major * 10000 + minor * 100 + patch)
        int species = 0, moves = 0, items = 0, abilities = 0;
        size_t info = std::string::npos, stride = 0;
        int nameOff = -1, natDexOff = -1, abilitiesOff = -1, frontOff = -1, paletteOff = -1, shinyOff = -1;
        int levelUpOff = -1, teachableOff = -1, eggOff = -1;
        size_t movesInfo = std::string::npos, moveStride = 0;
        int moveTypeOff = -1, movePpOff = -1;
        size_t itemsInfo = std::string::npos, itemStride = 0;
        int itemNameOff = -1;
        bool itemNamePtr = false;
        size_t abilitiesInfo = std::string::npos, abilityStride = 0;
    } x;
    // A FireRed 1.0 edited in place (the Complete FireRed Upgrade's hacks: Radical Red, Unbound; Ultra Violet): a
    // table it moved is where FireRed 1.0's own code now loads it from. The words of the literal pools that held the
    // old table's address are read and the address most of them hold is taken, each checked by what it holds.
    bool cfru = false; // (CFRU's moves by level: {u16 move, u8 level}, level 0xFF last)
    bool pooled = false;
    // A hack on a pokeemerald of its own, its tables grown in its own shapes (R.O.W.E., Elite Redux, Inclement
    // Emerald, Monomyth): where each field of their records is, as FindOwn found them; these games' by default.
    struct Shape
    {
        bool own = false;
        bool namePtrs = false;                 // (the species' names: a table of pointers (Elite Redux's))
        bool u16Stats = false;                 // (base stats a u16 each: R.O.W.E.'s)
        int typeOff = 6, catchOff = 8, expOff = 9, evOff = 10, genderOff = 16, hatchOff = 17, friendOff = 18, growthOff = 19, eggOff = 20;
        bool expU16 = false;
        int abilityOff[3] = { 22, 23, -1 };    // (the third, the hidden one; -1 none)
        bool abilityU16 = false;
        int natDexOff = -1;                    // (a u16 in the record: the species' National Pokedex number)
        size_t identityDex = 0;                // (an own dex where species and number agree up to here)
        size_t moveNameLength = 13;
        size_t battleStride = 12; int battlePpOff = 2; // (battleMoves: entry 0's type; the PP that far after it)
        bool abilityPtrs = false;              // (the abilities' names: pointed to from records (Elite Redux's))
        size_t abilityStride = 4;
        size_t itemStride = 44; int itemNameOff = 0; size_t itemNameLength = 14; bool itemNamePtr = false; int itemCount = 0;
        int learnFormat = 0;                   // (moves by level: 0 these games' u16s, 1 {u16 move, u16 level}, 0xFFFF last)
        int typeMode = 0;                      // (2: the Fairy type at 18, these games' mystery type at 9)
    } sh;
    static size_t Voted(const Bytes& r, std::initializer_list<uint32_t> pools)
    {
        std::vector<std::pair<uint32_t, int>> votes;
        int n = 0;
        for (uint32_t at : pools)
        {
            if (!Range(r.size(), at, 4)) continue;
            const uint32_t v = U32(r.data() + at);
            n++;
            auto it = std::find_if(votes.begin(), votes.end(), [&](const std::pair<uint32_t, int>& q) { return q.first == v; });
            if (it == votes.end()) votes.push_back({ v, 1 }); else it->second++;
        }
        for (const auto& q : votes)
            if (q.second * 2 > n && q.first >= 0x08000000u && q.first - 0x08000000u < r.size()) return q.first - 0x08000000u;
        return std::string::npos;
    }
    void UsePools(const Bytes& r)
    {
        const uint8_t bulba[] = { 45, 49, 49, 45, 65, 65 }, ivy[] = { 60, 62, 63, 60, 80, 80 };
        const size_t st = Voted(r, { 0x12754, 0x12FA8, 0x13230, 0x16A60, 0x1F008, 0x21C28, 0x240B4, 0x2BABC });
        if (st == std::string::npos || !Range(r.size(), st + 56, 6) || memcmp(r.data() + st + 28, bulba, 6) || memcmp(r.data() + st + 56, ivy, 6)) return;
        pooled = true;
        if (st != stats)
        {
            stats = st; statsStride = 28; count = 0;
            for (int i = 1; i < 2048 && Range(r.size(), st + i * 28, 28) && r[st + i * 28 + 6] <= 25 && r[st + i * 28 + 7] <= 25; i++) count = i;
        }
        auto upper = [&](size_t at, size_t n) { std::string t = Text(r.data() + at, n); for (char& c : t) c = (char)toupper((unsigned char)c); return t; };
        size_t t = Voted(r, { 0x11450, 0x1150C, 0x115A4, 0x116BC, 0x34D80, 0x40FEC, 0x435DC, 0x49768 });
        if (t != std::string::npos && Range(r.size(), t, 22) && r[t] == 0xAC && upper(t + 11, 11) == "BULBASAUR") { names = t; nameLength = 11; }
        t = Voted(r, { 0x4323C, 0x432B0 });
        if (t != std::string::npos && Range(r.size(), t, 6) && U16(r.data() + t) == 1 && U16(r.data() + t + 2) == 2 && U16(r.data() + t + 4) == 3) national = t;
        t = Voted(r, { 0x3EA7C, 0x3EB10, 0x3EB84, 0x43DC8, 0x43E20, 0x43F84 });
        if (t != std::string::npos && Pointer(r, t + 4) != std::string::npos)
        {
            // (Bulbasaur's moves: CFRU's three bytes each (Tackle 21 00, level 1), or these games' word (Tackle | 1 << 9).)
            const size_t b = Pointer(r, t + 4);
            const bool three = Range(r.size(), b, 3) && U16(r.data() + b) == 33 && r[b + 2] == 1;
            const bool word = Range(r.size(), b, 2) && U16(r.data() + b) == (33 | 1 << 9);
            if (three || word) { levelMoves = t; cfru = three; }
        }
        t = Voted(r, { 0x45C50, 0x45CC8 });
        if (t != std::string::npos && Range(r.size(), t, 2) && U16(r.data() + t) == 20001) eggMoves = t;
        // (CFRU's machines are 16 bytes a Pokemon, their moves in a list FireRed's code doesn't load: none shown.)
        t = Voted(r, { 0x43C68, 0x43C80 });
        if (t != tmBits) tmBits = tmMoves = std::string::npos;
        t = Voted(r, { 0x308A4, 0x4EF84, 0x6BE8C, 0x9B524, 0xBD830, 0xBFAA4, 0xCC914, 0xD764C });
        if (t != std::string::npos && Range(r.size(), t, 26) && upper(t + 13, 13) == "POUND") moveNames = t;
        t = Voted(r, { 0x9A8D8, 0x9A8FC, 0x9A920, 0x9A944, 0x9A968, 0x9A98C, 0x9A9B0, 0x9A9D4 });
        if (t != std::string::npos && Range(r.size(), t, 88) && U16(r.data() + t + 44 + 14) == 1) items = t;
        t = Voted(r, { 0xE3D8, 0x128CC, 0x12BA4, 0x15060, 0x15FBC, 0x160F0, 0x16238, 0x16368 });
        if (t != std::string::npos && Range(r.size(), t, 24) && r[t + 13] == 40 && r[t + 15] == 100 && r[t + 16] == 35) battleMoves = t;
        t = Voted(r, { 0xD8004, 0xD8624, 0x1367A0 });
        for (size_t len : { 13, 17 })
            if (t != std::string::npos && Range(r.size(), t, 3 * len) && upper(t + len, len) == "STENCH" && upper(t + 2 * len, len) == "DRIZZLE") { abilities = t; abilityLength = len; }
        // (Pictures: {picture, 0x800, species} and {palette, species} a Pokemon, as many as it has.)
        const size_t pics = Voted(r, { 0xECA8, 0xECEC, 0xED68, 0xED80, 0xF0F8, 0xF130, 0xF1AC, 0xF1C4 });
        const size_t pals = Voted(r, { 0x4410C, 0x44130, 0x441A4, 0x839C0 }), shiny = Voted(r, { 0x44144, 0x441B4 });
        if (pics != std::string::npos && pals != std::string::npos && shiny != std::string::npos && Range(r.size(), pics, 16) && Range(r.size(), pals, 16) && Range(r.size(), shiny, 16) &&
            U16(r.data() + pics + 12) == 0x800 && U16(r.data() + pics + 14) == 1 && U16(r.data() + pals + 12) == 1 && Pointer(r, shiny + 8) != std::string::npos)
        {
            frontPics = pics; palettes = pals; shinyPalettes = shiny;
            picRun = palRun = shinyRun = (size_t)count + 1;
            pictureCount = count + 1;
        }
        t = Voted(r, { 0x82990, 0x82D4C, 0x82E18, 0x82EA8, 0x82F18, 0x82F68, 0x82FA4, 0x82FE4 });
        wildTables = 4; wildSets = 1;
        if (t != std::string::npos && WildEntry(r, t))
        {
            const size_t groups = Voted(r, { 0x5524C });
            size_t found = groups != std::string::npos && Header(r, Pointer(r, Pointer(r, groups))) ? groups : std::string::npos;
            uint32_t key = 0;
            // (Unbound keeps its gMapGroups' words XORed with a key of its own: found by trying, for its first group,
            // each table of map headers in the ROM, the key that turns its next words into such tables kept.)
            if (found == std::string::npos && groups != std::string::npos && Range(r.size(), groups, 24))
            {
                auto groupAt = [&](size_t q) { return Range(r.size(), q, 8) && Header(r, Pointer(r, q)) && Header(r, Pointer(r, q + 4)); };
                for (size_t q = 0; Range(r.size(), q, 8) && found == std::string::npos; q += 4)
                {
                    if (!groupAt(q)) continue;
                    const uint32_t k = U32(r.data() + groups) ^ (0x08000000u + (uint32_t)q);
                    int ok = 0;
                    for (int i = 1; i < 6; i++)
                    {
                        const uint32_t p = U32(r.data() + groups + i * 4) ^ k;
                        ok += p >= 0x08000000u && p - 0x08000000u < r.size() && Header(r, Pointer(r, p - 0x08000000u));
                    }
                    if (ok == 5) { found = groups; key = k; }
                }
            }
            if (t != wildHeaders || (found != std::string::npos && (found != mapGroups || key != mapGroupsKey)))
            {
                wildHeaders = t;
                mapGroups = found; mapGroupsKey = key;
                mapsecNames = std::string::npos;
                FindPlaces(r);
            }
        }
    }
    // The TMs and HMs (the expansion's: their items' names and the moves they teach), as {"TM01", move}.
    std::vector<std::pair<std::string, int>> teachers;
    // The type a hack's number is in these games' numbers (companion_types.h): the expansion counts from a type of
    // none at 0, one past theirs, and adds Fairy (19) and Stellar; -1 for one these games have no number for.
    // (The Complete FireRed Upgrade keeps these games' numbers and makes 23 the Fairy type; 9, the mystery type
    // Curse has in these games, is no type here.)
    int Type(int raw) const
    {
        if (sh.typeMode == 2) return raw == 18 ? 9 : raw == 9 || raw > 18 ? -1 : raw;
        if (!x.on) return raw == 23 ? 9 : raw == 9 ? -1 : raw;
        if (raw >= 1 && raw <= 18) return raw - 1;
        return raw == 19 ? 9 : -1;
    }
    // Whether the game has the Fairy type (and the newer chart with it): the expansion's, or Clefairy's own type.
    bool Fairy() const
    {
        if (x.on || sh.typeMode == 2) return true;
        const Stats c = Get(35);
        return c.ok && (c.type1 == 9 || c.type2 == 9);
    }
    const uint8_t* Entry(int species) const
    {
        if (!x.on || species <= 0 || species >= x.species || !Range(rom->size(), x.info + (size_t)species * x.stride, x.stride)) return nullptr;
        return rom->data() + x.info + (size_t)species * x.stride;
    }
    // A picture's tiles or a palette, LZ77 or smol (the newer expansion), or a raw palette of 16 colours.
    Bytes Unpack(size_t at, bool palette) const
    {
        if (at == std::string::npos || !Range(rom->size(), at, 8)) return {};
        if ((*rom)[at] == 0x10) return Lz77(*rom, at);
        if (!palette && GbaSmol::IsSmol(rom->data() + at, rom->size() - at)) return GbaSmol::Decode(rom->data() + at, rom->size() - at);
        if (palette && Range(rom->size(), at, 32)) return Bytes(rom->data() + at, rom->data() + at + 32);
        return {};
    }
    bool LoadExpansion(const Bytes& r)
    {
        x = Expansion();
        size_t h = std::string::npos;
        for (size_t at = 0x100; at + 24 <= 0x400 && at + 24 <= r.size(); at += 4)
            if (!memcmp(r.data() + at, "RHHEXP", 6)) { h = at; break; }
        if (h == std::string::npos) return false;
        x.version = r[h + 6] * 10000 + r[h + 7] * 100 + r[h + 8];
        x.moves = U16(r.data() + h + 10); x.species = U16(r.data() + h + 12); x.abilities = U16(r.data() + h + 14);
        x.abilitiesInfo = Pointer(r, h + 16); x.items = U16(r.data() + h + 20);
        x.info = Pointer(r, 0x100 + 0xBC); x.itemsInfo = Pointer(r, 0x100 + 0xC8); x.movesInfo = Pointer(r, 0x100 + 0xCC);
        if (x.info == std::string::npos || x.species < 400 || x.species > 4000 || x.moves < 300 || x.moves > 4000) return false;
        const uint8_t bulba[] = { 45, 49, 49, 45, 65, 65 }, ivy[] = { 60, 62, 63, 60, 80, 80 };
        for (size_t s = 28; s <= 0x200 && !x.stride; s += 4)
            if (Range(r.size(), x.info + 3 * s, 6) && !memcmp(r.data() + x.info + s, bulba, 6) && !memcmp(r.data() + x.info + 2 * s, ivy, 6)) x.stride = s;
        if (!x.stride || !Range(r.size(), x.info, (size_t)x.species * x.stride)) return false;
        x.on = true;
        const uint8_t* e[7];
        for (int i = 0; i < 7; i++) e[i] = r.data() + x.info + (size_t)i * x.stride;
        // (Bulbasaur's name, in either case.)
        for (int o = 0; o + 10 <= (int)x.stride && x.nameOff < 0; o++)
        {
            std::string n = Text(e[1] + o, 10);
            for (char& c : n) c = (char)toupper((unsigned char)c);
            if (n == "BULBASAUR" && e[1][o] == 0xBC) x.nameOff = o;
        }
        for (int o = 0; o + 2 <= (int)x.stride && x.natDexOff < 0; o += 2)
            if (U16(e[1] + o) == 1 && U16(e[2] + o) == 2 && U16(e[3] + o) == 3 && U16(e[6] + o) == 6) x.natDexOff = o;
        // (Its three abilities, the hidden one last: Overgrow and Chlorophyll among them.)
        for (int o = 8; o + 6 <= (int)x.stride && x.abilitiesOff < 0; o += 2)
            if (U16(e[1] + o) == 65 && U16(e[1] + o + 4) == 34) x.abilitiesOff = o;
        // (Its moves by level ({u16 move, u16 level}, a move of 0xFFFF last), then the ones it can be taught and its
        // egg moves (u16 lists, 0xFFFF last): three pointers in a row, Bulbasaur's and Ivysaur's.)
        auto levelList = [&](size_t p) {
            if (p == std::string::npos) return false;
            for (int i = 0; i < 100 && Range(r.size(), p + i * 4, 4); i++)
            {
                const unsigned m = U16(r.data() + p + i * 4), l = U16(r.data() + p + i * 4 + 2);
                if (m == 0xFFFF) return i > 0;
                if (!m || (int)m >= x.moves || l > 100) return false;
            }
            return false;
        };
        auto moveList = [&](size_t p, int least) {
            if (p == std::string::npos) return false;
            for (int i = 0; i < 400 && Range(r.size(), p + i * 2, 2); i++)
            {
                const unsigned m = U16(r.data() + p + i * 2);
                if (m == 0xFFFF) return i >= least;
                if (!m || (int)m >= x.moves) return false;
            }
            return false;
        };
        for (int o = 0; o + 12 <= (int)x.stride && x.levelUpOff < 0; o += 4)
        {
            const size_t a = e[1] - r.data() + o, b = e[2] - r.data() + o;
            if (levelList(Pointer(r, a)) && levelList(Pointer(r, b)) && moveList(Pointer(r, a + 4), 1) && moveList(Pointer(r, b + 4), 1) &&
                moveList(Pointer(r, a + 8), 0))
            { x.levelUpOff = o; x.teachableOff = o + 4; x.eggOff = o + 8; }
        }
        // (Its battle picture, 64 x 64 (two frames in some), then its palettes, normal and shiny.)
        for (int o = 0; o + 16 <= (int)x.stride && x.frontOff < 0; o += 4)
        {
            const Bytes tiles = Unpack(Pointer(r, e[1] - r.data() + o), false);
            if (tiles.size() != 0x800 && tiles.size() != 0x1000) continue;
            for (int q = o + 4; q + 8 <= (int)x.stride && q <= o + 16 && x.frontOff < 0; q += 4)
                if (Unpack(Pointer(r, e[1] - r.data() + q), true).size() == 32 && Unpack(Pointer(r, e[1] - r.data() + q + 4), true).size() == 32 &&
                    U16(Unpack(Pointer(r, e[1] - r.data() + q), true).data()) == U16(Unpack(Pointer(r, e[1] - r.data() + q + 4), true).data()) &&
                    Unpack(Pointer(r, e[1] - r.data() + q), true) != Unpack(Pointer(r, e[1] - r.data() + q + 4), true))
                { x.frontOff = o; x.paletteOff = q; x.shinyOff = q + 4; }
        }
        // (gMovesInfo: a name pointer first, POUND's then KARATE CHOP's; the type in the low five bits of a word
        // (Pound Normal, Karate Chop Fighting, Fire Punch Fire); the PP a byte (35, 25, 10, 15).)
        auto named = [&](size_t p, const char* want) {
            if (p == std::string::npos) return false;
            std::string n = Text(r.data() + p, std::min<size_t>(16, r.size() - p));
            for (char& c : n) c = (char)toupper((unsigned char)c);
            return n == want;
        };
        for (size_t s = 12; s <= 0x80 && x.movesInfo != std::string::npos && !x.moveStride; s += 4)
            if (Range(r.size(), x.movesInfo, 8 * s) && named(Pointer(r, x.movesInfo + s), "POUND") && named(Pointer(r, x.movesInfo + 2 * s), "KARATE CHOP")) x.moveStride = s;
        if (x.moveStride)
        {
            const uint8_t* m = r.data() + x.movesInfo;
            const size_t s = x.moveStride;
            for (int o = 4; o + 2 <= (int)s && x.moveTypeOff < 0; o += 2)
                if ((U16(m + s + o) & 31) == 1 && (U16(m + 2 * s + o) & 31) == 2 && (U16(m + 7 * s + o) & 31) == 11) x.moveTypeOff = o;
            for (int o = 4; o < (int)s && x.movePpOff < 0; o++)
                if (m[s + o] == 35 && m[2 * s + o] == 25 && m[3 * s + o] == 10 && m[4 * s + o] == 15) x.movePpOff = o;
        }
        // (gItemsInfo: item 0's name "????????", item 1's a POKé BALL or MASTER BALL, inline or by a pointer.)
        auto firstItem = [&](size_t p) {
            if (p == std::string::npos) return false;
            std::string n = Text(r.data() + p, std::min<size_t>(20, r.size() - p));
            for (char& c : n) c = (char)toupper((unsigned char)c);
            return n == "MASTER BALL" || n.rfind("POK", 0) == 0;
        };
        for (size_t s = 0x24; s <= 0x80 && x.itemsInfo != std::string::npos && !x.itemStride; s += 4)
            for (int o = 0; o + 4 <= (int)s && !x.itemStride && Range(r.size(), x.itemsInfo, 3 * s); o++)
            {
                const size_t a0 = x.itemsInfo + o, a1 = a0 + s;
                if (r[a0] == 0xAC && r[a0 + 1] == 0xAC && firstItem(a1) && r[a1 + s] != 0) { x.itemStride = s; x.itemNameOff = o; }
                else if (o % 4 == 0 && Pointer(r, a0) != std::string::npos && r[Pointer(r, a0)] == 0xAC && firstItem(Pointer(r, a1))) { x.itemStride = s; x.itemNameOff = o; x.itemNamePtr = true; }
            }
        // (gAbilitiesInfo: the name first, STENCH's then DRIZZLE's.)
        for (size_t s = 0x10; s <= 0x40 && x.abilitiesInfo != std::string::npos && !x.abilityStride; s += 4)
            if (Range(r.size(), x.abilitiesInfo, 3 * s) && named(x.abilitiesInfo + s, "STENCH") && named(x.abilitiesInfo + 2 * s, "DRIZZLE")) x.abilityStride = s;
        // (The TMs and HMs: items named so, each teaching the move in its secondary id (1.10), or the move a table of
        // {item, move} beside gItemsInfo gives it (1.15 on).)
        teachers.clear();
        std::vector<std::pair<std::string, int>> machines; // (name, item)
        for (int id = 1; id < x.items && x.itemStride; id++)
        {
            const std::string n = ItemName(id);
            if (n.size() >= 4 && (n[0] == 'T' || n[0] == 'H') && n[1] == 'M' && std::all_of(n.begin() + 2, n.end(), [](char c) { return c >= '0' && c <= '9'; }))
                machines.push_back({ n, id });
        }
        int byId = 0;
        for (const auto& m : machines) byId += U16(r.data() + x.itemsInfo + (size_t)m.second * x.itemStride + 4) > 0;
        if (!machines.empty() && byId * 2 > (int)machines.size())
            for (const auto& m : machines) teachers.push_back({ m.first, U16(r.data() + x.itemsInfo + (size_t)m.second * x.itemStride + 4) });
        else if (machines.size() >= 3)
        {
            const size_t from = x.itemsInfo > 0x10000 ? x.itemsInfo - 0x10000 : 0;
            for (size_t at = from; at + 12 <= x.itemsInfo + 0x10000 && Range(r.size(), at, 12) && teachers.empty(); at += 2)
                if ((int)U16(r.data() + at) == machines[0].second && (int)U16(r.data() + at + 4) == machines[1].second && (int)U16(r.data() + at + 8) == machines[2].second)
                    for (size_t q = at; Range(r.size(), q, 4); q += 4)
                    {
                        const int item = U16(r.data() + q), move = U16(r.data() + q + 2);
                        const auto m = std::find_if(machines.begin(), machines.end(), [&](const std::pair<std::string, int>& k) { return k.second == item; });
                        if (m == machines.end() || move <= 0 || move >= x.moves) break;
                        teachers.push_back({ m->first, move });
                    }
        }
        stats = x.info; statsStride = x.stride; count = x.species - 1;
        return true;
    }

    bool Ready() const { return rom && stats != std::string::npos; }
    void Load(const Bytes& r)
    {
        rom = &r;
        if (LoadExpansion(r)) { FindWild(r); pictureCount = x.frontOff >= 0 ? count + 1 : 0; return; }
        // (Base stats: Bulbasaur's 45 49 49 45 65 65, then Ivysaur's 60 62 63 60 80 80 a record on.
        // A hack may keep the old table and use a longer one elsewhere: the longest is taken.)
        const uint8_t bulba[] = { 45, 49, 49, 45, 65, 65 }, ivy[] = { 60, 62, 63, 60, 80, 80 };
        auto statsCount = [&](size_t table, size_t stride) {
            int n = 0;
            for (int i = 1; i < 2048 && Range(r.size(), table + i * stride, stride); i++)
            {
                const uint8_t* e = r.data() + table + i * stride;
                if (e[6] > 25 || e[7] > 25) break;
                n = i;
            }
            return n;
        };
        for (size_t at = Find(r, Bytes(bulba, bulba + 6)); at != std::string::npos; at = Find(r, Bytes(bulba, bulba + 6), at + 1))
            for (size_t stride : { 28 }) // (pokeemerald-expansion's records are laid out differently)
                if (at >= stride && Range(r.size(), at + stride, 6) && !memcmp(r.data() + at + stride, ivy, 6))
                {
                    const int n = statsCount(at - stride, stride);
                    if (n > count) { stats = at - stride; statsStride = stride; count = n; }
                }
        // (Names and abilities: tables of fixed-length entries, the first "??????????" for names and
        // "-------" for abilities in every language; the longest run of names that read as words.)
        auto word = [&](size_t at, size_t len) {
            int letters = 0;
            for (size_t i = 0; i < len && at + i < r.size(); i++)
            {
                const uint8_t c = r[at + i];
                if (c == 0xFF) return letters >= 2;
                if ((c >= 0xBB && c <= 0xEE) || (c >= 0xA1 && c <= 0xAA) || c == 0 || c == 0xAD || c == 0xAE || c == 0xB4 || c == 0xB5 || c == 0xB6 || c == 0x1B || c == 0xB8) { letters++; continue; }
                if (Accented(c)) { letters++; continue; }
                return false;
            }
            return letters >= 2;
        };
        auto table = [&](const Bytes& first, std::initializer_list<size_t> lengths, size_t& best, size_t& bestLen, int most) {
            int bestRun = 0;
            for (size_t at = Find(r, first); at != std::string::npos; at = Find(r, first, at + 1))
                for (size_t len : lengths)
                {
                    int run = 0;
                    while (run < most && word(at + (run + 1) * len, len)) run++;
                    if (run > bestRun) { bestRun = run; best = at; bestLen = len; }
                }
            return bestRun;
        };
        Bytes unknown(10, 0xAC); unknown.push_back(0xFF);
        Bytes dashes(7, 0xAE); dashes.push_back(0xFF);
        table(unknown, { 11, 12, 13 }, names, nameLength, 2048);
        table(dashes, { 13, 17 }, abilities, abilityLength, 1024);
        // (The National Pokedex number of each species, where the games keep their own order (Hoenn's
        // Pokemon from 277): 1 to 251 in turn, then Treecko's 252 at species 277.)
        for (size_t at = 0; Range(r.size(), at, 277 * 2) && national == std::string::npos; at += 2)
        {
            if (U16(r.data() + at) != 1 || U16(r.data() + at + 2) != 2 || U16(r.data() + at + 500) != 251) continue;
            bool ok = true;
            for (int i = 0; i < 251 && ok; i++) ok = U16(r.data() + at + i * 2) == i + 1;
            if (ok && U16(r.data() + at + 276 * 2) == 252) national = at;
        }
        // (Move names: 13 bytes each, the first "-": the longest such table that reads as words.)
        {
            const Bytes dash = { 0xAE, 0xFF };
            size_t len = 13; int best = 0;
            for (size_t found = Find(r, dash); found != std::string::npos; found = Find(r, dash, found + 1))
            {
                size_t at = found;
                while (at > 0 && r[at - 1] == 0xAE) at--; // (the first entry may be "--")
                int run = 0;
                while (run < 1024 && word(at + (run + 1) * 13, 13)) run++;
                if (run > best && run >= 300) { best = run; moveNames = at; }
            }
            (void)len;
        }
        // (Moves by level: a table of pointers, one a species, to u16 lists (move, and the level
        // times 512), 0xFFFF at the end; found by Bulbasaur's: Tackle 1, Growl 4, Leech Seed 7,
        // Vine Whip 10.)
        {
            const Bytes bulbaMoves = { 0x21, 0x02, 0x2D, 0x08, 0x49, 0x0E, 0x16, 0x14 };
            for (size_t at = Find(r, bulbaMoves, 0, 2); at != std::string::npos && levelMoves == std::string::npos; at = Find(r, bulbaMoves, at + 1, 2))
            {
                uint8_t ptr[4]; const uint32_t p = 0x08000000u + (uint32_t)at;
                memcpy(ptr, &p, 4);
                for (size_t q = Find(r, Bytes(ptr, ptr + 4), 0, 4); q != std::string::npos; q = Find(r, Bytes(ptr, ptr + 4), q + 4, 4))
                    if (q >= 4 && Pointer(r, q - 4) != std::string::npos && Pointer(r, q + 4) != std::string::npos) { levelMoves = q - 4; break; }
            }
        }
        // (TMs and HMs: their 58 moves (Focus Punch, Dragon Claw, Water Pulse, Calm Mind first), and
        // 8 bytes of bits a species, found by none for species 0 and Bulbasaur's.)
        {
            const Bytes first = { 0x08, 0x01, 0x51, 0x01, 0x60, 0x01, 0x5B, 0x01 };
            tmMoves = Find(r, first, 0, 2);
            const Bytes bits = { 0, 0, 0, 0, 0, 0, 0, 0, 32, 7, 53, 132, 8, 30 }; // (Bulbasaur: Toxic, Bullet Seed ... Cut, Strength)
            tmBits = Find(r, bits, 0, 4);
        }
        // (Egg moves: one list, each species' after 20000 plus its number; Bulbasaur's first.)
        {
            const Bytes marker = { 0x21, 0x4E };
            for (size_t at = Find(r, marker, 0, 2); at != std::string::npos; at = Find(r, marker, at + 2, 2))
            {
                size_t end = at + 2;
                while (Range(r.size(), end, 2) && U16(r.data() + end) > 0 && U16(r.data() + end) < 1000) end += 2;
                if (Range(r.size(), end, 2) && U16(r.data() + end) == 20004 && end - at > 4) { eggMoves = at; break; }
            }
        }
        // (A hack with more Pokemon than these games (Radical Red, Unbound) may keep the old move
        // tables beside its own: the moves only where the table covers all its Pokemon, the
        // machines and egg moves not at all, rather than show another game's.)
        UsePools(r);
        if (count > 420 && !pooled)
        {
            int n = 0;
            if (levelMoves != std::string::npos)
                while (n <= count && Pointer(r, levelMoves + n * 4) != std::string::npos) n++;
            if (n <= count) levelMoves = std::string::npos;
            tmMoves = tmBits = eggMoves = std::string::npos;
        }
        FindWild(r);
        // (Items: 44 bytes each, the name first and the item's own number at 14; found by items 1
        // and 2 numbering themselves.)
        for (size_t at = 0; Range(r.size(), at, 44 * 4) && items == std::string::npos; at += 4)
            if (U16(r.data() + at + 44 + 14) == 1 && U16(r.data() + at + 88 + 14) == 2 && U16(r.data() + at + 132 + 14) == 3 && U16(r.data() + at + 14) == 0 &&
                word(at + 44, 14) && word(at + 88, 14))
                items = at;
        // (Moves' data: 12 bytes each; Pound (power 40, Normal, 100%, 35 PP) and Karate Chop (50,
        // Fighting, 100%, 25 PP) first.)
        {
            const Bytes pound = { 0, 40, 0, 100, 35 };
            for (size_t at = Find(r, pound); at != std::string::npos && battleMoves == std::string::npos; at = Find(r, pound, at + 1))
                if (at >= 12 && Range(r.size(), at + 12, 5) && r[at + 13] == 50 && r[at + 14] == 1 && r[at + 15] == 100 && r[at + 16] == 25) battleMoves = at - 12;
        }
        // (Pictures: tables of {pointer, 0x800, species} from species 0; palettes {pointer, tag, 0},
        // the normal ones tagged with the species, the shiny ones with it plus a fixed amount. Hacks
        // keep the old tables and add longer ones: the longest of each is taken.)
        picRun = palRun = shinyRun = 0;
        std::vector<std::pair<size_t, size_t>> shinyTables;
        for (size_t at = 0; Range(r.size(), at, 24); at += 4)
        {
            const uint8_t* e = r.data() + at;
            if (Pointer(r, at) == std::string::npos || Pointer(r, at + 8) == std::string::npos) continue;
            if (U16(e + 4) == 0x800 && U16(e + 6) == 0 && U16(e + 12) == 0x800 && U16(e + 14) == 1)
            {
                size_t n = 0;
                while (Range(r.size(), at + n * 8, 8) && U16(e + n * 8 + 4) == 0x800 && U16(e + n * 8 + 6) == n && Pointer(r, at + n * 8) != std::string::npos) n++;
                if (n > picRun) { picRun = n; frontPics = at; }
                at += n * 8 - 4;
                continue;
            }
            if (U16(e + 6) || U16(e + 14) || U16(e + 12) != U16(e + 4) + 1) continue;
            const size_t p0 = Pointer(r, at);
            if (r[p0] != 0x10) continue;
            const unsigned t0 = U16(e + 4);
            size_t n = 0;
            while (Range(r.size(), at + n * 8, 8) && U16(e + n * 8 + 4) == t0 + n && !U16(e + n * 8 + 6) && Pointer(r, at + n * 8) != std::string::npos) n++;
            if (n < 100) continue;
            if (t0 == 0) { if (n > palRun) { palRun = n; palettes = at; } }
            else shinyTables.push_back({ at, n });
            at += n * 8 - 4;
        }
        // (The shiny palettes' tags start past the normal ones' (500 in these games).)
        for (const auto& t : shinyTables)
            if (t.second > shinyRun && t.second + 8 >= palRun && U16(r.data() + t.first + 4) >= palRun) { shinyRun = t.second; shinyPalettes = t.first; }
        // (The pictures only when their tables agree: a hack that added Pokemon has longer tables of
        // its own (Radical Red, Unbound), and a picture with another's palette is worse than none.)
        if (picRun != palRun) { frontPics = palettes = shinyPalettes = std::string::npos; picRun = palRun = shinyRun = 0; }
        pictureCount = (int)std::min(picRun, palRun);
        if (pooled) UsePools(r); // (the pictures and places again, over the ones found by looking)
        if (stats == std::string::npos) FindOwn(r);
    }
    // (A word of the games' text, at least two letters, ended by 0xFF within len.)
    static bool Word(const Bytes& r, size_t at, size_t len)
    {
        int letters = 0;
        for (size_t i = 0; i < len && at + i < r.size(); i++)
        {
            const uint8_t c = r[at + i];
            if (c == 0xFF) return letters >= 2;
            if ((c >= 0xBB && c <= 0xEE) || (c >= 0xA1 && c <= 0xAA) || c == 0 || c == 0xAD || c == 0xAE || c == 0xB4 || c == 0xB5 || c == 0xB6 || c == 0x1B || c == 0xB8 || Accented(c)) { letters++; continue; }
            return false;
        }
        return letters >= 2;
    }
    // (The games' text of an ASCII word: letters, digits and spaces; "e'" for e acute.)
    static Bytes Encode(const char* t)
    {
        Bytes out;
        for (const char* c = t; *c; c++)
        {
            if (*c >= 'A' && *c <= 'Z') out.push_back((uint8_t)(0xBB + *c - 'A'));
            else if (*c >= 'a' && *c <= 'z') out.push_back((uint8_t)(0xD5 + *c - 'a'));
            else if (*c >= '0' && *c <= '9') out.push_back((uint8_t)(0xA1 + *c - '0'));
            else if (*c == '\'' ) out.back() = 0x1B; // (e')
            else out.push_back(0);
        }
        out.push_back(0xFF);
        return out;
    }
    // The tables of a hack whose records are shaped its own way, each found by what its first records hold:
    // - the species' records by the Kanto starters' types (Grass and Poison three times, Fire, Fire, Fire and Flying,
    //   Water three times) in a record of 24, 36, 44 or 64 bytes (the hack's own fields at their own places: old
    //   pokeemerald-expansion's (Inclement Emerald, Elite Redux), R.O.W.E.'s with its stats a u16 each); a closed
    //   game of its own Pokemon (Monomyth) by 400 records of plausible stats beside its names;
    // - the moves' names by Pound and Karate Chop a record apart, their data by Pound's 40 power, Normal, 100%, 35 PP
    //   and Karate Chop's 50, Fighting, 100%, 25 a record later; the items' by Master Ball's and Ultra Ball's names (or
    //   Poke Ball's and Great Ball's), in the record or pointed to from it; the moves by level by Bulbasaur's, Tackle
    //   and Growl, as {move, level} words.
    void FindOwn(const Bytes& r)
    {
        // R.O.W.E. keeps a table of its own symbols for its online play ({name pointer, address}): its base stats there.
        size_t symbols = std::string::npos;
        {
            const char* key = "gBaseStats";
            const Bytes k(key, key + strlen(key) + 1);
            const size_t at = Find(r, k);
            if (at != std::string::npos)
            {
                uint8_t p[4]; const uint32_t v = 0x08000000u + (uint32_t)at; memcpy(p, &v, 4);
                symbols = Find(r, Bytes(p, p + 4), 0, 4);
            }
        }
        auto symbol = [&](const char* name) -> size_t {
            if (symbols == std::string::npos) return std::string::npos;
            for (size_t at = symbols; at >= 8 && Pointer(r, at - 8) != std::string::npos; at -= 8) symbols = at - 8;
            for (size_t at = symbols; Pointer(r, at) != std::string::npos && Range(r.size(), at, 8); at += 8)
            {
                const size_t n = Pointer(r, at);
                if (!strncmp((const char*)r.data() + n, name, strlen(name) + 1)) return Pointer(r, at + 4);
            }
            return std::string::npos;
        };
        // The species' records.
        const uint8_t sig[9][2] = { { 12, 3 }, { 12, 3 }, { 12, 3 }, { 10, 10 }, { 10, 10 }, { 10, 2 }, { 11, 11 }, { 11, 11 }, { 11, 11 } };
        size_t table = std::string::npos, stride = 0; int to = 0;
        for (size_t st : { 36, 44, 64 })
            for (int typeAt : { 6, 12 })
                for (size_t at = Find(r, Bytes(sig[0], sig[0] + 2)); at != std::string::npos && table == std::string::npos; at = Find(r, Bytes(sig[0], sig[0] + 2), at + 1))
                {
                    if (at < (size_t)typeAt + st) continue;
                    const size_t base = at - typeAt;
                    bool ok = Range(r.size(), base, st * 9);
                    for (int k = 0; k < 9 && ok; k++) ok = r[base + k * st + typeAt] == sig[k][0] && r[base + k * st + typeAt + 1] == sig[k][1];
                    // (Its stats as stats: six bytes of 10 to 255, or six words of them.)
                    for (int i = 0; i < 6 && ok; i++) ok = typeAt == 12 ? U16(r.data() + base + i * 2) >= 10 && U16(r.data() + base + i * 2) < 256 : r[base + i] >= 10;
                    if (ok) { table = base - st; stride = st; to = typeAt; }
                }
        Shape s;
        s.own = true;
        if (table != std::string::npos && to == 12)
        {
            // (R.O.W.E.: u16 stats, the types at 12, its National Pokedex number a u16 at 0x2C.)
            s.u16Stats = true; s.typeOff = 12; s.catchOff = 14; s.expOff = 16; s.expU16 = true; s.evOff = 18; s.genderOff = 24; s.hatchOff = 25;
            s.friendOff = 26; s.growthOff = 27; s.eggOff = 28; s.abilityOff[0] = 30; s.abilityOff[1] = 32; s.abilityOff[2] = 36; s.abilityU16 = true;
            s.typeMode = 2; // (0x2C: the species its family starts from, not its number)
        }
        else if (table != std::string::npos)
        {
            // (Old pokeemerald-expansion's struct BaseStats: the experience yield and the EV yields words at 10 and
            // 12, the gender at 18, three abilities words from 24, the Fairy type at 18.)
            s.expOff = 10; s.expU16 = true; s.evOff = 12; s.genderOff = 18; s.hatchOff = 19; s.friendOff = 20; s.growthOff = 21; s.eggOff = 22;
            s.abilityOff[0] = 24; s.abilityOff[1] = 26; s.abilityOff[2] = 28; s.abilityU16 = true; s.typeMode = 2;
        }
        else if (names != std::string::npos)
        {
            // (A game of its own Pokemon (Monomyth): 24-byte records near the names, 400 of them plausible, record 0 empty.)
            auto plausible = [&](size_t at) {
                const uint8_t* e = r.data() + at;
                for (int i = 0; i < 6; i++) if (!e[i]) return false;
                return e[6] <= 18 && e[7] <= 18 && e[8] && e[12] <= 5;
            };
            const size_t from = names > 0x40000 ? names - 0x40000 : 0;
            for (size_t at = from & ~(size_t)3; at < names + 0x40000 && Range(r.size(), at, 24 * 402) && table == std::string::npos; at += 4)
            {
                bool ok = true;
                for (int i = 0; i < 24 && ok; i++) ok = !r[at + i];
                for (int k = 1; k <= 400 && ok; k++) ok = plausible(at + k * 24);
                if (ok) { table = at; stride = 24; }
            }
            if (table == std::string::npos) return;
            s.catchOff = 8; s.genderOff = 9; s.hatchOff = 10; s.friendOff = 11; s.growthOff = 12; s.expOff = -1; s.evOff = -1; s.eggOff = -1;
            s.abilityOff[0] = s.abilityOff[1] = -1; s.typeMode = 2;
        }
        else return;
        if (const size_t b = symbol("gBaseStats"); b != std::string::npos && b == table) table = b;
        // (How many: records until the types stop being types, and no more than the names.)
        int n = 0;
        for (int i = 1; i < 4096 && Range(r.size(), table + i * stride, stride); i++)
        {
            const uint8_t* e = r.data() + table + i * stride + s.typeOff;
            if (e[0] > 23 || e[1] > 23) break;
            n = i;
        }
        // Names: a table of pointers to them where there is one (Elite Redux), else the scanned table.
        for (const char* bulba : { "Bulbasaur", "BULBASAUR" })
        {
            const Bytes b = Encode(bulba), ivy = Encode(bulba[1] == 'u' ? "Ivysaur" : "IVYSAUR");
            for (size_t at = Find(r, b); at != std::string::npos && !s.namePtrs; at = Find(r, b, at + 1))
            {
                uint8_t q[4]; const uint32_t v = 0x08000000u + (uint32_t)at; memcpy(q, &v, 4);
                for (size_t p = Find(r, Bytes(q, q + 4), 0, 4); p != std::string::npos; p = Find(r, Bytes(q, q + 4), p + 4, 4))
                {
                    const size_t next = Pointer(r, p + 4);
                    if (p >= 4 && Pointer(r, p - 4) != std::string::npos && next != std::string::npos && Range(r.size(), next, ivy.size()) && !memcmp(r.data() + next, ivy.data(), ivy.size()))
                        { names = p - 4; s.namePtrs = true; break; }
                }
            }
        }
        // Abilities: records that point to their names where there are such (Elite Redux's: a name pointer first, Stench
        // then Drizzle a record later).
        {
            const Bytes a = Encode("Stench"), b = Encode("Drizzle"), c = Encode("Speed Boost");
            auto is = [&](size_t p, const Bytes& t) { return p != std::string::npos && Range(r.size(), p, t.size()) && !memcmp(r.data() + p, t.data(), t.size()); };
            for (size_t at = Find(r, a); at != std::string::npos && !s.abilityPtrs; at = Find(r, a, at + 1))
            {
                uint8_t q[4]; const uint32_t v = 0x08000000u + (uint32_t)at; memcpy(q, &v, 4);
                for (size_t p = Find(r, Bytes(q, q + 4), 0, 4); p != std::string::npos && !s.abilityPtrs; p = Find(r, Bytes(q, q + 4), p + 4, 4))
                    for (size_t st = 4; st <= 256; st += 4)
                        if (p >= st && is(Pointer(r, p + st), b) && is(Pointer(r, p + 2 * st), c))
                            { abilities = p - st; s.abilityPtrs = true; s.abilityStride = st; break; }
            }
        }
        Bytes abilitiesFirst(7, 0xAE); abilitiesFirst.push_back(0xFF);
        {
            int best = 0; size_t bestAt = std::string::npos, bestLen = abilityLength;
            for (size_t at = Find(r, abilitiesFirst); at != std::string::npos; at = Find(r, abilitiesFirst, at + 1))
                for (size_t len : { 13, 17, 21 })
                {
                    int run = 0;
                    while (run < 1024 && Word(r, at + (run + 1) * len, len)) run++;
                    if (run > best) { best = run; bestAt = at; bestLen = len; }
                }
            if (best > 50 && !s.abilityPtrs) { abilities = bestAt; abilityLength = bestLen; }
        }
        // Move names: "Pound" then "Karate Chop" a record later, 13, 17 or 19 bytes (the longest such table).
        {
            size_t found = std::string::npos, foundLen = 0;
            for (const char* pound : { "Pound", "POUND" })
            {
                const Bytes p = Encode(pound), k = Encode(pound[1] == 'o' ? "Karate Chop" : "KARATE CHOP");
                for (size_t at = Find(r, p); at != std::string::npos; at = Find(r, p, at + 1))
                    for (size_t len : { 19, 17, 13 })
                        if (len > foundLen && at >= len && Range(r.size(), at + len, k.size()) && !memcmp(r.data() + at + len, k.data(), k.size()))
                            { found = at - len; foundLen = len; }
            }
            if (found != std::string::npos) { moveNames = found; s.moveNameLength = foundLen; }
        }
        // Moves' data: the first ten moves' types a record apart (Pound and Karate Chop Normal and Fighting, Fire, Ice
        // and Thunder Punch Fire, Ice and Electric, Scratch Normal; a hack may change the others'), found by the three
        // punches; the PP after Pound's 100% accuracy.
        {
            const int types[10] = { 0, 1, -1, -1, -1, -1, 10, 15, 13, 0 };
            battleMoves = std::string::npos;
            for (size_t at = 0; at < r.size() && battleMoves == std::string::npos; at++)
            {
                if (r[at] != 10) continue;
                for (size_t st = 12; st <= 64 && battleMoves == std::string::npos; st += 2)
                {
                    if (at < 7 * st || !Range(r.size(), at + 3 * st, 1) || r[at + st] != 15 || r[at + 2 * st] != 13) continue;
                    const size_t first = at - 6 * st;
                    bool ok = true;
                    for (int k = 0; k < 10 && ok; k++) ok = types[k] < 0 || r[first + k * st] == types[k];
                    if (!ok) continue;
                    for (int acc : { 1, 2 })
                        if (r[first + acc] == 100) { battleMoves = first - st; s.battleStride = st; s.battlePpOff = acc + 1; break; }
                }
            }
        }
        // Items: Ultra Ball item 2 and Great Ball item 3 a record apart, in the record or pointed to from it.
        for (const auto& pair : { std::make_pair("Ultra Ball", "Great Ball"), std::make_pair("ULTRA BALL", "GREAT BALL") })
        {
            const Bytes a = Encode(pair.first), b = Encode(pair.second);
            for (size_t at = Find(r, a); at != std::string::npos && items == std::string::npos; at = Find(r, a, at + 1))
            {
                for (size_t st = 24; st <= 64 && items == std::string::npos; st += 2)
                    if (at >= 2 * st && Range(r.size(), at + st, b.size()) && !memcmp(r.data() + at + st, b.data(), b.size()))
                        { items = at - 2 * st; s.itemStride = st; s.itemNameOff = 0; s.itemNameLength = std::min<size_t>(st, 20); s.itemNamePtr = false; }
                uint8_t q[4]; const uint32_t v = 0x08000000u + (uint32_t)at; memcpy(q, &v, 4);
                for (size_t p = Find(r, Bytes(q, q + 4), 0, 4); p != std::string::npos && items == std::string::npos; p = Find(r, Bytes(q, q + 4), p + 4, 4))
                    for (size_t st = 24; st <= 64; st += 4)
                    {
                        const size_t nb = Pointer(r, p + st);
                        if (p >= 2 * st && nb != std::string::npos && Range(r.size(), nb, b.size()) && !memcmp(r.data() + nb, b.data(), b.size()))
                            { items = p - 2 * st; s.itemStride = st; s.itemNameOff = 0; s.itemNameLength = 20; s.itemNamePtr = true; break; }
                    }
            }
            if (items != std::string::npos) break;
        }
        if (items != std::string::npos)
        {
            int misses = 0, last = 0;
            for (int i = 1; i < 3000 && misses < 8; i++)
            {
                const size_t at = items + (size_t)i * s.itemStride;
                if (!Range(r.size(), at, s.itemStride)) break;
                const size_t t = s.itemNamePtr ? Pointer(r, at) : at;
                if (t != std::string::npos && Word(r, t, s.itemNameLength)) { last = i; misses = 0; }
                else misses++;
            }
            s.itemCount = last + 1;
        }
        // Moves by level: {u16 move, u16 level} lists, 0xFFFF (or none) last, a pointer to each from a table a species.
        // Where the game has Kanto's Pokemon: the run of 300 or more pointers whose Bulbasaur learns Tackle and Growl,
        // Charmander Scratch and Pikachu Thunder Shock. Otherwise (Monomyth) the table whose first Pokemon's list
        // starts with Tackle and Growl.
        {
            auto learns = [&](size_t table, int species, std::initializer_list<int> want) {
                const size_t at = Pointer(r, table + (size_t)species * 4);
                if (at == std::string::npos) return false;
                for (int m : want)
                {
                    bool has = false;
                    for (size_t i = 0; i < 120 && Range(r.size(), at + i * 4, 4); i++)
                    {
                        const unsigned v = U16(r.data() + at + i * 4);
                        if (v == 0xFFFF || !v) break;
                        has = has || (int)v == m;
                    }
                    if (!has) return false;
                }
                return true;
            };
            sh.namePtrs = s.namePtrs; // (Name() reads the shape's names already)
            const bool kanto = Name(1).find("ulbasaur") != std::string::npos || Name(1).find("ULBASAUR") != std::string::npos;
            levelMoves = std::string::npos;
            if (kanto)
            {
                size_t run = 0;
                for (size_t at = 0; Range(r.size(), at, 4) && levelMoves == std::string::npos; at += 4)
                {
                    if (Pointer(r, at) != std::string::npos) { run++; continue; }
                    if (run >= 300)
                        // (The table may follow other pointers in the run: any start in it, entry 0 its first word or the
                        // word before.)
                        for (size_t t = at - run * 4 - 4; t + 300 * 4 <= at && levelMoves == std::string::npos; t += 4)
                            if (learns(t, 1, { 33, 45 }) && learns(t, 4, { 10 }) && learns(t, 25, { 84 })) { levelMoves = t; s.learnFormat = 1; }
                    run = 0;
                }
            }
            else
                for (const Bytes& start : { Bytes{ 0x21, 0x00, 0x01, 0x00, 0x2D, 0x00 } })
                    for (size_t at = Find(r, start, 0, 2); at != std::string::npos && levelMoves == std::string::npos; at = Find(r, start, at + 2, 2))
                    {
                        uint8_t ptr[4]; const uint32_t v = 0x08000000u + (uint32_t)at; memcpy(ptr, &v, 4);
                        for (size_t q = Find(r, Bytes(ptr, ptr + 4), 0, 4); q != std::string::npos; q = Find(r, Bytes(ptr, ptr + 4), q + 4, 4))
                            if (q >= 4 && Pointer(r, q - 4) != std::string::npos && Pointer(r, q + 4) != std::string::npos) { levelMoves = q - 4; s.learnFormat = 1; break; }
                    }
        }
        // (The machines aren't read in these shapes; egg moves where they're these games' list.)
        tmMoves = tmBits = std::string::npos;
        stats = table; statsStride = stride; count = n; sh = s;
        if (pictureCount > n + 1) pictureCount = n + 1;
        if (wildHeaders == std::string::npos) FindWild(r); // (now with the expansion's shapes too)
        if (mapGroups == std::string::npos) FindPlaces(r); // (the places' names even without their wild Pokemon)
        if (const size_t m = symbol("gRegionMapEntries"); m != std::string::npos && Range(r.size(), m, 8 * 40))
            { mapsecNames = m; mapsecPointers = false; mapsecFirst = 0; } // (R.O.W.E.'s own, by its symbol table)
        // (Species and their numbers: R.O.W.E. keeps each's; the expansion's old ones number them in order, the
        // forms after them, each a number its base species' (the same name) has.)
        ownDex.assign(n + 1, 0);
        if (s.natDexOff < 0)
        {
            std::map<std::string, int> first;
            for (int i = 1; i <= n; i++)
            {
                const std::string nm = Name(i);
                const auto f = first.find(nm);
                ownDex[i] = f != first.end() && Readable(nm) ? ownDex[f->second] : i;
                if (f == first.end()) first[nm] = i;
            }
        }
    }
    std::vector<int> ownDex;
    static bool Readable(const std::string& t) { return t.size() >= 2 && t.find('?') == std::string::npos; }
    // Species from a National Pokedex number, and back (the games' own order from Treecko on).
    int Species(int dex) const
    {
        if (x.on || sh.own)
        {
            if (sh.own && sh.natDexOff < 0) return dex > 0 && dex < (int)ownDex.size() && ownDex[dex] == dex ? dex : 0;
            for (int sp = 1; sp <= count; sp++) if (Dex(sp) == dex) return sp;
            return 0;
        }
        if (national == std::string::npos) return dex;
        for (int sp = 1; sp <= count; sp++) if (Range(rom->size(), national + (sp - 1) * 2, 2) && U16(rom->data() + national + (sp - 1) * 2) == dex) return sp;
        return 0;
    }
    int Dex(int species) const
    {
        if (x.on) { const uint8_t* e = Entry(species); return e && x.natDexOff >= 0 ? U16(e + x.natDexOff) : 0; }
        if (sh.own)
        {
            if (species <= 0 || species > count) return 0;
            if (sh.natDexOff >= 0) return U16(rom->data() + stats + (size_t)species * statsStride + sh.natDexOff);
            return species < (int)ownDex.size() ? ownDex[species] : 0;
        }
        if (national == std::string::npos || species <= 0) return species;
        return Range(rom->size(), national + (species - 1) * 2, 2) ? U16(rom->data() + national + (species - 1) * 2) : 0;
    }
    std::string Name(int species) const
    {
        if (x.on) { const uint8_t* e = Entry(species); return e && x.nameOff >= 0 ? Text(e + x.nameOff, 13) : std::string(); }
        if (sh.namePtrs)
        {
            const size_t p = names != std::string::npos && species >= 0 && species <= 8192 ? Pointer(*rom, names + (size_t)species * 4) : std::string::npos;
            return p == std::string::npos ? std::string() : Text(rom->data() + p, std::min<size_t>(20, rom->size() - p));
        }
        if (names == std::string::npos || species < 0 || !Range(rom->size(), names + species * nameLength, nameLength)) return {};
        return Text(rom->data() + names + species * nameLength, nameLength);
    }
    // Where to find: gWildMonHeaders (pokeemerald): {map group, map number, 2 bytes, then pointers
    // to the land, water, rock smash and fishing encounters (or none)}, 0xFF 0xFF at the end; each
    // encounters {rate, 3 bytes, pointer to {min level, max level, species} slots}. The maps' place
    // names: gMapGroups (pointers to each group's pointers to map headers, the region map section at
    // 0x14) and the sections' names (Ruby, Sapphire and Emerald: {x, y, w, h, name pointer} a
    // section; FireRed and LeafGreen: a name pointer a section from KANTO's, 0x58).
    // (An encounter table: {rate, 3 bytes, a pointer to its slots of {lowest level, highest, species}}. A hack may
    // leave a slot empty (species 0) or give its levels the wrong way round (Heart and Soul): taken as they are.)
    bool Info(const Bytes& r, size_t info, int slots) const
    {
        if (info == std::string::npos || !Range(r.size(), info, 8) || r[info] > 100) return false;
        const size_t mons = Pointer(r, info + 4);
        if (mons == std::string::npos || !Range(r.size(), mons, slots * 4)) return false;
        const int most = x.on ? x.species : 2048;
        for (int i = 0; i < slots; i++)
        {
            const uint8_t* m = r.data() + mons + i * 4;
            if (!U16(m + 2)) continue;
            if (!m[0] || m[0] > 100 || !m[1] || m[1] > 100 || U16(m + 2) >= most) return false;
        }
        return true;
    }
    static constexpr int kWildSlots[5] = { 12, 5, 5, 10, 3 };
    // (A header with no tables at all is one too, among the others (Emerald Legacy's).)
    bool WildEntry(const Bytes& r, size_t at) const { int tables = 0; return WildEntry(r, at, tables); }
    bool WildEntry(const Bytes& r, size_t at, int& tables) const
    {
        tables = 0;
        if (!Range(r.size(), at, WildStride()) || r[at] == 0xFF || r[at + 2] || r[at + 3]) return false;
        for (int t = 0; t < wildTables * wildSets; t++)
        {
            const size_t q = at + 4 + (size_t)t * 4;
            if (!U32(r.data() + q)) continue;
            if (Pointer(r, q) == std::string::npos) return false;
            const int k = t % wildTables;
            if (k < 4 && !Info(r, Pointer(r, q), kWildSlots[k])) return false;
            tables += k < 4;
        }
        return true;
    }
    static bool Header(const Bytes& r, size_t h)
    {
        return h != std::string::npos && Range(r.size(), h, 0x1C) && Pointer(r, h) != std::string::npos && Pointer(r, h + 4) != std::string::npos && r[h + 0x14] < 0xD5;
    }
    void FindWild(const Bytes& r)
    {
        // (Each shape a hack may have, the longest run of headers with 0xFF after it; a run is only skipped past
        // once it has been counted, so one that starts inside another's isn't lost.)
        int best = 0, bestTables = 4, bestSets = 1;
        static const int shapes[][2] = { { 4, 1 }, { 5, 1 }, { 5, 4 }, { 5, 3 }, { 5, 2 } };
        for (const auto& shape : shapes)
        {
            if (!x.on && !sh.own && shape[0] == 5) break; // (only the expansion (and hacks of their own) has five)
            wildTables = shape[0]; wildSets = shape[1];
            const size_t stride = WildStride();
            for (size_t at = 0; Range(r.size(), at, stride * 20); at += 4)
            {
                const uint32_t w = U32(r.data() + at + 4);
                if (w && (w < 0x08000000u || w >= 0x08000000u + r.size())) continue;
                int tables = 0;
                if (!WildEntry(r, at, tables) || !tables) continue;
                int n = 0, withTables = 0;
                while (WildEntry(r, at + n * stride, tables)) { n++; withTables += tables > 0; }
                if (withTables > best && Range(r.size(), at + n * stride, 2) && r[at + n * stride] == 0xFF) { best = withTables; wildHeaders = at; bestTables = shape[0]; bestSets = shape[1]; }
                if (Range(r.size(), at + n * stride, 2) && r[at + n * stride] == 0xFF) at += n * stride - 4;
            }
        }
        wildTables = bestTables; wildSets = bestSets;
        if (best < 20) { wildHeaders = std::string::npos; return; }
        FindPlaces(r);
    }
    // The maps' groups and their sections' names, for the wild Pokemon's places.
    void FindPlaces(const Bytes& r)
    {
        // (Map groups: pointers to groups whose first map's header looks like one.)
        for (size_t at = 0; Range(r.size(), at, 4 * 20) && mapGroups == std::string::npos; at += 4)
        {
            int n = 0;
            for (; n < 64; n++)
            {
                const size_t group = Pointer(r, at + n * 4);
                if (group == std::string::npos || !Header(r, Pointer(r, group))) break;
            }
            if (n >= 20) mapGroups = at;
        }
        size_t bestAt = std::string::npos;
        // (Section names: Ruby, Sapphire and Emerald's {x, y, w, h, name}; FireRed and LeafGreen's pointers from
        // KANTO's (0x58). Both are tried in every game: Ultra Violet is FireRed with a code of its own, and an
        // expansion built as FireRed (Generation 3 Evolved) keeps Emerald's shape.)
        auto readable = [&](size_t p) {
            if (p == std::string::npos || !Range(r.size(), p, 2)) return false;
            int n = 0;
            while (n < 24 && Range(r.size(), p + n, 1) && r[p + n] != 0xFF) { const uint8_t c = r[p + n]; if (!((c >= 0xBB && c <= 0xEE) || c == 0 || (c >= 0xA1 && c <= 0xAA) || c == 0xAD || c == 0xAE || c == 0xB4 || c == 0xFC || Accented(c))) return false; n++; }
            return n >= 3 && n < 24;
        };
        // (Of the tables of names, the one that names the most of the wild encounters' maps after
        // routes: "ROUTE 101", "ROUTE 1", "PERCORSO 1"; credits and item lists name none.)
        int bestScore = 0;
        bool bestPointers = false;
        for (int shape = 0; shape < 2; shape++)
        {
            const bool pointers = shape == 1;
            for (size_t at = 0; Range(r.size(), at, 8 * 40); at += 4)
            {
                int n = 0;
                if (pointers) { while (n < 200 && readable(Pointer(r, at + n * 4))) n++; }
                else { while (n < 300 && Range(r.size(), at + n * 8, 8) && r[at + n * 8] < 40 && r[at + n * 8 + 1] < 40 && readable(Pointer(r, at + n * 8 + 4))) n++; }
                if (n < 40) continue;
                mapsecNames = at; mapsecPointers = pointers; mapsecFirst = pointers ? 0x58 : 0;
                std::set<std::string> routes;
                for (size_t w = wildHeaders; WildEntry(r, w); w += WildStride())
                {
                    const std::string place = PlaceOf(r[w], r[w + 1]);
                    if (place.find_first_of("0123456789") != std::string::npos && place.find('-') == std::string::npos) routes.insert(place);
                }
                // (No wild tables read (Elite Redux, R.O.W.E.): the table's own names of routes.)
                if (wildHeaders == std::string::npos)
                    for (int i = 0; i < n; i++)
                    {
                        const size_t name = pointers ? Pointer(r, at + i * 4) : Pointer(r, at + i * 8 + 4);
                        const std::string t = name == std::string::npos ? std::string() : Text(r.data() + name, 24);
                        if ((t.rfind("ROUTE ", 0) == 0 || t.rfind("Route ", 0) == 0) && t.find_first_of("0123456789") != std::string::npos) routes.insert(t);
                    }
                if ((int)routes.size() > bestScore) { bestScore = (int)routes.size(); bestAt = at; bestPointers = pointers; }
                at += (pointers ? n * 4 : n * 8) - 4;
            }
        }
        mapsecNames = bestScore >= 3 ? bestAt : std::string::npos;
        mapsecPointers = bestPointers;
        mapsecFirst = bestPointers ? 0x58 : 0;
    }
    // A map section's name (FLY's towns), or "".
    std::string MapsecName(int sec) const
    {
        const Bytes& r = *rom;
        if (mapsecNames == std::string::npos || sec - mapsecFirst < 0) return {};
        const int i = sec - mapsecFirst;
        const size_t name = mapsecPointers ? Pointer(r, mapsecNames + i * 4) : Pointer(r, mapsecNames + i * 8 + 4);
        if (name == std::string::npos) return {};
        return Text(r.data() + name, 32);
    }
    // A map group's table of map headers (gMapGroups[group], its key undone).
    size_t Group(int group) const
    {
        const Bytes& r = *rom;
        if (mapGroups == std::string::npos || group < 0 || !Range(r.size(), mapGroups + group * 4, 4)) return std::string::npos;
        const uint32_t p = U32(r.data() + mapGroups + group * 4) ^ mapGroupsKey;
        return p >= 0x08000000u && p - 0x08000000u < r.size() ? p - 0x08000000u : std::string::npos;
    }
    std::string PlaceOf(int group, int num) const
    {
        const Bytes& r = *rom;
        if (mapGroups == std::string::npos || mapsecNames == std::string::npos) return {};
        const size_t g = Group(group);
        if (g == std::string::npos) return {};
        const size_t h = Pointer(r, g + num * 4);
        if (!Header(r, h)) return {};
        const int sec = r[h + 0x14] - mapsecFirst;
        if (sec < 0) return {};
        const size_t name = mapsecPointers ? Pointer(r, mapsecNames + sec * 4) : Pointer(r, mapsecNames + sec * 8 + 4);
        if (name == std::string::npos) return {};
        return Text(r.data() + name, 32);
    }
    // A map of the region drawn from the game's own table of its places (Ruby, Sapphire and
    // Emerald: each place's square on the region map's grid of 8-pixel tiles): towns, routes and the
    // rest in their own colours, where you are marked. Not the game's map picture: none is copied.
    // FireRed and LeafGreen keep their squares elsewhere: no map (ok false).
    struct RegionPicture { bool ok = false; int w = 0, h = 0; std::vector<uint32_t> px; float markerX = -1, markerY = -1; };
    int MapsecOf(int group, int num) const
    {
        const Bytes& r = *rom;
        if (mapGroups == std::string::npos) return -1;
        const size_t g = Group(group);
        if (g == std::string::npos) return -1;
        const size_t h = Pointer(r, g + num * 4);
        return Header(r, h) ? r[h + 0x14] : -1;
    }
    RegionPicture Region(int group, int num) const
    {
        RegionPicture m;
        const Bytes& r = *rom;
        if (mapsecPointers || mapsecNames == std::string::npos) return m;
        struct Sec { int x, y, w, h; bool route, town; };
        std::vector<Sec> secs;
        int gw = 0, gh = 0;
        for (int i = 0; i < 256 && Range(r.size(), mapsecNames + i * 8, 8); i++)
        {
            const uint8_t* e = r.data() + mapsecNames + i * 8;
            const size_t name = Pointer(r, mapsecNames + i * 8 + 4);
            if (name == std::string::npos || e[0] >= 40 || e[1] >= 40) break;
            const std::string t = Text(r.data() + name, 24);
            const bool route = t.find_first_of("0123456789") != std::string::npos;
            const bool town = !route && (t.find("TOWN") != std::string::npos || t.find("CITY") != std::string::npos || t.find("STADT") != std::string::npos ||
                                         t.find("VILLE") != std::string::npos || t.find("CIUDAD") != std::string::npos || t.find("CITT") != std::string::npos ||
                                         t.find("PUEBLO") != std::string::npos || t.find("BORGO") != std::string::npos || t.find("DORF") != std::string::npos ||
                                         t.find("BOURG") != std::string::npos || t.find("CITE") != std::string::npos);
            secs.push_back({ e[0], e[1], std::max(1, (int)e[2]), std::max(1, (int)e[3]), route, town });
            // (The region's grid: the places with a square on it; the later ones (Kanto's, the
            // secret bases') sit at 0,0.)
            if (e[0] || e[1]) { gw = std::max(gw, e[0] + std::max(1, (int)e[2])); gh = std::max(gh, e[1] + std::max(1, (int)e[3])); }
        }
        if (secs.size() < 40 || gw < 10 || gh < 5) return m;
        const int cell = 8;
        m.w = (gw + 2) * cell; m.h = (gh + 2) * cell;
        m.px.assign((size_t)m.w * m.h, 0xFF1E3A5Fu); // (the sea)
        auto fill = [&](int x, int y, int w, int h, uint32_t c) {
            for (int yy = y; yy < y + h && yy < m.h; yy++)
                for (int xx = x; xx < x + w && xx < m.w; xx++) if (xx >= 0 && yy >= 0) m.px[(size_t)yy * m.w + xx] = c;
        };
        for (int pass = 0; pass < 3; pass++)
            for (const Sec& sec : secs)
            {
                if (!sec.x && !sec.y) continue;
                if ((pass == 0) != sec.route || (pass == 2) != sec.town) continue;
                const uint32_t col = sec.town ? 0xFFE0614Bu : sec.route ? 0xFF7CC08Bu : 0xFFC9B27Au;
                fill((sec.x + 1) * cell + 1, (sec.y + 1) * cell + 1, sec.w * cell - 2, sec.h * cell - 2, col);
            }
        const int here = MapsecOf(group, num);
        if (here >= 0 && here < (int)secs.size() && (secs[here].x || secs[here].y))
        {
            const Sec& sec = secs[here];
            m.markerX = ((sec.x + 1) + sec.w * 0.5f) * cell / m.w;
            m.markerY = ((sec.y + 1) + sec.h * 0.5f) * cell / m.h;
        }
        m.ok = true;
        return m;
    }
    struct Found { std::string place, method; int low = 0, high = 0; };
    // Each wild slot of one header: its species, how (fishing by rod: the old rod's first 2 slots, the good rod's
    // next 3, the super rod's 5), its levels, and (the newer expansion) the times of day it is there, a bit each
    // of morning, day, evening and night; all set when the game has no times.
    struct Slot { int species = 0; const char* method = ""; int low = 0, high = 0, times = 0, kind = 0; };
    std::vector<Slot> Slots(size_t at) const
    {
        static const char* const methods[5] = { "GRASS", "SURF", "ROCK SMASH", "FISHING", "HIDDEN" };
        std::vector<Slot> out;
        const Bytes& r = *rom;
        int present[5] = {}; // (the times of day each kind has a table for)
        for (int set = 0; set < wildSets; set++)
            for (int k = 0; k < wildTables; k++)
            {
                const size_t info = Pointer(r, at + 4 + ((size_t)set * wildTables + k) * 4);
                const size_t mons = info == std::string::npos ? std::string::npos : Pointer(r, info + 4);
                if (mons == std::string::npos || !Range(r.size(), mons, kWildSlots[k] * 4)) continue;
                present[k] |= 1 << set;
                for (int i = 0; i < kWildSlots[k]; i++)
                {
                    const uint8_t* m = r.data() + mons + i * 4;
                    if (!U16(m + 2)) continue;
                    const char* how = k == 3 ? (i < 2 ? "OLD ROD" : i < 5 ? "GOOD ROD" : "SUPER ROD") : methods[k];
                    const int lo = std::min(m[0], m[1]), hi = std::max(m[0], m[1]), bit = wildSets > 1 ? 1 << set : 15;
                    auto same = std::find_if(out.begin(), out.end(), [&](const Slot& s) { return s.species == U16(m + 2) && !strcmp(s.method, how); });
                    if (same == out.end()) out.push_back({ U16(m + 2), how, lo, hi, bit, k });
                    else { same->low = std::min(same->low, lo); same->high = std::max(same->high, hi); same->times |= bit; }
                }
            }
        // (A time with no table of that kind takes another's (the game falls back to one): a Pokemon in every time
        // that has one, or a kind with one time only, is there all day.)
        for (Slot& sl : out)
            if (wildSets > 1 && (sl.times == present[sl.kind] || !(present[sl.kind] & (present[sl.kind] - 1)))) sl.times = 15;
        return out;
    }
    // A method with the times of day it holds at ("GRASS (NIGHT)"), as it is when it holds all day.
    static std::string When(const char* method, int times)
    {
        if ((times & 15) == 15) return method;
        static const char* const names[4] = { "MORNING", "DAY", "EVENING", "NIGHT" };
        std::string t;
        for (int i = 0; i < 4; i++) if (times >> i & 1) t += (t.empty() ? "" : ", ") + std::string(names[i]);
        return std::string(method) + " (" + t + ")";
    }
    std::vector<Found> WhereToFind(int species) const
    {
        std::vector<Found> out;
        if (wildHeaders == std::string::npos || species <= 0) return out;
        const Bytes& r = *rom;
        for (size_t at = wildHeaders; WildEntry(r, at); at += WildStride())
        {
            std::string place = PlaceOf(r[at], r[at + 1]);
            if (place.empty()) { char b[24]; snprintf(b, sizeof b, "MAP %d.%d", r[at], r[at + 1]); place = b; }
            for (const Slot& sl : Slots(at))
            {
                if (sl.species != species) continue;
                const std::string how = When(sl.method, sl.times);
                bool merged = false;
                for (auto& f : out)
                    if (f.place == place && f.method == how) { f.low = std::min(f.low, sl.low); f.high = std::max(f.high, sl.high); merged = true; break; }
                if (!merged) out.push_back({ place, how, sl.low, sl.high });
            }
        }
        return out;
    }
    std::string ItemName(int id) const
    {
        if (x.on)
        {
            if (!x.itemStride || id <= 0 || id >= x.items) return {};
            const size_t at = x.itemsInfo + (size_t)id * x.itemStride + x.itemNameOff;
            const size_t p = x.itemNamePtr ? Pointer(*rom, at) : at;
            return p == std::string::npos || !Range(rom->size(), p, 1) ? std::string() : Text(rom->data() + p, std::min<size_t>(20, rom->size() - p));
        }
        if (sh.own)
        {
            if (items == std::string::npos || id <= 0 || id >= sh.itemCount || !Range(rom->size(), items + (size_t)id * sh.itemStride, 4)) return {};
            const size_t at = items + (size_t)id * sh.itemStride + sh.itemNameOff;
            const size_t p = sh.itemNamePtr ? Pointer(*rom, at) : at;
            return p == std::string::npos || !Range(rom->size(), p, 1) ? std::string() : Text(rom->data() + p, std::min<size_t>(sh.itemNameLength, rom->size() - p));
        }
        if (items == std::string::npos || id <= 0 || !Range(rom->size(), items + id * 44, 44)) return {};
        return Text(rom->data() + items + id * 44, 14);
    }
    // A move's type (Generation 3's numbers) and its PP, or -1.
    int MoveType(int id) const
    {
        if (x.on) return x.moveTypeOff >= 0 && id > 0 && id < x.moves ? Type(U16(rom->data() + x.movesInfo + (size_t)id * x.moveStride + x.moveTypeOff) & 31) : -1;
        if (sh.own) return battleMoves != std::string::npos && id > 0 && id < 2048 && Range(rom->size(), battleMoves + id * sh.battleStride, 4) ? Type(rom->data()[battleMoves + id * sh.battleStride]) : -1;
        return battleMoves != std::string::npos && id > 0 && Range(rom->size(), battleMoves + id * 12, 12) ? Type(rom->data()[battleMoves + id * 12 + 2]) : -1; }
    int MovePp(int id) const
    {
        if (x.on) return x.movePpOff >= 0 && id > 0 && id < x.moves ? rom->data()[x.movesInfo + (size_t)id * x.moveStride + x.movePpOff] : 0;
        if (sh.own) return battleMoves != std::string::npos && id > 0 && id < 2048 && Range(rom->size(), battleMoves + id * sh.battleStride, 4) ? rom->data()[battleMoves + id * sh.battleStride + sh.battlePpOff] : 0;
        return battleMoves != std::string::npos && id > 0 && Range(rom->size(), battleMoves + id * 12, 12) ? rom->data()[battleMoves + id * 12 + 4] : 0; }
    // A move's battle data, from the original games' gBattleMoves (12 bytes a move): its effect (Generation 3's
    // EFFECT_ numbers), power, type, accuracy (0: never misses), the effect's chance and priority. Not known (ok
    // false) for a game that keeps its moves its own way (an expansion hack), nor past the original 354 moves.
    struct MoveBattleData { bool ok = false; int effect = 0, power = 0, type = -1, accuracy = 0, chance = 0, priority = 0; };
    MoveBattleData MoveBattle(int id) const
    {
        MoveBattleData b;
        if (x.on || sh.own || battleMoves == std::string::npos || id <= 0 || id > 354 || !Range(rom->size(), battleMoves + (size_t)id * 12, 12)) return b;
        const uint8_t* p = rom->data() + battleMoves + (size_t)id * 12;
        b.ok = true; b.effect = p[0]; b.power = p[1]; b.type = Type(p[2]); b.accuracy = p[3]; b.chance = p[5]; b.priority = (int8_t)p[7];
        return b;
    }
    // The wild Pokemon of one map: species, how, levels.
    struct Wild { int species = 0; std::string method; int low = 0, high = 0; };
    std::vector<Wild> WildAt(int group, int num) const
    {
        std::vector<Wild> out;
        if (wildHeaders == std::string::npos) return out;
        const Bytes& r = *rom;
        for (size_t at = wildHeaders; WildEntry(r, at); at += WildStride())
        {
            if (r[at] != group || r[at + 1] != num) continue;
            for (const Slot& sl : Slots(at))
            {
                const std::string how = When(sl.method, sl.times);
                bool merged = false;
                for (auto& w : out)
                    if (w.species == sl.species && w.method == how) { w.low = std::min(w.low, sl.low); w.high = std::max(w.high, sl.high); merged = true; break; }
                if (!merged) out.push_back({ sl.species, how, sl.low, sl.high });
            }
        }
        return out;
    }
    std::string Move(int id) const
    {
        if (x.on)
        {
            if (!x.moveStride || id <= 0 || id >= x.moves) return {};
            const size_t p = Pointer(*rom, x.movesInfo + (size_t)id * x.moveStride);
            return p == std::string::npos ? std::string() : Text(rom->data() + p, std::min<size_t>(20, rom->size() - p));
        }
        const size_t len = sh.moveNameLength;
        if (moveNames == std::string::npos || id <= 0 || !Range(rom->size(), moveNames + id * len, len)) return {};
        return Text(rom->data() + moveNames + id * len, len);
    }
    struct Learned { int move = 0, level = 0; };
    std::vector<Learned> LevelMoves(int species) const
    {
        std::vector<Learned> out;
        if (x.on)
        {
            const uint8_t* e = Entry(species);
            const size_t at = e && x.levelUpOff >= 0 ? Pointer(*rom, e - rom->data() + x.levelUpOff) : std::string::npos;
            for (size_t i = 0; at != std::string::npos && i < 100 && Range(rom->size(), at + i * 4, 4); i++)
            {
                const unsigned m = U16(rom->data() + at + i * 4);
                if (m == 0xFFFF || !m) break;
                out.push_back({ (int)m, U16(rom->data() + at + i * 4 + 2) });
            }
            return out;
        }
        if (levelMoves == std::string::npos || species <= 0) return out;
        const size_t at = Pointer(*rom, levelMoves + species * 4);
        if (at == std::string::npos) return out;
        if (sh.learnFormat == 1)
        {
            for (size_t i = 0; i < 100 && Range(rom->size(), at + i * 4, 4); i++)
            {
                const unsigned m = U16(rom->data() + at + i * 4);
                if (m == 0xFFFF || !m) break;
                out.push_back({ (int)m, U16(rom->data() + at + i * 4 + 2) });
            }
            return out;
        }
        if (cfru)
        {
            for (size_t i = 0; i < 100 && Range(rom->size(), at + i * 3, 3); i++)
            {
                const uint8_t* e = rom->data() + at + i * 3;
                if (e[2] == 0xFF || !U16(e)) break;
                out.push_back({ U16(e), e[2] });
            }
            return out;
        }
        for (size_t i = 0; i < 64 && Range(rom->size(), at + i * 2, 2); i++)
        {
            const unsigned v = U16(rom->data() + at + i * 2);
            if (v == 0xFFFF) break;
            out.push_back({ (int)(v & 511), (int)(v >> 9) });
        }
        return out;
    }
    // TM01-50 then HM01-08: the machine moves this species can use, label and move.
    std::vector<std::pair<std::string, int>> Machines(int species) const
    {
        std::vector<std::pair<std::string, int>> out;
        if (x.on)
        {
            // (The expansion lists what a Pokemon can be taught, its TMs and its tutors' moves together: each named
            // by the TM or HM that teaches it, "TUTOR" otherwise.)
            for (int m : MoveList(species, x.teachableOff))
            {
                const auto t = std::find_if(teachers.begin(), teachers.end(), [&](const std::pair<std::string, int>& k) { return k.second == m; });
                out.push_back({ t != teachers.end() ? t->first : std::string("TUTOR"), m });
            }
            auto rank = [](const std::string& l) { return l == "TUTOR" ? 3000000 : (l[0] == 'H' ? 1000000 : 0) + atoi(l.c_str() + 2); };
            std::stable_sort(out.begin(), out.end(), [&](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { return rank(a.first) < rank(b.first); });
            return out;
        }
        if (tmMoves == std::string::npos || tmBits == std::string::npos || species <= 0 || !Range(rom->size(), tmBits + species * 8, 8)) return out;
        const uint8_t* b = rom->data() + tmBits + species * 8;
        for (int i = 0; i < 58; i++)
            if (b[i / 8] >> (i % 8) & 1)
            {
                char label[8];
                snprintf(label, sizeof label, i < 50 ? "TM%02d" : "HM%02d", i < 50 ? i + 1 : i - 49);
                out.push_back({ label, U16(rom->data() + tmMoves + i * 2) });
            }
        return out;
    }
    // A u16 list of moves (an expansion entry's pointer at off), 0xFFFF last.
    std::vector<int> MoveList(int species, int off) const
    {
        std::vector<int> out;
        const uint8_t* e = Entry(species);
        const size_t at = e && off >= 0 ? Pointer(*rom, e - rom->data() + off) : std::string::npos;
        for (size_t i = 0; at != std::string::npos && i < 400 && Range(rom->size(), at + i * 2, 2); i++)
        {
            const unsigned m = U16(rom->data() + at + i * 2);
            if (m == 0xFFFF || !m) break;
            out.push_back((int)m);
        }
        return out;
    }
    std::vector<int> EggMoves(int species) const
    {
        std::vector<int> out;
        if (x.on) return MoveList(species, x.eggOff);
        if (eggMoves == std::string::npos || species <= 0) return out;
        bool mine = false;
        for (size_t at = eggMoves; Range(rom->size(), at, 2) && at < eggMoves + (pooled ? 0x40000 : 0x4000); at += 2)
        {
            const unsigned v = U16(rom->data() + at);
            if (v == 0xFFFF) break;
            if (v > 20000) { if (mine) break; mine = (int)v - 20000 == species; continue; }
            if (mine) out.push_back((int)v);
        }
        return out;
    }
    std::string Ability(int id) const
    {
        if (x.on)
        {
            if (!x.abilityStride || id <= 0 || id >= x.abilities || !Range(rom->size(), x.abilitiesInfo + (size_t)id * x.abilityStride, 17)) return {};
            return Text(rom->data() + x.abilitiesInfo + (size_t)id * x.abilityStride, 17);
        }
        if (sh.abilityPtrs)
        {
            const size_t p = abilities != std::string::npos && id > 0 && id < 4096 ? Pointer(*rom, abilities + (size_t)id * sh.abilityStride) : std::string::npos;
            return p == std::string::npos ? std::string() : Text(rom->data() + p, std::min<size_t>(20, rom->size() - p));
        }
        if (abilities == std::string::npos || id <= 0 || !Range(rom->size(), abilities + id * abilityLength, abilityLength)) return {};
        return Text(rom->data() + abilities + id * abilityLength, abilityLength);
    }
    Stats Get(int species) const
    {
        Stats s;
        if (!Ready() || species <= 0 || species > count) return s;
        const uint8_t* p = rom->data() + stats + species * statsStride;
        if (x.on)
        {
            // (gSpeciesInfo's own layout: the experience yield a word at 10, the EV yield after it, the abilities three
            // words (the hidden one last); types counted from none.)
            const int order[6] = { 0, 1, 2, 4, 5, 3 };
            const unsigned evs = U16(p + 0x0C);
            for (int i = 0; i < 6; i++) { s.base[i] = p[order[i]]; s.ev[i] = evs >> (order[i] * 2) & 3; }
            s.type1 = Type(p[6]); s.type2 = Type(p[7]); s.catchRate = p[8]; s.baseExp = U16(p + 0x0A);
            s.gender = p[0x12]; s.hatchCycles = p[0x13]; s.friendship = p[0x14]; s.growth = p[0x15];
            s.eggGroup1 = p[0x16]; s.eggGroup2 = p[0x17];
            if (x.abilitiesOff >= 0)
            {
                s.ability1 = U16(p + x.abilitiesOff); s.ability2 = U16(p + x.abilitiesOff + 2);
                const int h = U16(p + x.abilitiesOff + 4);
                if (h && h != s.ability1 && h != s.ability2) s.hidden = h;
            }
            s.ok = p[0] > 0;
            return s;
        }
        if (sh.own)
        {
            const int order[6] = { 0, 1, 2, 4, 5, 3 };
            for (int i = 0; i < 6; i++) s.base[i] = sh.u16Stats ? U16(p + order[i] * 2) : p[order[i]];
            if (sh.evOff >= 0) { const unsigned evs = U16(p + sh.evOff); for (int i = 0; i < 6; i++) s.ev[i] = evs >> (order[i] * 2) & 3; }
            s.type1 = Type(p[sh.typeOff]); s.type2 = Type(p[sh.typeOff + 1]);
            s.catchRate = p[sh.catchOff];
            s.baseExp = sh.expOff < 0 ? 0 : sh.expU16 ? U16(p + sh.expOff) : p[sh.expOff];
            s.gender = p[sh.genderOff]; s.hatchCycles = p[sh.hatchOff]; s.friendship = p[sh.friendOff]; s.growth = p[sh.growthOff];
            if (sh.eggOff >= 0) { s.eggGroup1 = p[sh.eggOff]; s.eggGroup2 = p[sh.eggOff + 1]; }
            auto ability = [&](int k) { const int o = sh.abilityOff[k]; return o < 0 ? 0 : sh.abilityU16 ? (int)U16(p + o) : (int)p[o]; };
            s.ability1 = ability(0); s.ability2 = ability(1);
            const int h = ability(2);
            if (h && h != s.ability1 && h != s.ability2) s.hidden = h;
            if (s.ability2 == s.ability1) s.ability2 = 0;
            s.ok = s.base[0] > 0;
            return s;
        }
        const int order[6] = { 0, 1, 2, 4, 5, 3 }; // (kept as HP, Atk, Def, Speed, SpA, SpD)
        const unsigned evs = U16(p + 10);
        for (int i = 0; i < 6; i++) { s.base[i] = p[order[i]]; s.ev[i] = evs >> (order[i] * 2) & 3; }
        s.type1 = Type(p[6]); s.type2 = Type(p[7]); s.catchRate = p[8]; s.baseExp = p[9];
        s.gender = p[16]; s.hatchCycles = p[17]; s.friendship = p[18]; s.growth = p[19];
        s.eggGroup1 = p[20]; s.eggGroup2 = p[21]; s.ability1 = p[22]; s.ability2 = p[23];
        // (The Complete FireRed Upgrade, Radical Red's and Unbound's engine: the hidden ability at 26.)
        if (p[26] && p[26] != s.ability1 && p[26] != s.ability2 && Readable(Ability(p[26]))) s.hidden = p[26];
        s.ok = true;
        return s;
    }
    // The battle picture, 64 x 64: 4bpp tiles, 8 to a row; colour 0 clear.
    Picture Front(int species, bool shiny) const
    {
        Picture img;
        if (x.on)
        {
            const uint8_t* e = Entry(species);
            if (!e || x.frontOff < 0) return img;
            const size_t base = e - rom->data();
            return Draw(Unpack(Pointer(*rom, base + x.frontOff), false), Unpack(Pointer(*rom, base + (shiny ? x.shinyOff : x.paletteOff)), true));
        }
        if (frontPics == std::string::npos || species <= 0 || species >= pictureCount) return img;
        const size_t pal = shiny ? shinyPalettes : palettes;
        if (pal == std::string::npos || (shiny && species >= (int)shinyRun)) return img;
        const size_t picAt = Pointer(*rom, frontPics + species * 8), palAt = Pointer(*rom, pal + species * 8);
        if (picAt == std::string::npos || palAt == std::string::npos) return img;
        return Draw(Lz77(*rom, picAt), Lz77(*rom, palAt));
    }
    Picture Draw(const Bytes& tiles, const Bytes& colours) const
    {
        Picture img;
        if (tiles.size() < 0x800 || colours.size() < 32) return img;
        img.width = img.height = 64;
        img.pixels.assign(64 * 64, 0);
        for (int t = 0; t < 64; t++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                {
                    int v = tiles[t * 32 + y * 4 + x / 2];
                    v = x & 1 ? v >> 4 : v & 15;
                    if (!v) continue;
                    const unsigned c = U16(colours.data() + v * 2);
                    img.pixels[((t / 8) * 8 + y) * 64 + (t % 8) * 8 + x] =
                        0xFF000000u | ((c & 31) * 255 / 31) << 16 | (((c >> 5) & 31) * 255 / 31) << 8 | ((c >> 10) & 31) * 255 / 31;
                }
        // (A picture that fills its edges is not one of these 64 x 64 battle pictures (a hack that
        // lays them out otherwise): none rather than a wrong one.)
        int edge = 0, opaque = 0;
        for (int i = 0; i < 64; i++)
            for (int k : { 0, 1, 62, 63 })
            {
                edge += 2;
                opaque += (img.pixels[k * 64 + i] >> 24 ? 1 : 0) + (img.pixels[i * 64 + k] >> 24 ? 1 : 0);
            }
        if (opaque * 100 > edge * 30) return Picture();
        return img;
    }
};
}
