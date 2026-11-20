#pragma once
// pokeemerald-expansion's "smol" graphics compression (its tools/compresSmol): tANS entropy coding plus LZ-style
// instructions over u16 symbols. The newer expansion hacks (Heart and Soul 2, Wishes of Tomorrow, Generation 3
// Evolved) keep their Pokemon's pictures this way instead of LZ77. Adapted from PokeDaisy by Lidor Itzhari -
// https://github.com/lidor30/pokedaisy (3ds/core/pd_smol.c, commit 79bd00e; GPL 3): the tile-data modes 1 to 6, its
// buffers local rather than static.
#include <cstdint>
#include <cstring>
#include <vector>

namespace GbaSmol
{
struct Tans { uint8_t sym, k, y, mask; };
constexpr int TABLE = 64, LO_MAX = 8192, SYM_MAX = 16384;

inline uint32_t Rd32(const uint8_t* b, size_t avail, size_t off, bool& ok)
{
    if (off + 4 > avail) { ok = false; return 0; }
    return (uint32_t)b[off] | (uint32_t)b[off + 1] << 8 | (uint32_t)b[off + 2] << 16 | (uint32_t)b[off + 3] << 24;
}
// Each of 16 symbols gets freq[s] consecutive slots j = f..2f-1; slot j reads k = 6 - floor(log2 j) bits, y = (j << k) - 64.
inline bool BuildTable(const uint8_t* src, size_t avail, size_t off, Tans* table)
{
    bool ok = true;
    int freqs[16] = {};
    for (int i = 0; i < 3; i++)
    {
        const uint32_t w = Rd32(src, avail, off + 4 * (size_t)i, ok);
        for (int j = 0; j < 5; j++) freqs[i * 5 + j] = (int)(w >> (6 * j) & 0x3F);
        freqs[15] += (int)((w & 0xC0000000u) >> (30 - 2 * i));
    }
    if (!ok) return false;
    int n = 0;
    for (int s = 0; s < 16; s++)
        for (int j = freqs[s]; j < 2 * freqs[s]; j++)
        {
            if (n >= TABLE) return false;
            int k = 0;
            while ((j << k) < TABLE) k++;
            table[n++] = { (uint8_t)s, (uint8_t)k, (uint8_t)((j << k) - TABLE), (uint8_t)((1 << k) - 1) };
        }
    return n == TABLE;
}
// One continuous little-endian u32 bitstream shared by the lo and symbol decodes; the tANS state carries over.
struct Reader { const uint8_t* src; size_t avail, base, word; uint32_t bits; int bitIndex, state; bool ok; };
inline int Next(Reader& r, const Tans* table)
{
    const Tans t = table[r.state];
    r.state = t.y + (int)((r.bits >> r.bitIndex) & t.mask);
    r.bitIndex += t.k;
    if (r.bitIndex >= 32)
    {
        r.word++;
        r.bits = Rd32(r.src, r.avail, r.base + 4 * r.word, r.ok);
        r.bitIndex -= 32;
        if (r.bitIndex) r.state += (int)((r.bits & ((1u << r.bitIndex) - 1)) << (t.k - r.bitIndex));
    }
    return t.sym;
}
// lo holds (length, offset) pairs, 7 bits per byte + continue bit. A nonzero length emits one symbol, then copies
// `length` values from `offset` back (1 = a run); length 0 emits `offset` literal symbols. The u16 count, or -1.
inline size_t Instructions(const uint8_t* lo, size_t loSize, const uint16_t* sym, size_t symSize, size_t outBytes, std::vector<uint16_t>& words)
{
    const size_t cap = outBytes / 2 + 1;
    words.assign(cap + 2, 0);
    size_t n = 0, i = 0, si = 0;
    auto B = [&](size_t at) { return at < loSize ? lo[at] : (uint8_t)0; };
    while (i < loSize && n < cap)
    {
        size_t length, offset;
        if (B(i) & 0x80)
        {
            length = (size_t)(B(i) & 0x7F) | (size_t)B(i + 1) << 7;
            offset = B(i + 2) & 0x7F;
            if (B(i + 2) & 0x80) { offset |= (size_t)B(i + 3) << 7; i += 4; } else i += 3;
        }
        else
        {
            length = B(i) & 0x7F;
            offset = B(i + 1) & 0x7F;
            if (B(i + 1) & 0x80) { offset |= (size_t)B(i + 2) << 7; i += 3; } else i += 2;
        }
        if (length)
        {
            if (si >= symSize || n >= cap) return (size_t)-1;
            words[n++] = sym[si++];
            if (offset == 0 || offset > n) return (size_t)-1;
            for (size_t k = 0; k < length && n < cap; k++) { words[n] = words[n - offset]; n++; }
        }
        else
            for (size_t k = 0; k < offset && n < cap; k++)
            {
                if (si >= symSize) return (size_t)-1;
                words[n++] = sym[si++];
            }
    }
    return n;
}
// Whether src starts with a smol tile-data header (modes 1..6; mode 0 is LZ77, whose id byte is 0x10).
inline bool IsSmol(const uint8_t* src, size_t avail) { return avail >= 8 && (src[0] & 0x0F) >= 1 && (src[0] & 0x0F) <= 6; }
// The decoded bytes, or none if it isn't valid smol data.
inline std::vector<uint8_t> Decode(const uint8_t* src, size_t avail)
{
    std::vector<uint8_t> lo(LO_MAX);
    std::vector<uint16_t> sym(SYM_MAX);
    bool ok = true;
    const uint32_t h0 = Rd32(src, avail, 0, ok), h1 = Rd32(src, avail, 4, ok);
    if (!ok) return {};
    const int mode = (int)(h0 & 0xF);
    const size_t imageSize = h0 >> 4 & 0x3FFF, symSize = h0 >> 18 & 0x3FFF;
    const size_t bitstreamSize = h1 >> 6 & 0x1FFF, loSize = h1 >> 19 & 0x1FFF;
    const size_t outBytes = imageSize * 4;
    if (mode < 1 || mode > 6 || !loSize || !symSize || loSize > (size_t)LO_MAX || outBytes > 8192) return {};
    const size_t data = 8;
    const bool loEncoded = mode >= 4, symEncoded = mode == 2 || mode == 3 || mode >= 5, symDelta = mode == 3 || mode == 6;
    size_t loFreqs = 0, symFreqs = 0, streamWord = 0;
    if (mode == 4) { loFreqs = data; streamWord = 3; }
    else if (mode == 2 || mode == 3) { symFreqs = data; streamWord = 3; }
    else if (mode >= 5) { loFreqs = data; symFreqs = data + 12; streamWord = 6; }
    Reader r = { src, avail, data, streamWord, 0, 0, (int)(h1 & 0x3F), true };
    r.bits = Rd32(src, avail, data + 4 * streamWord, r.ok);
    size_t leftover = data;
    Tans table[TABLE];
    if (loEncoded)
    {
        if (!BuildTable(src, avail, loFreqs, table)) return {};
        for (size_t i = 0; i < loSize; i++)
        {
            const int a = Next(r, table), b = Next(r, table);
            lo[i] = (uint8_t)(a | b << 4);
        }
        leftover += 12;
    }
    if (symEncoded)
    {
        if (!BuildTable(src, avail, symFreqs, table)) return {};
        int cur = 0;
        for (size_t i = 0; i < symSize; i++)
        {
            int v = 0;
            for (int n = 0; n < 4; n++)
            {
                int x = Next(r, table);
                if (symDelta) { cur = (cur + x) & 0xF; x = cur; }
                v |= x << (4 * n);
            }
            sym[i] = (uint16_t)v;
        }
        leftover += 12;
    }
    if (!r.ok) return {};
    if (loEncoded || symEncoded) leftover += 4 * bitstreamSize;
    if (!symEncoded)
    {
        if (leftover + 2 * symSize > avail) return {};
        for (size_t i = 0; i < symSize; i++) sym[i] = (uint16_t)(src[leftover + 2 * i] | src[leftover + 2 * i + 1] << 8);
        leftover += symSize * 2;
    }
    if (!loEncoded)
    {
        if (leftover + loSize > avail) return {};
        memcpy(lo.data(), src + leftover, loSize);
    }
    std::vector<uint16_t> words;
    const size_t n = Instructions(lo.data(), loSize, sym.data(), symSize, outBytes, words);
    if (n == (size_t)-1) return {};
    std::vector<uint8_t> out(outBytes, 0);
    for (size_t w = 0; w < n && w < outBytes / 2; w++)
    {
        out[2 * w] = (uint8_t)words[w];
        out[2 * w + 1] = (uint8_t)(words[w] >> 8);
    }
    return out;
}
}
