// Music and sound effects at their own volumes, for DS games made with Nintendo's sound library
// (all the Pokemon games).
//
// The game's main processor does not play sound itself: it writes a list of sound commands to
// main memory and sends the list's address to the sound processor through the IPC FIFO, tag 7
// (NitroSDK SND_FlushCommand). The list is read here, as it is sent, before the sound processor
// sees it:
//   - START_SEQ / PREPARE_SEQ (player, sequence data, offset, bank): a sequence starts on one of
//     the 16 sequence players. A sequence that opens 4 or more tracks is music (HeartGold's songs
//     use 7 to 16; its sound effects and cries 1).
//   - PLAYER_PARAM at offset 6, 2 bytes: the player's volume (SNDPlayer.extFader) as an
//     attenuation in tenths of a decibel (0 full, -723 silent). The sound library sends it only when
//     it changes; Music's setting is added to it for a music player, Sound Effects' for the others.
// So that a change of setting is heard at once (not at the game's next change of volume), the
// sound processor's own players (SNDWork.players, 16 of 0x24 bytes, playerId at +1, extFader at
// +6) are found in its memory and their volume is kept at ours every frame (Apply).
//   - PAUSE_SEQ (player, 1 to pause, 0 to go on): the games pause their music for a jingle (a level
//     up, an item; HeartGold also the route's music for a battle's own), and the sound driver lets go
//     of every note the song was holding: after the jingle the song comes back a note at a time, its
//     long notes only when they next start. With MUSIC THROUGH JINGLES (Duck) a music player's pause
//     is sent as its volume at nothing instead, and the pause's end as the game's own volume again:
//     the song plays on unheard under the jingle and is whole the moment it ends (later in the song
//     by the jingle's length). While unheard its channel priority (SNDPlayer.prio) is 0, so its notes
//     take only the hardware channels no other sound wants: at its own, the route's music playing on
//     unheard through a battle took notes from the battle's music, which came out a fifth quieter. A
//     pause that outlasts any jingle (8 seconds: a battle) is then held still with the sound
//     processor's own pause flag, so the song goes on from there, as the game's own pause would. A
//     saved state (or rewind) from under a pause loaded after it, or the other way round, is told by
//     the sound processor's own player (see ducked), so the music is never left silent.
// Layouts: NitroSDK, as in pret/pokediamond include/nitro/SND_command_shared.h, SND_seq_shared.h
// (SNDPlayer: flags at +0, bit 0 active and bit 2 paused; prio +4; volume +5; extFader +6; tracks +8),
// SND_work_shared.h, SND_SetPlayerVolume in arm9/lib/NitroSDK/src/SND_interface.c, and the sound
// processor's side in arm7/lib/src (SND_seq.c, SND_exChannel.c: a channel goes to the higher priority,
// the player's plus the track's).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "NDS.h"

struct GameMusic
{
    melonDS::NDS* Nds = nullptr;
    int Music = 100, Effects = 100; // percent, as the menu shows them
    bool External = false; // the player's own music is playing and QUIET GAME MUSIC is on: no music
    bool Duck = false; // MUSIC THROUGH JINGLES: a music player's pause is a silence instead (see above)
    bool Log = false;
    // MUSIC THROUGH JINGLES: the players silenced in place of a pause (ducked), until the game ends
    // the pause or starts or stops a song on them; of those, the ones at channel priority 0 (lowered,
    // their own in duckPrio) and the ones held still (frozen, after duckFrames reaches FreezeFrames).
    // (A silenced player found at another volume for a sixth of a second, as after a saved state from
    // before the pause is loaded, is let go: duckStale counts the frames.)
    static constexpr uint16_t FreezeFrames = 480;
    uint32_t ducked = 0, lowered = 0, frozen = 0;
    uint8_t duckPrio[16] = {}, duckStale[16] = {};
    uint16_t duckFrames[16] = {};

    bool isMusic[16] = {};
    // (Streamed music, as Black 2's opening: the main processor locks two hardware channels at
    // once (LOCK_CHANNEL, a mask of 2 or more), sets each up as PCM (SETUP_CHANNEL_PCM, its volume
    // in the third word's top byte) and fades them (CHANNEL_VOLUME: mask, volume). Those channels'
    // volumes scaled by Music until UNLOCK_CHANNEL; a single locked channel (a voice, a sample
    // played by the game itself) by Sound Effects.)
    uint32_t streamChannels = 0, sampleChannels = 0;
    int16_t gameVolume[16] = {}; // the volume the game last asked for, before ours
    // (For fast forward's music, ff_music.h: the players a song is on now, and a count that moves
    // each time a song starts or stops.)
    uint32_t musicOn = 0;
    unsigned musicEvent = 0;

