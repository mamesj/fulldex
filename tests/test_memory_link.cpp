// MEMORY LINK (memory_link.h): a Black or White save's memories linked into a Black 2 or White 2
// save, on synthetic saves laid out as the games lay them out (two copies, block CRCs, the table of
// them and the footer; the key data at 0x7E800).
#include "memory_link.h"
#include <cassert>
#include <cstdio>
using namespace MemoryLink;

static const size_t SaveSize = 0x80000;
static void Put(Bytes& s, size_t o, uint32_t v, int n) { for (int i = 0; i < n; i++) s[o + i] = (unsigned char)(v >> (8 * i)); }
// A block in a copy: its data, then its count and CRC, and the CRC in that copy's table.
static void Block(Bytes& s, const Layout& L, uint32_t base, const BlockAt& b, const Bytes& data)
{
    std::copy(data.begin(), data.end(), s.begin() + base + b.offset);
    const uint16_t crc = Crc16(&s[base + b.offset], b.size);
    Put(s, base + b.offset + b.size, 1, 2);
    Put(s, base + b.offset + b.size + 2, crc, 2);
    Put(s, base + L.size - 0x10 - L.crcs + 2 * b.index, crc, 2);
}
// The footer that ends a copy: its count, size, the magic, the CRC of the table.
static void Seal(Bytes& s, const Layout& L, uint32_t base, uint32_t count)
{
    const size_t f = base + L.size - 0x10;
    Put(s, f, count, 4);
    Put(s, f + 4, L.size, 4);
    Put(s, f + 8, SaveMagic, 4);
    Put(s, f + 14, Crc16(&s[f - L.crcs], L.crcs), 2);
}
// The musical's props: the first byte, or all 100.
static Bytes Props(unsigned char first, bool all = false)
{
    Bytes p(0x20, 0);
    p[0] = first;
    if (all) for (int k = 0; k < 100; k++) p[k / 8] |= (unsigned char)(1 << (k % 8));
    return p;
}
static const BlockAt Trainer = { 27, 0x19400, 0x68 }, Musical = { 42, 0x1F700, 0x2A4 }, Events = { 45, 0x20100, 0x3EC }, Word = { 52, 0x21200, 0xEC }, Records = { 66, 0x23B00, 0x16C };
struct Story
{
    uint32_t id = 0x05E531A6;
    std::u16string name = u"Hilbert";
    int gender = 0, version = 21;
    std::vector<int> flags = { 2400 };      // event flags set
    uint32_t word = 0x01;                    // block 52's second word
    uint16_t vars[3] = { 0, 0, 0 };          // 0x4030, 0x408E, 0x40BC
    uint16_t shows = 5, points = 1234;
    Bytes props = Props(0x07);               // the musical's props (Black 2's own start as Props(0x03))
    unsigned char record = 0;                // every byte of the six records
};
static Bytes BlackWhiteSave(const Story& st, bool both = true)
{
    Bytes s(SaveSize, 0xFF);
    for (uint32_t base : { 0u, BlackWhite.backup })
    {
        if (base && !both) continue;
        for (size_t i = 0; i < BlackWhite.crcs; i++) s[base + BlackWhite.size - 0x10 - BlackWhite.crcs + i] = 0;
        Bytes t(Trainer.size, 0);
        for (int i = 0; i < 8; i++) Put(t, 4 + 2 * i, i < (int)st.name.size() ? st.name[i] : 0xFFFF, 2);
        Put(t, 4 + 0x10, st.id, 4);
        t[4 + 0x1B] = (unsigned char)st.version;
        t[4 + 0x1D] = (unsigned char)st.gender;
        Block(s, BlackWhite, base, Trainer, t);
        Bytes m(Musical.size, 0);
        Put(m, 0x252, st.shows, 2);
        Put(m, 0x256, st.points, 2);
        std::copy(st.props.begin(), st.props.end(), m.begin() + 0x258);
        Block(s, BlackWhite, base, Musical, m);
        Bytes e(Events.size, 0);
        for (int f : st.flags) e[0x27C + f / 8] |= (unsigned char)(1 << (f % 8));
        Put(e, 2 * (0x4030 - 0x4000), st.vars[0], 2);
        Put(e, 2 * (0x408E - 0x4000), st.vars[1], 2);
        Put(e, 2 * (0x40BC - 0x4000), st.vars[2], 2);
        Block(s, BlackWhite, base, Events, e);
        Bytes w(Word.size, 0);
        Put(w, 4, st.word, 4);
        Block(s, BlackWhite, base, Word, w);
        Block(s, BlackWhite, base, Records, Bytes(Records.size, st.record));
        Seal(s, BlackWhite, base, 2);
    }
    return s;
}
// A Black 2 save: its musical block (Black 2's own props, and whether it has downloaded props), and
// key data never written (as on a new card: all 0xFF).
static Bytes Black2Save(const Bytes& props = Props(0x03), bool downloaded = false)
{
    Bytes s(SaveSize, 0xFF);
    for (uint32_t base : { 0u, Black2White2.backup })
    {
        for (size_t i = 0; i < Black2White2.crcs; i++) s[base + Black2White2.size - 0x10 - Black2White2.crcs + i] = 0;
        Bytes m(Musical.size, 0);
        std::copy(props.begin(), props.end(), m.begin() + 0x258);
        m[0x2A0] = downloaded;
        Block(s, Black2White2, base, Musical, m);
        Seal(s, Black2White2, base, 2);
    }
    return s;
}
static const unsigned char* LinkOf(const Bytes& s) { return &s[KeysAt + KeysHeader + LinkIn]; }
// Everything but the key data is as it was.
static bool OnlyKeysChanged(const Bytes& a, const Bytes& b)
{
    for (size_t i = 0; i < a.size(); i++)
        if (a[i] != b[i] && (i < KeysAt || i >= KeysAt + KeysHeader + KeysSize)) return false;
    return true;
}

