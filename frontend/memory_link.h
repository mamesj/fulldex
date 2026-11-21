#pragma once
// MEMORY LINK (Black 2 and White 2): the Unova Link's Memory Link, made between two saves here.
// In the games a Black or White DS sends its save over the wireless, and Black 2 or White 2 keeps
// what it says of the first story (who played it, what they did, their musical) in its key data,
// beside its save at the end of the card, where it opens the memories and changes what people say.
// Here the Black or White save is read from its file and the key data written as the game writes
// it, so the game finds the link when it starts. Only the key data changes; nothing else in either
// save does. (As the pokebw2 decompilation's wb_save_convert.c, memory_link_savedata.c and
// save_outside.c have it.)
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace MemoryLink
{
using Bytes = std::vector<unsigned char>;

inline uint16_t U16(const unsigned char* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t U32(const unsigned char* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
inline void Put16(unsigned char* p, uint16_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
inline void Put32(unsigned char* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i)); }

// The CRC the games keep of each part of a save (CRC-16/CCITT: 0x1021, from 0xFFFF).
inline uint16_t Crc16(const unsigned char* p, size_t n)
{
    uint16_t c = 0xFFFF;
    while (n--)
    {
        c ^= (uint16_t)(*p++ << 8);
        for (int k = 0; k < 8; k++) c = (uint16_t)(c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

// A Gen 5 save is two copies, the second a backup. Each is its blocks (a count and a CRC after
// each), the blocks' CRCs, then a footer: the copy's count, its size, the magic and the CRC of the
// blocks' CRCs.
struct Layout { uint32_t backup, size, crcs; };
const Layout BlackWhite = { 0x24000, 0x23F9C, 0x8C }, Black2White2 = { 0x26000, 0x25FA4, 0x94 };
const uint32_t SaveMagic = 0x31053527;
struct BlockAt { int index; uint32_t offset, size; }; // a block: its number, where it is in a copy, its size before the count and CRC

inline bool FooterRight(const Bytes& s, uint32_t base, const Layout& L, uint32_t& count)
{
    if (s.size() < (size_t)base + L.size) return false;
    const unsigned char* f = &s[base + L.size - 0x10];
    count = U32(f);
    return U32(f + 4) == L.size && U32(f + 8) == SaveMagic && U16(f + 14) == Crc16(f - L.crcs, L.crcs);
}
// Where blocks are as the game loads the save: the newer right copy's CRCs say what each block
// holds, and it comes from the main copy if that one has it, else from the backup (the older
// copy's CRCs if the newer ones do not fit them all). False: not this game's save, or broken.
inline bool FindBlocks(const Bytes& s, const Layout& L, const std::vector<BlockAt>& want, std::vector<size_t>& at)
{
    uint32_t mc = 0, bc = 0;
    const bool m = FooterRight(s, 0, L, mc), b = FooterRight(s, L.backup, L, bc);
    const bool mainFirst = m && (!b || mc >= bc || (mc == 0 && bc == 0xFFFFFFFFu)); // (the count goes round to 0)
    const uint32_t tables[2] = { mainFirst ? 0u : L.backup, mainFirst ? L.backup : 0u };
    for (const uint32_t table : tables)
    {
        if (!(table == 0 ? m : b)) continue;
        at.clear();
        for (const BlockAt& k : want)
        {
            const uint16_t crc = U16(&s[table + L.size - 0x10 - L.crcs + 2 * k.index]);
            for (const uint32_t base : { 0u, L.backup })
                if (U16(&s[base + k.offset + k.size + 2]) == crc && Crc16(&s[base + k.offset], k.size) == crc) { at.push_back(base + k.offset); break; }
        }
        if (at.size() == want.size()) return true;
    }
    return false;
}

// What a Black or White save gives the link: its player (the trainer's id, name, gender and
// version, and whether linked) and the memories (16 flags, 3 of the event work's values, the
// musical's props, points and shows, and six records of the story). The game's MemoryLinkData.
const size_t PlayerSize = 0x18, MemoriesSize = 0x194;
struct Source
{
    Bytes data;          // PlayerSize + MemoriesSize bytes
    std::string name;    // the player's name (UTF-8)
    int version = 0;     // 20 White, 21 Black
};
inline const char* VersionName(int v) { return v == 21 ? "Black" : v == 20 ? "White" : v == 23 ? "Black 2" : v == 22 ? "White 2" : "Black or White"; }
// A name as the games keep it (eight UTF-16 letters at most, 0xFFFF after the last) in UTF-8.
inline std::string NameOf(const unsigned char* p)
{
    std::string r;
    for (int i = 0; i < 8; i++)
    {
        const uint16_t c = U16(p + 2 * i);
        if (c == 0xFFFF || c == 0) break;
        if (c < 0x80) r += (char)c;
        else if (c < 0x800) { r += (char)(0xC0 | c >> 6); r += (char)(0x80 | (c & 0x3F)); }
        else { r += (char)(0xE0 | c >> 12); r += (char)(0x80 | (c >> 6 & 0x3F)); r += (char)(0x80 | (c & 0x3F)); }
    }
    return r;
}
inline bool ReadBlackWhite(const Bytes& s, Source& out, std::string& why)
{
    // (The trainer, the musical, the event work, a word of flags and the six records.)
    static const std::vector<BlockAt> blocks = { { 27, 0x19400, 0x68 }, { 42, 0x1F700, 0x2A4 }, { 45, 0x20100, 0x3EC }, { 52, 0x21200, 0xEC }, { 66, 0x23B00, 0x16C } };
    std::vector<size_t> at;
    if (!FindBlocks(s, BlackWhite, blocks, at)) { why = "It is not a save Black or White has saved in, or it is damaged."; return false; }
    const unsigned char *trainer = &s[at[0] + 4], *musical = &s[at[1]], *events = &s[at[2]], *word = &s[at[3] + 4], *records = &s[at[4]];
    Bytes d(PlayerSize + MemoriesSize, 0);
    memcpy(&d[0], trainer + 0x10, 4); // id
    memcpy(&d[4], trainer, 16);       // name
    d[0x14] = trainer[0x1D];          // gender
    d[0x15] = trainer[0x1B];          // version
    unsigned char* m = &d[PlayerSize];
    // The flags: three event flags set, five not set (each a bit), then the word's eight bits.
    static const struct { int flag, bit; bool set; } flags[] = { { 2400, 0, true }, { 2401, 1, true }, { 2427, 2, true },
        { 681, 3, false }, { 682, 4, false }, { 683, 5, false }, { 684, 6, false }, { 685, 7, false } };
    for (const auto& f : flags)
        if (((events[0x27C + f.flag / 8] >> (f.flag % 8) & 1) != 0) == f.set) m[f.bit / 8] |= (unsigned char)(1 << (f.bit % 8));
    m[1] = word[0];
    static const int vars[3] = { 0x4030, 0x408E, 0x40BC };
    for (int i = 0; i < 3; i++) memcpy(m + 2 + 2 * i, events + 2 * (vars[i] - 0x4000), 2);
    memcpy(m + 0x08, musical + 0x258, 0x20); // props
    memcpy(m + 0x28, musical + 0x256, 2);    // total points
    memcpy(m + 0x2A, musical + 0x252, 2);    // shows
    memcpy(m + 0x2C, records, 6 * 0x3C);
    out.data = std::move(d);
    out.name = NameOf(&out.data[4]);
    out.version = out.data[0x15];
    if (out.version != 20 && out.version != 21) { why = "It is a " + std::string(VersionName(out.version)) + " save, not Black or White's."; return false; }
    return true;
}

// The key data, after the Mystery Gift data and its backup at the end of the card: a header (the
// magic, a count of its saves, the CRC of what follows, its size), the keys (0x38 bytes, the last
// four a seed) and the Memory Link's: three players (one for each part: the events, the musical,
// the records) and the memories.
const size_t KeysAt = 0x7E800, KeysHeader = 12, KeysSize = 0x214, KeysSeed = 0x34, LinkIn = 0x38, LinkMemories = 3 * PlayerSize;
const uint32_t KeysMagic = 0xA10F49AE;
inline bool KeysRight(const Bytes& s)
{
    return s.size() >= KeysAt + KeysHeader + KeysSize && U32(&s[KeysAt]) == KeysMagic && U16(&s[KeysAt + 8]) == Crc16(&s[KeysAt + KeysHeader], KeysSize);
}
// Names compare up to the 0xFFFF after the stored one's last letter.
inline bool SameName(const unsigned char* a, const unsigned char* b)
{
    for (int i = 0; i < 8; i++)
    {
        const uint16_t x = U16(a + 2 * i);
        if (x == 0xFFFF) return U16(b + 2 * i) == 0xFFFF;
        if (x != U16(b + 2 * i)) return false;
    }
    return true;
}
// The game's merge: a bit for each part a save changes (events 1, musical 2, records 4), and, if
// apply, the change made, with that part's player now this save's. musical: Black 2's MusicalSave.
inline unsigned Merge(unsigned char* link, const unsigned char* data, const unsigned char* musical, bool apply)
{
    unsigned char* dst = link + LinkMemories;
    const unsigned char* src = data + PlayerSize;
    bool changed[3] = {};
    for (int i = 0; i < 16; i++)
        if (src[i / 8] >> (i % 8) & 1)
        {
            if (!(dst[i / 8] >> (i % 8) & 1)) changed[0] = true;
            if (apply) dst[i / 8] |= (unsigned char)(1 << (i % 8));
        }
    for (int i = 0; i < 3; i++)
    {
        const uint16_t v = U16(src + 2 + 2 * i), was = U16(dst + 2 + 2 * i);
        if ((i != 1 || v != 0) && v != was) // (the second one only when the save has it)
        {
            if (was == 0) changed[0] = true;
            if (apply) Put16(dst + 2 + 2 * i, v);
        }
    }
    // The musical, when the Black or White save has been to one and Black 2 has no props from a
    // download: if its props are not Black 2's, or Black 2 already has all 100.
    if (U16(src + 0x2A) != 0 && musical[0x2A0] == 0)
    {
        int props = 0;
        for (int p = 0; p < 100; p++) props += musical[0x258 + p / 8] >> (p % 8) & 1;
        if (memcmp(src + 0x08, musical + 0x258, 0x20) != 0 || props == 100)
        {
            changed[1] = true;
            if (apply) { memcpy(dst + 0x28, src + 0x28, 4); memcpy(dst + 0x08, src + 0x08, 0x20); }
        }
    }
    if (src[0] & 1 && memcmp(src + 0x2C, dst + 0x2C, 6 * 0x3C) != 0)
    {
        if (apply) memcpy(dst + 0x2C, src + 0x2C, 6 * 0x3C);
        changed[2] = true;
    }
    if (apply)
        for (int i = 0; i < 3; i++)
            if (changed[i]) { memcpy(link + PlayerSize * i, data, PlayerSize); link[PlayerSize * i + 0x16] = 1; }
    return (unsigned)changed[0] | (unsigned)changed[1] << 1 | (unsigned)changed[2] << 2;
}
// Whether the save can be merged without asking: nobody linked yet, or each part it changes is
// already this player's (the same id, name, gender and version).
inline bool CanMerge(const unsigned char* link, const unsigned char* data, unsigned changed)
{
    if (!link[0x16] && !link[PlayerSize + 0x16] && !link[2 * PlayerSize + 0x16]) return true;
    for (int i = 0; i < 3; i++)
    {
        const unsigned char* p = link + PlayerSize * i;
        if (changed >> i & 1 && (memcmp(p, data, 4) != 0 || !SameName(p + 4, data + 4) || p[0x14] != data[0x14] || p[0x15] != data[0x15])) return false;
    }
    return true;
}
// The eight memories, a bit each: open by a flag of the memories, or (the seventh) by the musical's player.
inline unsigned Unlocked(const unsigned char* link)
{
    static const int flag[8] = { 8, 13, 0, 15, 12, 0, -1, 0 };
    unsigned r = 0;
    for (int i = 0; i < 8; i++)
        if (flag[i] < 0 ? link[PlayerSize + 0x16] != 0 : (link[LinkMemories + flag[i] / 8] >> (flag[i] % 8) & 1) != 0) r |= 1u << i;
    return r;
}

struct Player { std::string name; int version = 0; };
// Who a Black 2 or White 2 save is linked with, each once (none: not linked).
inline std::vector<Player> LinkedWith(const Bytes& s)
{
    std::vector<Player> r;
    if (!KeysRight(s)) return r;
    const unsigned char* link = &s[KeysAt + KeysHeader + LinkIn];
    for (int i = 0; i < 3; i++)
    {
        const unsigned char* p = link + PlayerSize * i;
        bool seen = false;
        for (int j = 0; j < i; j++)
        {
            const unsigned char* q = link + PlayerSize * j;
            seen = seen || (q[0x16] && memcmp(p, q, 0x16) == 0);
        }
        if (p[0x16] && !seen) r.push_back({ NameOf(p + 4), p[0x15] });
    }
    return r;
}

// A seed for new keys, picked at random as the game picks it (the keys are kept XORed with it).
inline uint32_t NewSeed()
{
    uint32_t s = (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
    try { std::random_device rd; s ^= rd(); } catch (...) {}
    return s;
}

enum Outcome { Failed, AsksFirst, Unchanged, Linked };
struct Result { Outcome outcome = Failed; bool newMemories = false; std::string why; };
// Links a Black or White save into a Black 2 or White 2 save (s, changed only when Linked). As the
// game does: a save that would change a part another player's save gave asks first (AsksFirst;
// replace: link it anyway). seed: the keys' seed if the key data is new (the game picks one at random).
inline Result Link(Bytes& s, const Source& src, bool replace, uint32_t seed)
{
    Result r;
    std::vector<size_t> at;
    if (src.data.size() != PlayerSize + MemoriesSize) { r.why = "The Black or White save was not read."; return r; }
    if (s.size() < KeysAt + KeysHeader + KeysSize || !FindBlocks(s, Black2White2, { { 42, 0x1F700, 0x2A4 } }, at))
    {
        r.why = "It is not a save Black 2 or White 2 has saved in, or it is damaged.";
        return r;
    }
    // The key data as the game loads it: as saved, or new (when never saved or broken): all clear
    // with a new seed, and the count kept going.
    const bool had = KeysRight(s);
    Bytes keys(s.begin() + KeysAt + KeysHeader, s.begin() + KeysAt + KeysHeader + KeysSize);
    if (!had) { std::fill(keys.begin(), keys.end(), 0); Put32(&keys[KeysSeed], seed); }
    unsigned char* link = &keys[LinkIn];
    const unsigned char* data = src.data.data();
    if (!replace && !CanMerge(link, data, Merge(link, data, &s[at[0]], false))) { r.outcome = AsksFirst; return r; }
    const unsigned before = Unlocked(link);
    Merge(link, data, &s[at[0]], true);
    const unsigned after = Unlocked(link);
    r.newMemories = (after & (before ^ after)) != 0;
    if (had && std::equal(keys.begin(), keys.end(), s.begin() + KeysAt + KeysHeader)) { r.outcome = Unchanged; return r; }
    unsigned char* h = &s[KeysAt];
    Put32(h, KeysMagic);
    Put32(h + 4, U32(h + 4) + 1);
    Put16(h + 8, Crc16(keys.data(), KeysSize));
    Put16(h + 10, (uint16_t)KeysSize);
    std::copy(keys.begin(), keys.end(), s.begin() + KeysAt + KeysHeader);
    r.outcome = Linked;
    return r;
}
}
