#pragma once
// COMPANION for the GBA games: what the game holds now (the party, the bag, where you are, the
// Pokedex's seen and caught), read from the running game's memory while it is paused. The
// addresses for each version and language are PokeDaisy's (NativeConfigsGen.kt, from the pret
// decompilations' maps); the party structures' decoding is Gen3Mon.kt's (GPL-3.0).
// Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace GbaCompanion
{
// struct BattlePokemon as a game keeps it (PokeDaisy's pd_battle_layout): these games' and the Complete FireRed
// Upgrade's, pokeemerald-expansion's (and R.O.W.E.'s), and the two hacks with their own order. -1: not kept there.
struct BattleLayout
{
    int size, species, moves, type1, type2, pp, hp, level, maxHp, status, personality, ability;
    bool abilityU16;
    int item, stats, ivs, nickname;
};
inline constexpr BattleLayout kVanillaBattle = { 0x58, 0x00, 0x0C, 0x21, 0x22, 0x24, 0x28, 0x2A, 0x2C, 0x4C, 0x48, 0x20, false, 0x2E, 0x02, 0x14, 0x30 };
inline constexpr BattleLayout kExpansionBattle = { 0x60, 0x00, 0x0C, 0x22, 0x23, 0x25, 0x2A, 0x2C, 0x2E, 0x50, 0x4C, 0x20, true, 0x30, 0x02, 0x14, -1 };
inline constexpr BattleLayout kExpansion115Battle = { 0x88, 0x00, 0x0C, 0x22, 0x23, 0x25, 0x2A, 0x2C, 0x2E, 0x50, 0x4C, 0x20, true, 0x30, 0x02, 0x14, -1 };
inline constexpr BattleLayout kExpansion116Battle = { 0x8C, 0x00, 0x0C, 0x22, 0x23, 0x25, 0x2A, 0x2C, 0x2E, 0x50, 0x4C, 0x20, true, 0x30, 0x02, 0x14, -1 };
inline constexpr BattleLayout kEliteReduxBattle = { 0x68, 0x14, 0x20, 0x44, 0x45, 0x47, 0x36, 0x4B, 0x38, 0x08, 0x00, 0x28, true, 0x3A, 0x16, -1, -1 };
inline constexpr BattleLayout kMonomythBattle = { 0x58, 0x0C, 0x18, 0x0F, 0x10, 0x00, 0x2C, 0x12, 0x2E, 0x30, 0x40, 0x0E, false, 0x20, 0x22, -1, -1 };

struct Config
{
    const char* code; int revision;
    uint32_t party, partyCount, saveBlock1, saveBlock2;
    bool staticSaveBlocks;  // (Ruby and Sapphire: the addresses are the blocks, not pointers to them)
    uint32_t bagPockets; int pocketCount;
    int keyOff;             // (SaveBlock2's encryption key; -1: none, Ruby and Sapphire)
    char pocketOrder;       // (E: Items, Poke Balls, TMs and HMs, Berries, Key Items; F: Items, Key Items, Poke Balls, TM Case, Berry Pouch)
    uint32_t battleMons, battlerPositions, gMain, inBattleOff; // (gBattleMons, which battler is where, gMain and its inBattle byte)
    // A ROM hack's own (Hacks(), found by the ROM's SHA-1 before any code's); the rest only differs in those.
    const char* sha1 = nullptr;
    int layout = 0;                    // (the party's struct: 0 these games'; 1 R.O.W.E.'s, 2 Elite Redux's, 3 Monomyth's)
    int monSize = 100;                 // (a party Pokemon's size: R.O.W.E.'s and Elite Redux's 0x4C)
    int locationOff = 4;               // (SaveBlock1's location {map group, map}: Heart and Soul's and Monomyth's 4 later)
    bool plainBoxes = false;           // (the Complete FireRed Upgrade's: its Pokemon's 48 bytes plain, in order, unchecked)
    int bagBitsOff = -1, bagBitsCount = 0; // (Elite Redux's bag: a bit an item in SaveBlock1, owned or not, no quantities)
    unsigned speciesMask = 0xFFFF, moveMask = 0xFFFF, ppMask = 0xFF; // (the expansion packs other bits beside them)
    int moneyOff = -1;                 // (in SaveBlock1; -1: 0x490, FireRed and LeafGreen's 0x290)
    const char* const* pocketNames = nullptr; // (gBagPockets' pockets' names in order, pocketCount of them; null: the game's five)
    bool dexInSb1 = false;             // (the Pokedex's caught and seen flags: SaveBlock2's (these games) or SaveBlock1's)
    int dexCaughtOff = 0x28, dexSeenOff = 0x5C, dexBytes = 52, national = 412;
    const BattleLayout* battle = &kVanillaBattle;
    int typeMode = 0;                  // (types: 0 these games' (the Complete FireRed Upgrade's Fairy 23), 1 the expansion's
                                       // (from a type of none at 0, Fairy 19), 2 these games' with Fairy at 18)
    uint32_t enemyParty = 0;           // (gEnemyParty where it isn't beside gPlayerParty)
};
inline const std::vector<Config>& Configs()
{
    static const std::vector<Config> c = {
    { "AXPD", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD3DC, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPD", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD3DC, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 0, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1670, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 1, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1690, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPE", 2, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1690, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPF", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8A6C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPF", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8A6C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPI", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C1D40, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPI", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C1D40, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPJ", 0, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A50, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXPJ", 1, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A50, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXPS", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C4D48, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXPS", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C4D48, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVD", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD470, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVD", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083CD470, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 0, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1618, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 1, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1634, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVE", 2, 0x03004360, 0x03004350, 0x02025734, 0x02024EA4, true, 0x083C1634, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVF", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8F3C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVF", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C8F3C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVI", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C209C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVI", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C209C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVJ", 0, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A6C, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXVJ", 1, 0x03004290, 0x03004280, 0x02025494, 0x02024C04, true, 0x08397A6C, 5, -1, 'E', 0x020247E0, 0x020247D2, 0x030016E0, 0x439 },
    { "AXVS", 0, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C500C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "AXVS", 1, 0x03004370, 0x03004360, 0x02025734, 0x02024EA4, true, 0x083C500C, 5, -1, 'E', 0x02024A80, 0x02024A72, 0x03001770, 0x43D },
    { "BPED", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEE", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEF", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEI", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPEJ", 0, 0x02024190, 0x0202418D, 0x03005AEC, 0x03005AF0, false, 0x02039A78, 5, 0xAC, 'E', 0x02023D28, 0x02023D1A, 0x03002360, 0x439 },
    { "BPES", 0, 0x020244EC, 0x020244E9, 0x03005D8C, 0x03005D90, false, 0x02039DD8, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030022C0, 0x439 },
    { "BPGD", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGE", 0, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPGE", 1, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPGF", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGI", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPGJ", 0, 0x020241E4, 0x02023F89, 0x03005048, 0x0300504C, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003130, 0x439 },
    { "BPGJ", 1, 0x020241E4, 0x02023F89, 0x03004FA8, 0x03004FAC, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003090, 0x439 },
    { "BPGS", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRD", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRE", 0, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPRE", 1, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 },
    { "BPRF", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRI", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    { "BPRJ", 0, 0x020241E4, 0x02023F89, 0x03005048, 0x0300504C, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003130, 0x439 },
    { "BPRJ", 1, 0x020241E4, 0x02023F89, 0x03004FA8, 0x03004FAC, false, 0x020397D8, 5, 0xF20, 'F', 0x02023B44, 0x02023B36, 0x03003090, 0x439 },
    { "BPRS", 0, 0x02024284, 0x02024029, 0x03004F58, 0x03004F5C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x03003040, 0x439 },
    };
    return c;
}
// ROM hacks whose addresses and structures differ from their base game's: most from PokeDaisy's gamedata
// (gamedata/gen3/hacks, their generated 3ds/core/pd_configs_gen.h, commit 79bd00e), each checked there against the
// hack's own RAM; four found here (below). Each by the ROM's SHA-1, so a release they weren't read from isn't taken
// for one.
inline const std::vector<Config>& Hacks()
{
    static const std::vector<Config> h = [] {
        std::vector<Config> v;
        auto add = [&](const char* sha1, const char* code, Config c) { c.sha1 = sha1; c.code = code; v.push_back(c); };
        // (The Complete FireRed Upgrade's hacks and Ultra Violet: FireRed 1.0's RAM, the Pokedex's flags moved to SaveBlock1.)
        const Config fr = { "BPRE", 0, 0x02024284, 0x02024029, 0x03005008, 0x0300500C, false, 0x0203988C, 5, 0xF20, 'F', 0x02023BE4, 0x02023BD6, 0x030030F0, 0x439 };
        Config c = fr;
        c.dexInSb1 = true; c.dexSeenOff = 0x310; c.dexCaughtOff = 0x3B4; c.dexBytes = 164; c.national = 1025; c.plainBoxes = true;
        add("964f951a0fdaf209e4ea1344883ef0d557bb3a80", "BPRE", c); // Radical Red 4.1
        c = fr;
        c.dexInSb1 = true; c.dexSeenOff = 0x310; c.dexCaughtOff = 0x38D; c.dexBytes = 125; c.national = 905; c.plainBoxes = true;
        add("b4776b82a4c7915d0fadeaa27e013523f99dfd94", "BPRE", c); // Unbound 2.1.1.1
        c = fr;
        c.enemyParty = 0x0202402C; c.national = 386;
        add("3b6c51d8fb6b513bdd01b7bda3ed8965bc985d58", "PUVV", c); // Ultra Violet 1.22
        // (Emerald Legacy 1.1.4: Emerald rebuilt from its own source; its blocks' pointers, gMain and the bag moved.)
        c = { "BPEE", 0, 0x020244EC, 0x020244E9, 0x03005F6C, 0x03005F70, false, 0x0203A100, 5, 0xAC, 'E', 0x02024084, 0x02024076, 0x030024A0, 0x439 };
        c.enemyParty = 0x02024744; c.national = 386;
        add("cbd71af65ee3dcd53280a6a12014bfb11e8f626a", "BPEE", c);
        // (pokeemerald-expansion builds: each its own RAM; species, moves and PP packed with other bits.)
        c = { "BPEE", 0, 0x020375F8, 0x020375F5, 0x03006218, 0x0300621C, false, 0x0200AC44, 6, 0x44, 'E', 0x02000360, 0x02000350, 0x03004218, 0x439 };
        static const char* const imperiumPockets[] = { "ITEMS", "MEGA STONES", "POKE BALLS", "TMS AND HMS", "BERRIES", "KEY ITEMS" };
        c.speciesMask = 0x07FF; c.moveMask = 0x07FF; c.ppMask = 0x7F; c.pocketNames = imperiumPockets;
        c.dexInSb1 = true; c.dexSeenOff = 0x2F58; c.dexCaughtOff = 0x2FD9; c.dexBytes = 129; c.national = 1025;
        c.battle = &kExpansionBattle; c.typeMode = 1; c.enemyParty = 0x02037850;
        add("1d20091c4d936f5eb122db8780554dd0829ffb63", "BPEE", c); // Emerald Imperium 1.3.1 (expansion 1.10.0)
        c = { "BPEE", 0, 0x02034764, 0x020342A4, 0x030041D8, 0x030041D4, false, 0x02006DD0, 6, 0xAC, 'E', 0x02000420, 0x02000238, 0x03005BD8, 0x439 };
        static const char* const hnsPockets[] = { "ITEMS", "MEDICINE", "POKE BALLS", "TMS AND HMS", "BERRIES", "KEY ITEMS" };
        c.speciesMask = 0x07FF; c.moveMask = 0x07FF; c.ppMask = 0x7F; c.moneyOff = 0x494; c.pocketNames = hnsPockets;
        c.locationOff = 8; // (a word before SaveBlock1's position, as its ROM header's party offsets say: 4 past Emerald's)
        c.dexInSb1 = true; c.dexSeenOff = 0x39C0; c.dexCaughtOff = 0x3A7F; c.dexBytes = 191; c.national = 1080;
        c.battle = &kExpansion115Battle; c.typeMode = 1;
        add("79ee6df0869c1773c8c6a5f764afc1f8d833d8bb", "BPEE", c); // Heart and Soul 2.0.6 (expansion 1.15.2)
        // (Wishes of Tomorrow 1.3 (expansion 1.15.3), Generation 3 Evolved 1.0.25 (expansion 1.16.3, an Emerald of its own
        // under FireRed's code) and Inclement Emerald Beta 1.1.3 (an old expansion): not in PokeDaisy; found here, the save
        // blocks' pointers by the literal pools that load them, the parties by their six empty mail bytes and the count
        // byte by its references, the bag by its pockets' pointers into SaveBlock1, the money by a new game's, gMain by
        // its callbacks and frame counters, the battle's by a battle's memory, the Pokedex's by a battle's sighting.)
        c = { "BPEE", 0, 0x02032148, 0x02031EE9, 0x0300523C, 0x03005238, false, 0x02006B74, 5, 0xAC, 'E', 0x02000424, 0x02000240, 0x03006738, 0x439 };
        c.speciesMask = 0x07FF; c.moveMask = 0x07FF; c.ppMask = 0x7F;
        c.dexInSb1 = true; c.dexSeenOff = 0x359C; c.dexCaughtOff = 0x361D; c.dexBytes = 129; c.national = 1025;
        c.battle = &kExpansion115Battle; c.typeMode = 1; c.enemyParty = 0x02031EF0;
        add("0fb19d1d9ed68b550a4c1f562eaf0977d1007dbc", "BPEE", c); // Wishes of Tomorrow 1.3
        static const char* const g3ePockets[] = { "ITEMS", "MEDICINE", "BERRIES", "TMS AND HMS", "POKE BALLS", "POWER UPS", "KEY ITEMS" };
        c = { "BPRE", 0, 0x02033664, 0x02033654, 0x030052E0, 0x030052DC, false, 0x02006C54, 7, 0xB0, 'E', 0x020004FC, 0x020001E4, 0x030067DC, 0x439 };
        c.speciesMask = 0x07FF; c.moveMask = 0x07FF; c.ppMask = 0x7F; c.pocketNames = g3ePockets;
        c.dexInSb1 = true; c.dexSeenOff = 0x2BD4; c.dexCaughtOff = 0x2C55; c.dexBytes = 129; c.national = 1025;
        c.battle = &kExpansion116Battle; c.typeMode = 1; c.enemyParty = 0x020338BC;
        add("22179552ffed9800e30817a1f57ad5e5b78d181f", "BPRE", c); // Generation 3 Evolved 1.0.25
        static const char* const inclementPockets[] = { "ITEMS", "MEDICINE", "BATTLE ITEMS", "TMS AND HMS", "BERRIES", "POKE BALLS", "KEY ITEMS", "MEGA STONES" };
        c = { "BPEE", 0, 0x02025414, 0x02025411, 0x03004B6C, 0x03004B70, false, 0x0203B040, 8, 0xAC, 'E', 0x02024EE0, 0x02024ED2, 0x03002C00, 0x439 };
        c.pocketNames = inclementPockets; c.battle = &kExpansionBattle; c.typeMode = 2; c.enemyParty = 0x0202566C;
        c.dexInSb1 = true; c.dexSeenOff = 0x3004; c.dexCaughtOff = 0x3075; c.dexBytes = 113; c.national = 898;
        add("1931df920c282762a0578199ab4568977fd72902", "BPEE", c); // Inclement Emerald Beta 1.1.3
        // (Elite Redux 2.65.5.2b: its own rebuild of Inclement Emerald, its packed 0x4C-byte Pokemon and battle struct
        // (PokeDaisy's, of 2.65.4.1b, whose IWRAM and save layout this one keeps), its money plain and its bag a bit
        // an item; the party and the battle's addresses found here as the three above.)
        c = { "BPEE", 0, 0x02039928, 0x02039925, 0x0300340C, 0x03003410, false, 0, 0, -1, 'E', 0x0201C528, 0x0201C490, 0x03003420, 0x439 };
        c.layout = 2; c.monSize = 0x4C; c.moneyOff = 0x400; c.bagBitsOff = 0x40A; c.bagBitsCount = 440;
        c.dexInSb1 = true; c.dexSeenOff = 0x2E54; c.dexCaughtOff = 0x2F10; c.dexBytes = 188; c.national = 1504;
        c.battle = &kEliteReduxBattle; c.typeMode = 2; c.enemyParty = 0x02039AF0;
        add("2528d43021b28e71a11736cfbf2ae683100ffbc9", "BPEE", c); // Elite Redux 2.65.5.2b
        // (R.O.W.E. 2.1.9.1 Experimental: its own fork; a packed 0x4C-byte party Pokemon, the expansion's battle struct, the
        // Fairy type at 18. Every address is also in the ROM's own symbol table, for its online play.)
        c = { "BPEE", 0, 0x02025128, 0x02025125, 0x03004AFC, 0x03004B00, false, 0x0203B3B4, 10, 0x40, 'E', 0x02024C10, 0x02024C02, 0x03002B90, 0x439 };
        static const char* const rowePockets[] = { "ITEMS", "MEDICINE", "POKE BALLS", "BATTLE ITEMS", "TYPE ITEMS", "MEGA STONES", "BERRIES", "POWER-UP", "TMS AND HMS", "KEY ITEMS" };
        c.layout = 1; c.monSize = 0x4C; c.moneyOff = 0x400; c.pocketNames = rowePockets;
        c.dexInSb1 = true; c.dexSeenOff = 0x3320; c.dexCaughtOff = 0x33DC; c.dexBytes = 0xBC; c.national = 1025;
        c.battle = &kExpansionBattle; c.speciesMask = 0x07FF; c.typeMode = 2; c.enemyParty = 0x020252F0;
        add("81bd0f4bfa1c04ab2c6faab1bddd10e8a390ea77", "BPEE", c);
        // (Monomyth 1.1: its own closed build, its Pokemon its own; vanilla's party struct under a key of its own, its
        // battle struct in its own order, SaveBlock1's location 4 later and gMain's inBattle byte 0x17 before it.)
        c = { "BPEE", 0, 0x02025908, 0x02025905, 0x03001240, 0x03001244, false, 0x0203C18C, 8, 0x40, 'E', 0x0202541C, 0x0202540B, 0x02020601, 0 };
        static const char* const monomythPockets[] = { "ITEMS", "MEDICINE", "POKE BALLS", "BATTLE ITEMS", "TMS AND HMS", "BERRIES", "KEY ITEMS", "FREE SPACE" };
        c.layout = 3; c.moneyOff = 0x494; c.locationOff = 8; c.pocketNames = monomythPockets;
        c.dexInSb1 = true; c.dexSeenOff = 0x37F4; c.dexCaughtOff = 0x3827; c.dexBytes = 0x33; c.national = 405;
        c.battle = &kMonomythBattle; c.typeMode = 2; c.enemyParty = 0x02025B60;
        add("eb660caffbc26e1f09f86714d03af4c3eccd9e4d", "BPEE", c);
        return v;
    }();
    return h;
}
// The game's own version first, then any revision of its code (a hack keeps its base game's); a known hack by its
// SHA-1 before either, and a hack of a game rebuilt from source (none of its code where the game's is) not at all.
// (A game's own code loads gSaveBlock1Ptr from hundreds of literal pools: a ROM with its code where the game's is (a
// translation, a randomizer, a hack edited in place) has them all; one rebuilt from source has none at that address.)
inline int References(const uint8_t* rom, size_t size, uint32_t word)
{
    int n = 0;
    for (size_t at = 0; at + 4 <= size && at < 0x200000; at += 4)
        n += rom[at] == (word & 0xFF) && rom[at + 1] == (word >> 8 & 0xFF) && rom[at + 2] == (word >> 16 & 0xFF) && rom[at + 3] == word >> 24;
    return n;
}
inline const Config* ConfigFor(const char* code, int revision, const std::string& sha1 = std::string(), const uint8_t* rom = nullptr, size_t size = 0)
{
    for (const Config& c : Hacks())
        if (sha1 == c.sha1) return &c;
    const Config* any = nullptr;
    for (const Config& c : Configs())
        if (!strncmp(c.code, code, 4)) { if (c.revision == revision) { any = &c; break; } if (!any) any = &c; }
    if (any && rom && References(rom, size, any->saveBlock1) < 50) return nullptr;
    return any;
}
// A type as these games number it (companion_types.h): the expansion's count from none at 0 and make 19 Fairy; the
// Complete FireRed Upgrade keeps these games' and makes 23 Fairy.
inline int MapType(int raw, int mode)
{
    if (mode == 1) return raw >= 1 && raw <= 18 ? raw - 1 : raw == 19 ? 9 : -1;
    if (mode == 2) return raw == 18 ? 9 : raw == 9 || raw > 18 ? -1 : raw;
    return raw == 23 ? 9 : raw == 9 ? -1 : raw;
}

struct Mon
{
    int species = 0, item = 0, level = 0, hp = 0, maxHp = 0, friendship = 0;
    uint32_t personality = 0, otId = 0, status = 0, exp = 0;
    bool egg = false, abilityBit = false;
    std::string nickname;
    int moves[4] = {}, pp[4] = {}, ppBonuses = 0;
    int iv[6] = {}, ev[6] = {}, stats[6] = {}; // (HP, Attack, Defence, Speed, Sp. Atk, Sp. Def)
};
struct Item { int id = 0, quantity = 0; };
// One battler as the battle keeps it (struct BattlePokemon, 0x58 bytes).
struct Battler
{
    int species = 0, level = 0, hp = 0, maxHp = 0, type1 = 0, type2 = 0, ability = 0, item = 0;
    uint32_t status = 0, personality = 0;
    int moves[4] = {}, pp[4] = {}, stats[5] = {}; // (Attack, Defence, Speed, Sp. Atk, Sp. Def)
    int iv[6] = {}; // (HP, Attack, Defence, Speed, Sp. Atk, Sp. Def, as the party's)
    std::string nickname;
};
struct Snapshot
{
    bool ok = false;
    std::vector<Mon> party;
    int mapGroup = -1, mapNum = -1;
    long long money = -1;
    std::vector<std::pair<const char*, std::vector<Item>>> pockets;
    std::vector<uint8_t> seen, caught; // (by National Pokedex number)
    bool inBattle = false;
    Battler mine, foe; // (in a battle: your Pokemon out and the foe's, the left ones in a double)
    std::vector<Mon> foeParty; // (in a battle: gEnemyParty in order, the trainer's whole team)
};

inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

// One party Pokemon (100 bytes): its 48 bytes of growth, attacks, EVs and misc, each 12, XORed
// with personality ^ OT ID and in the order personality % 24 gives; checked by their checksum.
inline bool Decode(const uint8_t* m, Mon& out, const Config* c = nullptr)
{
    static const uint8_t order[24][4] = {
        { 0, 1, 2, 3 }, { 0, 1, 3, 2 }, { 0, 2, 1, 3 }, { 0, 2, 3, 1 }, { 0, 3, 1, 2 }, { 0, 3, 2, 1 },
        { 1, 0, 2, 3 }, { 1, 0, 3, 2 }, { 1, 2, 0, 3 }, { 1, 2, 3, 0 }, { 1, 3, 0, 2 }, { 1, 3, 2, 0 },
        { 2, 0, 1, 3 }, { 2, 0, 3, 1 }, { 2, 1, 0, 3 }, { 2, 1, 3, 0 }, { 2, 3, 0, 1 }, { 2, 3, 1, 0 },
        { 3, 0, 1, 2 }, { 3, 0, 2, 1 }, { 3, 1, 0, 2 }, { 3, 1, 2, 0 }, { 3, 2, 0, 1 }, { 3, 2, 1, 0 } };
    const uint32_t pid = U32(m), ot = U32(m + 4);
    // (Monomyth XORs its substructs with a key of its own: PokeDaisy's monomyth_key, from the game's box code.)
    auto ror = [](uint32_t x, int n) { return x >> n | x << (32 - n); };
    uint32_t key = pid ^ ot;
    if (c && c->layout == 3)
    {
        uint32_t r = ot + ror(pid ^ 0x690, 7);
        r = ror(r + ror(pid, 11), 13);
        r = pid + ror(0x5A000000u ^ r ^ ot, 17);
        key = r ^ ror(ot, 5);
    }
    if (!pid && !ot) return false;
    uint8_t d[48];
    for (int i = 0; i < 12; i++)
    {
        const uint32_t v = U32(m + 32 + i * 4) ^ key;
        memcpy(d + i * 4, &v, 4);
    }
    uint16_t sum = 0;
    for (int i = 0; i < 24; i++) sum = (uint16_t)(sum + U16(d + i * 2));
    const uint8_t* sub[4];
    if (sum == U16(m + 28) && sum)
        for (int slot = 0; slot < 4; slot++) sub[order[pid % 24][slot]] = d + slot * 12;
    else if (c && c->plainBoxes)
    {
        // (The Complete FireRed Upgrade keeps them plain, growth, attacks, EVs and the rest in that order, with no
        // checksum kept: PokeDaisy's plain box.)
        memcpy(d, m + 32, 48);
        for (int k = 0; k < 4; k++) sub[k] = d + k * 12;
    }
    else return false;
    const uint8_t *g = sub[0], *a = sub[1], *e = sub[2], *x = sub[3];
    out = Mon();
    out.personality = pid; out.otId = ot;
    out.species = U16(g); out.item = U16(g + 2); out.exp = U32(g + 4); out.ppBonuses = g[8]; out.friendship = g[9];
    for (int i = 0; i < 4; i++) { out.moves[i] = U16(a + i * 2); out.pp[i] = a[8 + i]; }
    // (The expansion packs other bits beside the species, the moves, their PP and the experience.)
    if (c && (c->speciesMask != 0xFFFF || c->moveMask != 0xFFFF || c->ppMask != 0xFF))
    {
        out.species &= (int)c->speciesMask;
        for (int i = 0; i < 4; i++) { out.moves[i] &= (int)c->moveMask; out.pp[i] &= (int)c->ppMask; }
        out.exp &= 0x1FFFFFu;
    }
    for (int i = 0; i < 6; i++) out.ev[i] = e[i];
    const uint32_t ivs = U32(x + 4);
    for (int i = 0; i < 6; i++) out.iv[i] = ivs >> (i * 5) & 31;
    out.egg = ivs >> 30 & 1; out.abilityBit = ivs >> 31 & 1;
    out.status = U32(m + 80); out.level = m[84]; out.hp = U16(m + 86); out.maxHp = U16(m + 88);
    out.stats[0] = out.maxHp;
    for (int i = 1; i < 6; i++) out.stats[i] = U16(m + 88 + i * 2);
    out.nickname.assign((const char*)m + 8, 10);
    return out.species > 0 && out.level <= 100;
}
// R.O.W.E. 2.x's and Elite Redux 2.65's own 0x4C-byte party Pokemon: plain, packed in fields of their own (PokeDaisy's
// decode_rowe_mon and decode_er_mon, each read against the game's own screens there). R.O.W.E.'s has no held item.
inline bool DecodePacked(const uint8_t* m, Mon& out, bool rowe)
{
    out = Mon();
    out.personality = U32(m);
    if (rowe)
    {
        const uint32_t w14 = U32(m + 0x14), w18 = U32(m + 0x18);
        out.species = (int)(U32(m + 0x1C) & 0xFFF);
        out.moves[0] = w14 & 0x3FF; out.moves[1] = w18 & 0x3FF; out.moves[2] = w18 >> 10 & 0x3FF; out.moves[3] = U16(m + 0x1E) >> 1 & 0x3FF;
        out.exp = w14 >> 10 & 0x1FFFFF;
        out.egg = (m[0x27] & 0x80) != 0;
        for (int i = 0; i < 6; i++) out.ev[i] = m[0x20 + i];
        out.nickname.assign((const char*)m + 0x08, 12);
    }
    else
    {
        const uint32_t w08 = U32(m + 0x08), w0c = U32(m + 0x0C), w10 = U32(m + 0x10), w14 = U32(m + 0x14);
        out.species = (int)(w10 & 0xFFFF);
        out.moves[0] = w08 & 0x7FF; out.moves[1] = w0c & 0x7FF; out.moves[2] = w0c >> 11 & 0x7FF; out.moves[3] = w10 >> 16 & 0x7FF;
        out.exp = w08 >> 11 & 0x1FFFFF;
        out.item = (int)(w14 & 0x3FF);
        out.egg = (w14 >> 15 & 1) != 0;
        for (int i = 0; i < 6; i++) { out.ev[i] = m[0x18 + i]; out.iv[i] = i == 3 && (m[0x32] & 0x20) ? 0 : 31; }
        out.nickname.assign((const char*)m + 0x26, 12);
    }
    for (int i = 0; i < 4; i++) out.pp[i] = m[0x34 + i];
    out.status = U32(m + 0x38); out.level = m[0x3C]; out.hp = U16(m + 0x3E); out.maxHp = U16(m + 0x40);
    for (int i = 0; i < 6; i++) out.stats[i] = U16(m + 0x40 + i * 2);
    return out.species > 0 && out.level <= 100;
}
// A party Pokemon as the game keeps it.
inline bool DecodeMon(const uint8_t* m, Mon& out, const Config& c)
{
    if (c.layout == 1 || c.layout == 2) return DecodePacked(m, out, c.layout == 1);
    return Decode(m, out, &c);
}

// The game as it is now. read(address, n, buffer): its memory, false where it can't.
template <class Read>
Snapshot Take(const Config& c, Read read)
{
    Snapshot s;
    uint8_t b[6 * 100];
    auto u32At = [&](uint32_t at) { uint8_t v[4] = {}; return read(at, 4, v) ? U32(v) : 0u; };
    uint8_t count = 0;
    if (!read(c.partyCount, 1, &count) || count > 6) count = 0;
    if (count && read(c.party, count * (uint32_t)c.monSize, b))
        for (int i = 0; i < count; i++) { Mon m; if (DecodeMon(b + i * c.monSize, m, c)) s.party.push_back(m); }
    const uint32_t sb1 = c.staticSaveBlocks ? c.saveBlock1 : u32At(c.saveBlock1);
    const uint32_t sb2 = c.staticSaveBlocks ? c.saveBlock2 : u32At(c.saveBlock2);
    const bool blocks = sb1 >= 0x02000000u && sb1 < 0x02040000u && sb2 >= 0x02000000u && sb2 < 0x02040000u;
    if (blocks)
    {
        uint8_t loc[2];
        if (read(sb1 + c.locationOff, 2, loc)) { s.mapGroup = loc[0]; s.mapNum = loc[1]; }
        const uint32_t key = c.keyOff >= 0 ? u32At(sb2 + c.keyOff) : 0;
        // (Money: Ruby and Sapphire's at SaveBlock1 + 0x490, Emerald's too; FireRed and LeafGreen's at 0x290.)
        s.money = (long long)(u32At(sb1 + (c.moneyOff >= 0 ? (uint32_t)c.moneyOff : c.pocketOrder == 'F' ? 0x290u : 0x490u)) ^ key);
        // (The Pokedex in SaveBlock2 + 0x18: owned at + 0x10, seen at + 0x44, 52 bytes each; a hack with more Pokemon
        // keeps longer arrays in SaveBlock1.)
        std::vector<uint8_t> caught(c.dexBytes), seen(c.dexBytes);
        const uint32_t dexBlock = c.dexInSb1 ? sb1 : sb2;
        if (c.dexBytes > 0 && read(dexBlock + c.dexCaughtOff, (size_t)c.dexBytes, caught.data()) && read(dexBlock + c.dexSeenOff, (size_t)c.dexBytes, seen.data()))
        {
            const int last = std::min(c.national, c.dexBytes * 8);
            s.caught.assign(last + 1, 0); s.seen.assign(last + 1, 0);
            for (int n = 1; n <= last; n++)
            {
                s.caught[n] = caught[(n - 1) / 8] >> ((n - 1) % 8) & 1;
                s.seen[n] = seen[(n - 1) / 8] >> ((n - 1) % 8) & 1;
            }
        }
        // (The bag: gBagPockets, each {its slots, how many}: slots {item, quantity ^ key's low half}.)
        static const char* const emerald[5] = { "ITEMS", "POKE BALLS", "TMS AND HMS", "BERRIES", "KEY ITEMS" };
        static const char* const firered[5] = { "ITEMS", "KEY ITEMS", "POKE BALLS", "TM CASE", "BERRY POUCH" };
        // (A hack's own pockets, by their names in gBagPockets' order.)
        if (c.bagBitsOff >= 0)
        {
            std::vector<uint8_t> bits((size_t)(c.bagBitsCount + 7) / 8);
            std::vector<Item> items;
            if (read(sb1 + (uint32_t)c.bagBitsOff, bits.size(), bits.data()))
                for (int i = 1; i < c.bagBitsCount; i++)
                    if (bits[i / 8] >> (i % 8) & 1) items.push_back({ i, 1 });
            s.pockets.push_back({ "ITEMS", items });
        }
        uint8_t pockets[10 * 8];
        const int count = c.pocketCount;
        if (count >= 1 && count <= 10 && (count == 5 || c.pocketNames) && read(c.bagPockets, (size_t)count * 8, pockets))
            for (int p = 0; p < count; p++)
            {
                const uint32_t slots = U32(pockets + p * 8);
                const int capacity = pockets[p * 8 + 4];
                std::vector<Item> items;
                uint8_t raw[255 * 4];
                if (slots >= 0x02000000u && slots < 0x02040000u && capacity && read(slots, capacity * 4u, raw))
                    for (int i = 0; i < capacity; i++)
                        if (U16(raw + i * 4)) items.push_back({ U16(raw + i * 4), U16(raw + i * 4 + 2) ^ (int)(key & 0xFFFF) });
                s.pockets.push_back({ c.pocketNames ? c.pocketNames[p] : c.pocketOrder == 'F' ? firered[p] : emerald[p], items });
            }
    }
    // (A battle: gMain's inBattle bit; the battlers by position, 0 yours and 1 the foe's on the left.)
    uint8_t flag = 0;
    if (c.gMain && read(c.gMain + c.inBattleOff, 1, &flag) && (flag & 2))
    {
        uint8_t pos[4] = {};
        const BattleLayout& L = *c.battle;
        std::vector<uint8_t> mons(4 * (size_t)L.size);
        if (read(c.battlerPositions, 4, pos) && read(c.battleMons, mons.size(), mons.data()))
            for (int b = 0; b < 4; b++)
            {
                if (pos[b] > 1) continue;
                const uint8_t* m = mons.data() + b * L.size;
                Battler& t = pos[b] == 0 ? s.mine : s.foe;
                t.species = U16(m + L.species) & (int)c.speciesMask; t.level = m[L.level]; t.hp = U16(m + L.hp); t.maxHp = U16(m + L.maxHp);
                t.type1 = MapType(m[L.type1], c.typeMode); t.type2 = MapType(m[L.type2], c.typeMode);
                t.ability = L.abilityU16 ? U16(m + L.ability) : m[L.ability]; t.item = U16(m + L.item);
                t.status = U32(m + L.status); t.personality = L.personality >= 0 ? U32(m + L.personality) : 0;
                for (int i = 0; i < 4; i++) { t.moves[i] = U16(m + L.moves + i * 2) & (int)c.moveMask; t.pp[i] = m[L.pp + i] & (int)c.ppMask; }
                for (int i = 0; i < 5; i++) t.stats[i] = U16(m + L.stats + i * 2);
                if (L.ivs >= 0)
                {
                    const uint32_t ivs = U32(m + L.ivs);
                    for (int i = 0; i < 6; i++) t.iv[i] = ivs >> (i * 5) & 31;
                }
                if (L.nickname >= 0) t.nickname.assign((const char*)m + L.nickname, 10);
            }
        s.inBattle = s.foe.species > 0;
        // (gEnemyParty: beside gPlayerParty, after it in Ruby, Sapphire and Emerald and before it in FireRed and
        // LeafGreen, 600 or 608 bytes away; the block holding the foe out is the one.)
        if (s.inBattle)
            for (int64_t d : { 600, -600, 608 })
            {
                const uint32_t at = c.enemyParty ? c.enemyParty : (uint32_t)((int64_t)c.party + d);
                uint8_t e[6 * 100];
                if (!read(at, 6u * c.monSize, e)) continue;
                std::vector<Mon> team;
                bool hasFoe = false;
                for (int i = 0; i < 6; i++)
                {
                    Mon m;
                    if (!DecodeMon(e + i * c.monSize, m, c) || !m.species) break;
                    hasFoe = hasFoe || m.personality == s.foe.personality;
                    team.push_back(m);
                }
                if (hasFoe || c.enemyParty) { if (hasFoe) s.foeParty = team; break; }
            }
    }
    s.ok = !s.party.empty() || blocks;
    return s;
}
}
