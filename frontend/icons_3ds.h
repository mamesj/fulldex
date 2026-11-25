#pragma once
// The Hub's Pokemon icons for Kalos and Alola (#650 to #807), which no DS game has: Ultra Sun / Ultra Moon's (or Sun
// / Moon's) own box icons, read from the player's decrypted .3ds. The ROM's first partition (NCCH), its RomFS (IVFC
// level 3), the icons' GARC (LZ11-compressed BFLIMs: 64x32 RGBA5551 in 8x8 Morton tiles, stored on their side) and
// each species' first icon from the game code's table of 16-byte records (the icon index first), found by its start
// (Bulbasaur 1, Ivysaur 2, Venusaur 3, Charmander 5: the Megas sit after their Pokemon). Each icon made 32x32 as the
// DS ones are (its picture shrunk to 4/5 and centred). Nothing found (another 3DS game, an encrypted ROM): no icons.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Icons3ds
{
struct Set
{
    std::vector<uint16_t> index;              // (each species' icon in the GARC, by National Pokedex number)
    std::vector<std::vector<uint8_t>> icons;  // (the GARC's files, still compressed)
};

inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline bool ReadAt(FILE* f, uint64_t off, size_t n, std::vector<uint8_t>& out)
{
#ifdef _WIN32
    if (_fseeki64(f, (long long)off, SEEK_SET)) return false;
#else
    if (fseeko(f, (off_t)off, SEEK_SET)) return false;
#endif
    out.resize(n);
    return n == 0 || fread(out.data(), 1, n, f) == n;
}
inline std::vector<uint8_t> Lz11(const std::vector<uint8_t>& b)
{
    std::vector<uint8_t> out;
    if (b.size() < 4 || b[0] != 0x11) return out;
    size_t size = b[1] | b[2] << 8 | b[3] << 16, i = 4;
    if (!size) { if (b.size() < 8) return out; size = U32(b.data() + 4); i = 8; }
    if (size > (1u << 20)) return {};
    out.reserve(size);
    while (out.size() < size && i < b.size())
    {
        const int flags = b[i++];
        for (int k = 0; k < 8 && out.size() < size; k++)
        {
            if (!(flags & (0x80 >> k))) { if (i >= b.size()) return {}; out.push_back(b[i++]); continue; }
            if (i + 1 >= b.size()) return {};
            size_t len, disp;
            const int t = b[i] >> 4;
            if (t == 0) { if (i + 2 >= b.size()) return {}; len = (((b[i] & 0xF) << 4) | (b[i + 1] >> 4)) + 0x11; disp = (((b[i + 1] & 0xF) << 8) | b[i + 2]) + 1; i += 3; }
            else if (t == 1) { if (i + 3 >= b.size()) return {}; len = (((b[i] & 0xF) << 12) | (b[i + 1] << 4) | (b[i + 2] >> 4)) + 0x111; disp = (((b[i + 2] & 0xF) << 8) | b[i + 3]) + 1; i += 4; }
            else { len = t + 1; disp = (((b[i] & 0xF) << 8) | b[i + 1]) + 1; i += 2; }
            if (disp > out.size()) return {};
            for (size_t n = 0; n < len; n++) out.push_back(out[out.size() - disp]);
        }
    }
    return out;
}

struct RomFs
{
    FILE* f = nullptr;
    uint64_t data = 0;
    std::vector<uint8_t> dirs, files;
    static std::string Name(const std::vector<uint8_t>& b, size_t at)
    {
        if (at + 4 > b.size()) return {};
        const uint32_t n = U32(b.data() + at);
        std::string s;
        for (uint32_t k = 0; k + 1 < n && at + 4 + k + 1 < b.size(); k += 2) s += (char)b[at + 4 + k];
        return s;
    }
    // A file's offset and size by its path ("a/0/6/2").
    bool Find(const std::string& path, uint64_t& off, uint64_t& size) const
    {
        uint32_t d = 0;
        size_t start = 0;
        for (size_t slash; (slash = path.find('/', start)) != std::string::npos; start = slash + 1)
        {
            const std::string part = path.substr(start, slash - start);
            if (d + 12 > dirs.size()) return false;
            uint32_t c = U32(dirs.data() + d + 8);
            while (c != 0xFFFFFFFFu && c + 0x18 <= dirs.size() && Name(dirs, c + 0x14) != part) c = U32(dirs.data() + c + 4);
            if (c == 0xFFFFFFFFu || c + 0x18 > dirs.size()) return false;
            d = c;
        }
        const std::string leaf = path.substr(start);
        if (d + 16 > dirs.size()) return false;
        for (uint32_t c = U32(dirs.data() + d + 12), guard = 0; c != 0xFFFFFFFFu && c + 0x20 <= files.size() && guard < 100000; c = U32(files.data() + c + 4), guard++)
            if (Name(files, c + 0x1C) == leaf)
            {
                off = data + (U32(files.data() + c + 8) | (uint64_t)U32(files.data() + c + 12) << 32);
                size = U32(files.data() + c + 16) | (uint64_t)U32(files.data() + c + 20) << 32;
                return true;
            }
        return false;
    }
};

// A GARC's files (each one's first part).
inline std::vector<std::vector<uint8_t>> Garc(const std::vector<uint8_t>& b)
{
    std::vector<std::vector<uint8_t>> out;
    if (b.size() < 0x1C || memcmp(b.data(), "CRAG", 4)) return out;
    const uint32_t hs = U32(b.data() + 4), dataOff = U32(b.data() + 0x10);
    if (hs + 12 > b.size() || memcmp(b.data() + hs, "OTAF", 4)) return out;
    const uint32_t fatoSize = U32(b.data() + hs + 4), count = U16(b.data() + hs + 8), fb = hs + fatoSize;
    if (hs + 12 + count * 4 > b.size() || fb + 12 > b.size() || memcmp(b.data() + fb, "BTAF", 4)) return out;
    for (uint32_t i = 0; i < count; i++)
    {
        size_t e = fb + 12 + U32(b.data() + hs + 12 + i * 4);
        if (e + 16 > b.size()) return {};
        const uint32_t bits = U32(b.data() + e);
        std::vector<uint8_t> file;
        if (bits & 1)
        {
            const uint32_t st = U32(b.data() + e + 4), len = U32(b.data() + e + 12);
            if ((uint64_t)dataOff + st + len <= b.size()) file.assign(b.begin() + dataOff + st, b.begin() + dataOff + st + len);
        }
        out.push_back(std::move(file));
    }
    return out;
}

inline bool Load(const std::string& path, Set& s)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> h, n, iv, l3, ex;
    bool ok = false;
    do
    {
        if (!ReadAt(f, 0, 0x200, h) || memcmp(h.data() + 0x100, "NCSD", 4)) break;
        const uint64_t p0 = (uint64_t)U32(h.data() + 0x120) * 0x200;
        if (!ReadAt(f, p0, 0x200, n) || memcmp(n.data() + 0x100, "NCCH", 4)) break;
        // (The game code: the ExeFS's ".code", whole; compressed games aren't read.)
        const uint64_t exo = p0 + (uint64_t)U32(n.data() + 0x1A0) * 0x200;
        if (!ReadAt(f, exo, 0x200, ex)) break;
        std::vector<uint8_t> code;
        for (int i = 0; i < 10; i++)
            if (!memcmp(ex.data() + i * 16, ".code\0\0\0", 8))
            {
                const uint32_t off = U32(ex.data() + i * 16 + 8), size = U32(ex.data() + i * 16 + 12);
                if (size < (16u << 20)) ReadAt(f, exo + 0x200 + off, size, code);
            }
        static const uint16_t start[9] = { 1, 2, 3, 5, 6, 7, 10, 11, 12 };
        size_t table = 0;
        for (size_t o = 16; o + 16 * 820 <= code.size() && !table; o += 2)
        {
            if (U16(code.data() + o) != 1) continue;
            bool m = true;
            for (int k = 1; k < 9 && m; k++) m = U16(code.data() + o + 16 * k) == start[k];
            if (m) table = o - 16;
        }
        if (!table) break;
        RomFs fs;
        fs.f = f;
        const uint64_t rfo = p0 + (uint64_t)U32(n.data() + 0x1B0) * 0x200;
        if (!ReadAt(f, rfo, 0x60, iv) || memcmp(iv.data(), "IVFC", 4)) break;
        const uint32_t master = U32(iv.data() + 8), bs = U32(iv.data() + 0x4C);
        if (bs > 20) break;
        const uint64_t l3o = rfo + ((0x60 + master + (1u << bs) - 1) & ~(uint64_t)((1u << bs) - 1));
        if (!ReadAt(f, l3o, 0x28, l3)) break;
        const uint32_t dmo = U32(l3.data() + 0xC), dms = U32(l3.data() + 0x10), fmo = U32(l3.data() + 0x1C), fms = U32(l3.data() + 0x20), fdo = U32(l3.data() + 0x24);
        if (dms > (8u << 20) || fms > (8u << 20) || !ReadAt(f, l3o + dmo, dms, fs.dirs) || !ReadAt(f, l3o + fmo, fms, fs.files)) break;
        fs.data = l3o + fdo;
        // (The icons' GARC: the a/0/x/y with over 900 files whose first Pokemon's is a 64x32 BFLIM.)
        for (int g = 0; g < 100 && s.icons.empty(); g++)
        {
            char p[16];
            snprintf(p, sizeof p, "a/0/%d/%d", g / 10, g % 10);
            uint64_t off = 0, size = 0;
            std::vector<uint8_t> head;
            if (!fs.Find(p, off, size) || size > (16u << 20) || !ReadAt(f, off, 4, head) || memcmp(head.data(), "CRAG", 4)) continue;
            std::vector<uint8_t> all;
            if (!ReadAt(f, off, (size_t)size, all)) continue;
            auto files = Garc(all);
            if (files.size() < 900) continue;
            const auto one = Lz11(files[1]);
            if (one.size() != 4136 || memcmp(one.data() + one.size() - 0x28, "FLIM", 4)) continue;
            s.icons = std::move(files);
        }
        if (s.icons.empty()) break;
        s.index.assign(810, 0);
        for (int sp = 1; sp < 810; sp++)
        {
            const uint16_t i = U16(code.data() + table + (size_t)sp * 16);
            if (i < s.icons.size()) s.index[sp] = i;
        }
        ok = true;
    } while (false);
    fclose(f);
    if (!ok) { s.icons.clear(); s.index.clear(); }
    return ok;
}