    uint8_t* players = nullptr; // the sound processor's 16 players, once found
    unsigned searchWait = 0;
    bool active = false; // our volumes are in the players

    void Attach(melonDS::NDS& nds)
    {
        Nds = &nds;
        Log = getenv("FULLDEX_SOUND_LOG") != nullptr;
        nds.IPCSendHook = &GameMusic::OnSend;
        nds.IPCSendUser = this;
    }

    // The attenuation added for a setting: 20 log10(percent / 100) decibels, in tenths.
    static int Attenuation(int percent)
    {
        if (percent >= 100) return 0;
        if (percent <= 0) return -32768;
        return (int)std::lround(200.0 * std::log10(percent / 100.0));
    }

    // A stream's channel volume (0 to 127) at Music's setting: amplitude, as the hardware's volume is.
    uint32_t StreamVolume(uint32_t v) const
    {
        const int percent = External ? 0 : Music;
        if (percent >= 100) return v;
        return (uint32_t)std::lround(v * std::max(0, percent) / 100.0);
    }

    uint32_t SampleVolume(uint32_t v) const
    {
        if (Effects >= 100) return v;
        return (uint32_t)std::lround(v * std::max(0, Effects) / 100.0);
    }

    int16_t Ours(int player, int16_t game) const
    {
        if (ducked >> player & 1) return -32768;
        const int add = Attenuation(isMusic[player] ? (External ? 0 : Music) : Effects);
        if (add <= -32768) return -32768;
        return (int16_t)std::max(-32768, (int)game + add);
    }

    uint8_t* Ram(uint32_t addr) { return Nds->MainRAM + (addr & Nds->MainRAMMask); }
    uint32_t Read32(uint32_t addr) { uint32_t v; memcpy(&v, Ram(addr), 4); return v; }
    static bool InMain(uint32_t addr) { return addr >= 0x02000000 && addr < 0x03000000; }

    // The sound processor's own player p, where found (layout above).
    uint8_t* Live(int p) const { return players && PlayersAt(players) ? players + p * 0x24 : nullptr; }
    static int16_t LiveVolume(const uint8_t* pl) { int16_t v; memcpy(&v, pl + 6, 2); return v; }

    // How many tracks the sound processor's player p has open (0xFF for none), where found: for a
    // song that started before a saved state was loaded, so not seen starting here (AUTO RESUME).
    int LiveTracks(int p) const
    {
        const uint8_t* pl = Live(p);
        if (!pl || !(pl[0] & 1)) return 0;
        int n = 0;
        for (int t = 0; t < 16; t++) n += pl[8 + t] != 0xFF;
        return n;
    }

    // A command at cmd rewritten as PLAYER_PARAM: size bytes of data at offset in player p's SNDPlayer.
    void SendAsParam(uint32_t cmd, uint32_t offset, uint32_t data, uint32_t size)
    {
        const uint32_t param = 0x06;
        memcpy(Ram(cmd + 4), &param, 4);
        memcpy(Ram(cmd + 12), &offset, 4);
        memcpy(Ram(cmd + 16), &data, 4);
        memcpy(Ram(cmd + 20), &size, 4);
    }

    // PAUSE_SEQ on player p with MUSIC THROUGH JINGLES (see the top).
    void Pause(uint32_t cmd, uint32_t p, bool on)
    {
        uint8_t* pl = Live((int)p);
        const uint32_t bit = 1u << p;
        if (on)
        {
            // (Once silenced, a player's next pause is one too, the setting turned off since or not:
            // a real pause queued behind the silence would be undone by its end.)
            if (!(ducked & bit) && !(Duck && (isMusic[p] || LiveTracks((int)p) >= 4))) return;
            SendAsParam(cmd, 6, (uint16_t)-32768, 2); // (the volume: nothing)
            if (!(ducked & bit)) { duckFrames[p] = 0; duckStale[p] = 0; }
            ducked |= bit;
            // (The priority set here at once: the command carries only the volume. The game's own is
            // kept for the end, and a change it makes meanwhile too: see Commands.)
            if (pl && !(lowered & bit)) { duckPrio[p] = pl[4]; pl[4] = 0; lowered |= bit; }
            if (Log) fprintf(stderr, "[sound] frame %u: player %u paused, silenced instead\n", Nds->NumFrames, p);
            return;
        }
        // The pause's end. (Not one of ours, but found silent at priority 0 as put there: a saved
        // state from under a pause loaded. Put back all the same, at the sound library's usual
        // priority, 64.)
        const bool stale = !(ducked & bit) && Duck && pl && pl[4] == 0 && LiveVolume(pl) == -32768;
        if (!(ducked & bit) && !stale) return;
        if (!pl && (frozen & bit))
        {
            // (Held still, and the players no longer found where they were: the sound processor
            // started afresh. The game's own end of the pause goes as it is.)
            ducked &= ~bit; lowered &= ~bit; frozen &= ~bit;
            return;
        }
        const bool prioToo = pl && ((lowered & bit) || stale);
        const uint8_t prio = (lowered & bit) ? duckPrio[p] : 64;
        ducked &= ~bit; lowered &= ~bit; frozen &= ~bit;
        const int16_t v = Ours((int)p, gameVolume[p]);
        // Sent in the command's place, after whatever the game sent before it: the volume, and the
        // priority with it (4 bytes from +4: prio, volume as it is, extFader).
        if (prioToo) SendAsParam(cmd, 4, prio | (uint32_t)pl[5] << 8 | (uint32_t)(uint16_t)v << 16, 4);
        else SendAsParam(cmd, 6, (uint16_t)v, 2);
        // (Held still here: let go here too, as the game's own end of a pause would.)
        if (pl && (pl[0] & 4)) pl[0] &= ~4;
        if (Log) fprintf(stderr, "[sound] frame %u: player %u goes on%s: volume %d\n", Nds->NumFrames, p, stale ? " (a loaded state's)" : "", v);
    }