int main()
{
    // Reading Black: the player, the flags (2400 set: bit 0; 681-685 not set: bits 3-7; the word's
    // bits after), the event work's values, the musical and the records.
    Story st;
    st.flags = { 2400, 2427, 683 };
    st.vars[0] = 7; st.vars[1] = 0; st.vars[2] = 0x1234;
    st.record = 0x5A;
    Source src;
    std::string why;
    assert(ReadBlackWhite(BlackWhiteSave(st), src, why));
    assert(src.name == "Hilbert" && src.version == 21 && std::string(VersionName(src.version)) == "Black");
    assert(U32(&src.data[0]) == st.id && src.data[0x14] == 0 && src.data[0x15] == 21 && src.data[0x16] == 0);
    const unsigned char* m = &src.data[PlayerSize];
    assert(m[0] == (1 | 4 | 8 | 16 | 64 | 128) && m[1] == 0x01);
    assert(U16(m + 2) == 7 && U16(m + 4) == 0 && U16(m + 6) == 0x1234);
    assert(m[8] == 0x07 && U16(m + 0x28) == 1234 && U16(m + 0x2A) == 5);
    assert(m[0x2C] == 0x5A && m[0x2C + 6 * 0x3C - 1] == 0x5A);

    // A name past ASCII comes out as UTF-8.
    Story fr; fr.name = u"Élise"; fr.version = 20; fr.gender = 1;
    assert(ReadBlackWhite(BlackWhiteSave(fr), src, why) && src.name == "\xC3\x89lise" && std::string(VersionName(src.version)) == "White");

    // Not Black or White's: a Black 2 save, a blank file, a save of another version, a block broken in both copies.
    assert(!ReadBlackWhite(Black2Save(), src, why) && !why.empty());
    assert(!ReadBlackWhite(Bytes(SaveSize, 0xFF), src, why) && !ReadBlackWhite(Bytes(), src, why));
    Story b2; b2.version = 23;
    assert(!ReadBlackWhite(BlackWhiteSave(b2), src, why));
    Bytes broken = BlackWhiteSave(Story());
    broken[Events.offset + 5] ^= 1; broken[BlackWhite.backup + Events.offset + 5] ^= 1;
    assert(!ReadBlackWhite(broken, src, why));
    // A block broken in the main copy comes from the backup, as the game takes it.
    Bytes mainBroken = BlackWhiteSave(Story());
    mainBroken[Trainer.offset + 4] ^= 0xFF;
    assert(ReadBlackWhite(mainBroken, src, why) && src.name == "Hilbert");
    // The newer copy wins: the backup saved later with another name, the main copy as before.
    Story later; later.name = u"Hilda";
    Bytes older = BlackWhiteSave(Story()), newer = BlackWhiteSave(later);
    std::copy(newer.begin() + BlackWhite.backup, newer.begin() + BlackWhite.backup + BlackWhite.size, older.begin() + BlackWhite.backup);
    Seal(older, BlackWhite, BlackWhite.backup, 3);
    assert(ReadBlackWhite(older, src, why) && src.name == "Hilda");
    // Only the main copy saved (the backup blank): read from it.
    assert(ReadBlackWhite(BlackWhiteSave(Story(), false), src, why) && src.name == "Hilbert");

    // Linking into Black 2 with no key data yet: the key data is made as the game makes it (keys
    // clear, the seed given, the count going round from 0xFFFFFFFF to 0), and the memories the
    // story opens are open: flag 0 (2400) opens the third, sixth and eighth, flag 8 the first, the
    // musical's player the seventh (the same five Black 2 lists in its Memory List).
    assert(ReadBlackWhite(BlackWhiteSave(Story()), src, why));
    const Bytes blank = Black2Save();
    Bytes save = blank;
    Result r = Link(save, src, false, 0xC0FFEE);
    assert(r.outcome == Linked && r.newMemories && KeysRight(save) && OnlyKeysChanged(blank, save));
    assert(U32(&save[KeysAt]) == KeysMagic && U32(&save[KeysAt + 4]) == 0 && U16(&save[KeysAt + 10]) == KeysSize);
    for (size_t i = 0; i < KeysSeed; i++) assert(save[KeysAt + KeysHeader + i] == 0);
    assert(U32(&save[KeysAt + KeysHeader + KeysSeed]) == 0xC0FFEE);
    assert(Unlocked(LinkOf(save)) == 0xE5);
    for (int i = 0; i < 2; i++) assert(LinkOf(save)[PlayerSize * i + 0x16] == 1 && memcmp(LinkOf(save) + PlayerSize * i, src.data.data(), 0x16) == 0);
    assert(LinkOf(save)[2 * PlayerSize + 0x16] == 0); // (the records: all zero, as Black 2 has them)
    std::vector<Player> with = LinkedWith(save);
    assert(with.size() == 1 && with[0].name == "Hilbert" && with[0].version == 21);
    assert(LinkedWith(blank).empty());

    // The same save again: nothing to change, nothing written.
    Bytes again = save;
    r = Link(again, src, false, 1);
    assert(r.outcome == Unchanged && !r.newMemories && again == save);

    // More of the same story (a flag that opens the second memory): linked, the count goes on, the
    // seed stays.
    Story more; more.word = 0x01 | 0x20;
    Source src2;
    assert(ReadBlackWhite(BlackWhiteSave(more), src2, why));
    Bytes saveMore = save;
    r = Link(saveMore, src2, false, 2);
    assert(r.outcome == Linked && r.newMemories && Unlocked(LinkOf(saveMore)) == 0xE7);
    assert(U32(&saveMore[KeysAt + 4]) == 1 && U32(&saveMore[KeysAt + KeysHeader + KeysSeed]) == 0xC0FFEE);

    // Another player's save that changes the events asks first; told to, it links, and the events'
    // player is the new one (the musical's stays Hilbert: this save has not been to the musical).
    Story other; other.id = 0x11112222; other.name = u"Hilda"; other.version = 20; other.gender = 1; other.flags = { 2400, 2401 }; other.shows = 0;
    Source src3;
    assert(ReadBlackWhite(BlackWhiteSave(other), src3, why));
    Bytes asked = save;
    r = Link(asked, src3, false, 3);
    assert(r.outcome == AsksFirst && asked == save);
    r = Link(asked, src3, true, 3);
    assert(r.outcome == Linked && !r.newMemories);
    assert(LinkOf(asked)[PlayerSize * 0 + 0x14] == 1 && LinkOf(asked)[PlayerSize * 0 + 0x15] == 20);
    assert(NameOf(LinkOf(asked) + 4) == "Hilda" && NameOf(LinkOf(asked) + PlayerSize + 4) == "Hilbert");
    with = LinkedWith(asked);
    assert(with.size() == 2 && with[0].name == "Hilda" && with[1].name == "Hilbert");
    // Another player's save that changes nothing does not ask: there is nothing to replace.
    Story same = other; same.flags = { 2400 }; same.word = 0;
    Source src4;
    assert(ReadBlackWhite(BlackWhiteSave(same), src4, why));
    Bytes unchanged = save;
    assert(Link(unchanged, src4, false, 4).outcome == Unchanged && unchanged == save);
    // A value the event work changes where Black 2 has one already is taken without asking (only a
    // value Black 2 does not have yet counts as a change); a value of 0 for the second one is not.
    Story vars = same; vars.vars[0] = 9; vars.vars[1] = 0;
    Bytes withVar = save;
    Put(withVar, KeysAt + KeysHeader + LinkIn + LinkMemories + 2, 4, 2);
    Put(withVar, KeysAt + KeysHeader + LinkIn + LinkMemories + 4, 6, 2);
    Put(withVar, KeysAt + 8, Crc16(&withVar[KeysAt + KeysHeader], KeysSize), 2);
    Source src5;
    assert(ReadBlackWhite(BlackWhiteSave(vars), src5, why));
    r = Link(withVar, src5, false, 5);
    assert(r.outcome == Linked && U16(LinkOf(withVar) + LinkMemories + 2) == 9 && U16(LinkOf(withVar) + LinkMemories + 4) == 6);
    assert(NameOf(LinkOf(withVar) + 4) == "Hilbert");

    // The musical: not taken when its props are Black 2's own (unless Black 2 has all 100), nor
    // when Black 2 has props from a download; taken when they differ (above).
    Story musical; musical.flags = {}; musical.word = 0; musical.props = Props(0x03);
    Source src6;
    assert(ReadBlackWhite(BlackWhiteSave(musical), src6, why));
    Bytes sameProps = Black2Save(Props(0x03));
    assert(Link(sameProps, src6, false, 6).outcome == Linked && LinkOf(sameProps)[PlayerSize + 0x16] == 0);
    musical.props = Props(0, true);
    assert(ReadBlackWhite(BlackWhiteSave(musical), src6, why));
    Bytes allProps = Black2Save(Props(0, true));
    assert(Link(allProps, src6, false, 6).outcome == Linked && LinkOf(allProps)[PlayerSize + 0x16] == 1 && U16(LinkOf(allProps) + LinkMemories + 0x2A) == 5);
    Bytes downloaded = Black2Save(Props(0x03), true);
    assert(Link(downloaded, src6, false, 6).outcome == Linked && LinkOf(downloaded)[PlayerSize + 0x16] == 0);

    // Broken key data is made again (a new seed), its count going on from the one there.
    Bytes brokenKeys = save;
    brokenKeys[KeysAt + KeysHeader + 3] ^= 1;
    assert(!KeysRight(brokenKeys) && LinkedWith(brokenKeys).empty());
    r = Link(brokenKeys, src, false, 77);
    assert(r.outcome == Linked && KeysRight(brokenKeys) && U32(&brokenKeys[KeysAt + 4]) == 1 && U32(&brokenKeys[KeysAt + KeysHeader + KeysSeed]) == 77);

    // Not a Black 2 save (a Black one, a blank file): nothing changes.
    Bytes notB2 = BlackWhiteSave(Story()), was = notB2;
    assert(Link(notB2, src, false, 8).outcome == Failed && notB2 == was);
    Bytes empty;
    assert(Link(empty, src, false, 8).outcome == Failed && empty.empty());
    puts("PASS: reading Black and White saves, linking, asking first, the musical, broken key data");
    return 0;
}
