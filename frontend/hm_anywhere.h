// HMS WITHOUT TEACHING (off to start): a field move (CUT, SURF, STRENGTH, ROCK SMASH, WATERFALL, ROCK CLIMB,
// WHIRLPOOL, DEFOG ...) can be used when no Pokemon in the party knows it, as long as the game's own badge check
// passes. The first Pokemon in the party uses it.
//
// Diamond, Pearl, Platinum, HeartGold and SoulSilver ask "which party Pokemon knows this move?" through one script
// command (ScrCmd_FindPartySlotWithMove, HeartGold/SoulSilver's ScrCmd_GetPartySlotWithMove). It starts the answer
// at PARTY_SIZE ("none") and keeps it unless a Pokemon has the move:
//     ldr r1, =6 ; ldr r0, [sp] ; ldr r6, =0 ; strh r1, [r0]
// Storing r6 (0, the loop's first slot) there instead makes "none" the first Pokemon. The program's own code in
// main RAM is changed after the game has started (found by those four instructions and their two constants, so
// another release or a ROM hack with other code there is left alone), and changed back when the setting is off.
// Black, White, Black 2 and White 2 check moves another way: not covered.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "NDS.h"

class HmAnywhere
{
public:
    bool on = false;

    // Diamond, Pearl, Platinum, HeartGold, SoulSilver (any language): the games this can change.
    static bool ForGame(const std::string& code)
    {
        if (code.size() < 3) return false;
        const std::string g = code.substr(0, 3);
        return g == "ADA" || g == "APA" || g == "CPU" || g == "IPK" || g == "IPG";
    }
    void Init(const std::string& gameCode) { game = ForGame(gameCode); state = 0; at = 0; frames = 0; }
    bool Supported() const { return game && state != 2; }

    // After every frame (the emulation's thread).
    void DoFrame(melonDS::NDS& nds)
    {
        if (!game) return;
        if (state == 0)
        {
            if (++frames < 120) return; // (the game's main program is in place by then)
            if (!Find(nds)) { state = 2; fprintf(stderr, "[hm] the move check is not where it is in the released games: HMS WITHOUT TEACHING does nothing here\n"); return; }
            state = 1;
            fprintf(stderr, "[hm] the party's move check found at %08X\n", (unsigned)at);
        }
        if (state != 1) return;
        // (Checked every frame and put right: a state loaded, a reset, or the setting changed.)
        const uint8_t want = on ? 0x06 : 0x01;
        if (Byte(nds, at + 6) != want)
        {
            if (!Matches(nds, at, true)) { state = 0; frames = 0; return; } // (not the code we found any more: look again)
            nds.ARM9Write8(at + 6, want);
        }
    }

private:
    bool game = false;
    int state = 0;      // 0 looking, 1 found, 2 not there
    uint32_t at = 0;    // the ldr r1, =6
    unsigned frames = 0;

    static uint8_t Byte(melonDS::NDS& nds, uint32_t addr) { return nds.MainRAM[addr & nds.MainRAMMask]; }
    static uint32_t Word(melonDS::NDS& nds, uint32_t addr)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v |= (uint32_t)Byte(nds, addr + i) << (8 * i);
        return v;
    }
    // ldr r1, [pc, #imm] ; ldr r0, [sp] ; ldr r6, [pc, #imm] ; strh r1 (or r6), [r0]: r1's constant 6, r6's 0.
    static bool Matches(melonDS::NDS& nds, uint32_t a, bool eitherStore)
    {
        const uint8_t b[8] = { Byte(nds, a), Byte(nds, a + 1), Byte(nds, a + 2), Byte(nds, a + 3),
                               Byte(nds, a + 4), Byte(nds, a + 5), Byte(nds, a + 6), Byte(nds, a + 7) };
        if (b[1] != 0x49 || b[2] != 0x00 || b[3] != 0x98 || b[5] != 0x4E || b[7] != 0x80) return false;
        if (!(b[6] == 0x01 || (eitherStore && b[6] == 0x06))) return false;
        const uint32_t lit1 = ((a + 4) & ~3u) + b[0] * 4u, lit6 = ((a + 4 + 4) & ~3u) + b[4] * 4u;
        return Word(nds, lit1) == 6 && Word(nds, lit6) == 0;
    }
    // In the main program (the first megabyte of main RAM), right after a call (the function reads its
    // arguments first): only if the pattern is found once.
    bool Find(melonDS::NDS& nds)
    {
        uint32_t found = 0;
        int count = 0;
        for (uint32_t a = 0x02004000; a < 0x02100000 && count < 2; a += 2)
            if (Byte(nds, a + 1) == 0x49 && Byte(nds, a + 3) == 0x98 && Byte(nds, a + 5) == 0x4E && Byte(nds, a + 7) == 0x80 &&
                Matches(nds, a, true))
            {
                found = a;
                count++;
            }
        if (count != 1) return false;
        at = found;
        return true;
    }
};