// Species' icon as 32x32 ARGB (out), as the DS games' icons come.
inline bool Icon(const Set& s, int species, std::vector<uint32_t>& out)
{
    if (species <= 0 || species >= (int)s.index.size() || !s.index[species]) return false;
    const auto d = Lz11(s.icons[s.index[species]]);
    if (d.size() < 4096) return false;
    // (Stored 32 wide and 64 tall in 8x8 Morton tiles; turned a quarter to the left it is the 64x32 picture.)
    uint32_t pic[64 * 32] = {};
    int x0 = 64, y0 = 32, x1 = -1, y1 = -1;
    for (int i = 0; i < 2048; i++)
    {
        const int tile = i / 64, k = i % 64, tx = (tile % 4) * 8, ty = (tile / 4) * 8;
        int x = 0, y = 0;
        for (int b = 0; b < 3; b++) { x |= ((k >> (2 * b)) & 1) << b; y |= ((k >> (2 * b + 1)) & 1) << b; }
        const int u = tx + x, v = ty + y; // (in the stored 32x64)
        const uint16_t c = U16(d.data() + i * 2);
        if (!(c & 1)) continue;
        const int X = v, Y = 31 - u; // (the picture's pixel)
        const uint32_t r = (c >> 11 & 31) * 255 / 31, g = (c >> 6 & 31) * 255 / 31, bl = (c >> 1 & 31) * 255 / 31;
        pic[Y * 64 + X] = 0xFF000000u | r << 16 | g << 8 | bl;
        x0 = std::min(x0, X); x1 = std::max(x1, X); y0 = std::min(y0, Y); y1 = std::max(y1, Y);
    }
    if (x1 < 0) return false;
    // (Shrunk to 4/5 about the middle of what it shows, so a 40x30 icon fills the 32x32 as the DS ones do.)
    const float cx = (x0 + x1 + 1) * 0.5f, cy = (y0 + y1 + 1) * 0.5f;
    out.assign(32 * 32, 0);
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++)
        {
            const int X = (int)(cx + (i - 16 + 0.5f) * 1.25f), Y = (int)(cy + (j - 16 + 0.5f) * 1.25f);
            if (X >= 0 && X < 64 && Y >= 0 && Y < 32) out[(size_t)j * 32 + i] = pic[Y * 64 + X];
        }
    return true;
}
}