    // How many tracks a sequence opens: its first command, 0xFE, and a mask of the tracks.
    int Tracks(uint32_t base, uint32_t offset)
    {
        const uint32_t at = base + offset;
        if (!InMain(at)) return 0;
        const uint8_t* p = Ram(at);
        if (p[0] != 0xFE) return 1;
        const unsigned mask = p[1] | p[2] << 8;
        int n = 0;
        for (unsigned m = mask; m; m &= m - 1) n++;
        return n;
    }

    static void OnSend(void* user, uint32_t word)
    {
        auto* self = static_cast<GameMusic*>(user);
        if ((word & 0x1F) == 7) self->Commands(word >> 6);
    }

    void Commands(uint32_t cmd)
    {
        for (int guard = 0; InMain(cmd) && guard < 256; guard++)
        {
            const uint32_t id = Read32(cmd + 4);
            const uint32_t a0 = Read32(cmd + 8), a1 = Read32(cmd + 12), a2 = Read32(cmd + 16), a3 = Read32(cmd + 20);
            if ((id == 0x00 || id == 0x02) && a0 < 16) // START_SEQ, PREPARE_SEQ
            {
                ducked &= ~(1u << a0); lowered &= ~(1u << a0); frozen &= ~(1u << a0); // (a new song starts afresh: PlayerInit)
                const int tracks = Tracks(a1, a2);
                isMusic[a0] = tracks >= 4;
                if (isMusic[a0]) { musicOn |= 1u << a0; musicEvent++; }
                else if (musicOn >> a0 & 1) { musicOn &= ~(1u << a0); musicEvent++; }
                gameVolume[a0] = 0; // (the sound processor starts a player at full: PlayerInit)
                if (Log) fprintf(stderr, "[sound] player %u starts a sequence of %d tracks: %s\n", a0, tracks, isMusic[a0] ? "music" : "effect");
            }
            else if (id == 0x01 && a0 < 16) // STOP_SEQ
            {
                if (musicOn >> a0 & 1) { musicOn &= ~(1u << a0); musicEvent++; } // (the song on this player ends)
                ducked &= ~(1u << a0); lowered &= ~(1u << a0); frozen &= ~(1u << a0); // (the next song starts afresh: PlayerInit)
            }
            else if (id == 0x04 && a0 < 16) Pause(cmd, a0, a1 != 0); // PAUSE_SEQ (see Duck)
            else if (id == 0x06 && a0 < 16 && a1 == 4 && a3 == 1 && (lowered >> a0 & 1)) // PLAYER_PARAM: priority
            {
                duckPrio[a0] = (uint8_t)a2; // (a silenced player's: kept for the end, 0 meanwhile)
                const uint32_t none = 0;
                memcpy(Ram(cmd + 16), &none, 4);
            }
            else if (id == 0x06 && a0 < 16 && a1 == 6 && a3 == 2) // PLAYER_PARAM: volume
            {
                const int16_t game = (int16_t)a2;
                gameVolume[a0] = game;
                const int16_t ours = Ours(a0, game);
                if (ours != game)
                {
                    const uint32_t v = (uint32_t)(uint16_t)ours;
                    memcpy(Ram(cmd + 16), &v, 4);
                }
                if (Log) fprintf(stderr, "[sound] player %u volume %d -> %d\n", a0, game, ours);
            }
            else if (id == 26) (__builtin_popcount(a0 & 0xFFFF) >= 2 ? streamChannels : sampleChannels) |= a0 & 0xFFFF; // LOCK_CHANNEL
            else if (id == 27) { streamChannels &= ~a0; sampleChannels &= ~a0; } // UNLOCK_CHANNEL
            else if (id == 14 && ((streamChannels | sampleChannels) >> (a0 & 15) & 1)) // SETUP_CHANNEL_PCM
            {
                const bool music = streamChannels >> (a0 & 15) & 1;
                const uint32_t v = a2 >> 24, scaled = music ? StreamVolume(v) : SampleVolume(v);
                if (scaled != v) { const uint32_t w = (a2 & 0x00FFFFFF) | scaled << 24; memcpy(Ram(cmd + 16), &w, 4); }
                if (Log) fprintf(stderr, "[sound] %s channel %u volume %u -> %u\n", music ? "stream" : "sample", a0 & 15, v, scaled);
            }
            else if (id == 20 && (a0 & (streamChannels | sampleChannels))) // CHANNEL_VOLUME
            {
                const uint32_t scaled = (a0 & streamChannels) ? StreamVolume(a1 & 0x7F) : SampleVolume(a1 & 0x7F);
                if (scaled != (a1 & 0x7F)) { const uint32_t w = (a1 & ~0x7Fu) | scaled; memcpy(Ram(cmd + 12), &w, 4); }
            }
            if (Log && id >= 12 && id <= 28) // (the channel commands, as streams use them)
                fprintf(stderr, "[sound] command %u: %08x %08x %08x %08x\n", id, a0, a1, a2, a3);
            cmd = Read32(cmd);
        }
    }

    // Whether 16 players start here: player p's id (byte 1) is p, its flags (byte 0) at most 7.
    static bool PlayersAt(const uint8_t* m)
    {
        for (int p = 0; p < 16; p++)
            if (m[p * 0x24 + 1] != p || m[p * 0x24] > 7) return false;
        return true;
    }

    uint8_t* FindPlayers()
    {
        const struct { uint8_t* mem; uint32_t size; } where[] = {
            { Nds->ARM7WRAM, Nds->ARM7WRAMSize }, { Nds->SharedWRAM, Nds->SharedWRAMSize }, { Nds->MainRAM, Nds->MainRAMMask + 1 } };
        for (const auto& w : where)
        {
            if (!w.mem || w.size < 16 * 0x24) continue;
            for (uint32_t o = 0; o + 16 * 0x24 <= w.size; o += 4)
                if (PlayersAt(w.mem + o)) return w.mem + o;
        }
        return nullptr;
    }

    // Every frame: the players' volume as ours (a change of setting heard at once).
    void Apply()
    {
        if (!Nds) return;
        if (players && !PlayersAt(players)) players = nullptr;
        // (Both at full: the game's own volumes put back once, then left alone. With MUSIC THROUGH
        // JINGLES the players are found all the same, for the silenced ones; see ducked.)
        const bool ours = Music < 100 || Effects < 100 || External;
        if (!ours && !active && !Duck && !ducked) return;
        if (!players)
        {
            if (searchWait) { searchWait--; return; }
            players = FindPlayers();
            if (!players) { searchWait = 120; return; }
            if (Log) fprintf(stderr, "[sound] the sound processor's players found\n");
        }
        for (int p = 0; p < 16; p++)
        {
            int16_t now;
            memcpy(&now, players + p * 0x24 + 6, 2);
            if (ducked >> p & 1)
            {
                // (Kept silent by the commands sent in its place, not here: so another volume found
                // here is a saved state's, as above.)
                uint8_t* pl = players + p * 0x24;
                if (now != -32768)
                {
                    if (++duckStale[p] >= 10)
                    {
                        ducked &= ~(1u << p); lowered &= ~(1u << p); frozen &= ~(1u << p);
                        duckStale[p] = 0;
                        if (Log) fprintf(stderr, "[sound] frame %u: player %u no longer silent (%d): let go\n", Nds->NumFrames, p, now);
                    }
                    continue;
                }
                duckStale[p] = 0;
                if (duckFrames[p] < FreezeFrames) duckFrames[p]++;
                else if (!(frozen >> p & 1) && (pl[0] & 1))
                {
                    // Longer than a jingle: held still, as the game's own pause, until its end.
                    pl[0] |= 4;
                    frozen |= 1u << p;
                    if (Log) fprintf(stderr, "[sound] frame %u: player %u silent %u frames: held still\n", Nds->NumFrames, p, (unsigned)FreezeFrames);
                }
                continue;
            }
            if (!ours && !active) continue;
            const int16_t want = Ours(p, gameVolume[p]);
            if (now != want) memcpy(players + p * 0x24 + 6, &want, 2);
        }
        active = ours;
    }
};
