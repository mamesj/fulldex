#pragma once
// MOVE ADVICE (after PokeDaisy's MoveAdvice.kt, GPL-3.0; Based on PokeDaisy by Lidor Itzhari -
// https://github.com/lidor30/pokedaisy): whether a party Pokemon should learn a move, and in place of which.
// Its moveset is scored from the game's own move data: for attacks, for every defending type the best hit it has
// on it (power x accuracy, x1.5 for its own type, scaled by the stat the move runs on; Generation 3 splits
// physical and special by type), averaged over the types; status moves add a share by how strong their effect
// is, less for a second of a kind. The best swap is the one that raises the score most; under 10% it is a SKIP
// (30% to learn an HM, which can't be forgotten again). HMs are never offered for forgetting.
#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "companion_types.h"

namespace MoveAdvice
{
// Generation 3's move effects (pokeemerald's battle_move_effects.h), by number.
inline const char* EffectName(int e)
{
    static const char* const names[] = { "HIT", "SLEEP", "POISON_HIT", "ABSORB", "BURN_HIT", "FREEZE_HIT", "PARALYZE_HIT", "EXPLOSION", "DREAM_EATER", "MIRROR_MOVE", "ATTACK_UP", "DEFENSE_UP", "SPEED_UP", "SPECIAL_ATTACK_UP", "SPECIAL_DEFENSE_UP", "ACCURACY_UP", "EVASION_UP", "ALWAYS_HIT", "ATTACK_DOWN", "DEFENSE_DOWN", "SPEED_DOWN", "SPECIAL_ATTACK_DOWN", "SPECIAL_DEFENSE_DOWN", "ACCURACY_DOWN", "EVASION_DOWN", "HAZE", "BIDE", "RAMPAGE", "ROAR", "MULTI_HIT", "CONVERSION", "FLINCH_HIT", "RESTORE_HP", "TOXIC", "PAY_DAY", "LIGHT_SCREEN", "TRI_ATTACK", "REST", "OHKO", "RAZOR_WIND", "SUPER_FANG", "DRAGON_RAGE", "TRAP", "HIGH_CRITICAL", "DOUBLE_HIT", "RECOIL_IF_MISS", "MIST", "FOCUS_ENERGY", "RECOIL", "CONFUSE", "ATTACK_UP_2", "DEFENSE_UP_2", "SPEED_UP_2", "SPECIAL_ATTACK_UP_2", "SPECIAL_DEFENSE_UP_2", "ACCURACY_UP_2", "EVASION_UP_2", "TRANSFORM", "ATTACK_DOWN_2", "DEFENSE_DOWN_2", "SPEED_DOWN_2", "SPECIAL_ATTACK_DOWN_2", "SPECIAL_DEFENSE_DOWN_2", "ACCURACY_DOWN_2", "EVASION_DOWN_2", "REFLECT", "POISON", "PARALYZE", "ATTACK_DOWN_HIT", "DEFENSE_DOWN_HIT", "SPEED_DOWN_HIT", "SPECIAL_ATTACK_DOWN_HIT", "SPECIAL_DEFENSE_DOWN_HIT", "ACCURACY_DOWN_HIT", "EVASION_DOWN_HIT", "SKY_ATTACK", "CONFUSE_HIT", "TWINEEDLE", "VITAL_THROW", "SUBSTITUTE", "RECHARGE", "RAGE", "MIMIC", "METRONOME", "LEECH_SEED", "SPLASH", "DISABLE", "LEVEL_DAMAGE", "PSYWAVE", "COUNTER", "ENCORE", "PAIN_SPLIT", "SNORE", "CONVERSION_2", "LOCK_ON", "SKETCH", "UNUSED_60", "SLEEP_TALK", "DESTINY_BOND", "FLAIL", "SPITE", "FALSE_SWIPE", "HEAL_BELL", "QUICK_ATTACK", "TRIPLE_KICK", "THIEF", "MEAN_LOOK", "NIGHTMARE", "MINIMIZE", "CURSE", "UNUSED_6E", "PROTECT", "SPIKES", "FORESIGHT", "PERISH_SONG", "SANDSTORM", "ENDURE", "ROLLOUT", "SWAGGER", "FURY_CUTTER", "ATTRACT", "RETURN", "PRESENT", "FRUSTRATION", "SAFEGUARD", "THAW_HIT", "MAGNITUDE", "BATON_PASS", "PURSUIT", "RAPID_SPIN", "SONICBOOM", "UNUSED_83", "MORNING_SUN", "SYNTHESIS", "MOONLIGHT", "HIDDEN_POWER", "RAIN_DANCE", "SUNNY_DAY", "DEFENSE_UP_HIT", "ATTACK_UP_HIT", "ALL_STATS_UP_HIT", "UNUSED_8D", "BELLY_DRUM", "PSYCH_UP", "MIRROR_COAT", "SKULL_BASH", "TWISTER", "EARTHQUAKE", "FUTURE_SIGHT", "GUST", "FLINCH_MINIMIZE_HIT", "SOLAR_BEAM", "THUNDER", "TELEPORT", "BEAT_UP", "SEMI_INVULNERABLE", "DEFENSE_CURL", "SOFTBOILED", "FAKE_OUT", "UPROAR", "STOCKPILE", "SPIT_UP", "SWALLOW", "UNUSED_A3", "HAIL", "TORMENT", "FLATTER", "WILL_O_WISP", "MEMENTO", "FACADE", "FOCUS_PUNCH", "SMELLINGSALT", "FOLLOW_ME", "NATURE_POWER", "CHARGE", "TAUNT", "HELPING_HAND", "TRICK", "ROLE_PLAY", "WISH", "ASSIST", "INGRAIN", "SUPERPOWER", "MAGIC_COAT", "RECYCLE", "REVENGE", "BRICK_BREAK", "YAWN", "KNOCK_OFF", "ENDEAVOR", "ERUPTION", "SKILL_SWAP", "IMPRISON", "REFRESH", "GRUDGE", "SNATCH", "LOW_KICK", "SECRET_POWER", "DOUBLE_EDGE", "TEETER_DANCE", "BLAZE_KICK", "MUD_SPORT", "POISON_FANG", "WEATHER_BALL", "OVERHEAT", "TICKLE", "COSMIC_POWER", "SKY_UPPERCUT", "BULK_UP", "POISON_TAIL", "WATER_SPORT", "CALM_MIND", "DRAGON_DANCE", "CAMOUFLAGE" };
    return e >= 0 && e < (int)(sizeof names / sizeof *names) ? names[e] : "";
}
struct MoveData { bool ok = false; int effect = 0, power = 0, type = -1, accuracy = 0, chance = 0, priority = 0; };
struct Mon { std::vector<int> types; int attack = 0, spAttack = 0, level = 1; std::vector<int> moves; };
enum Verdict { LEARN, REPLACE, SKIP, KNOWN, UNKNOWN };
struct Advice { Verdict verdict = UNKNOWN; int forget = 0; std::string why; float gain = 0; };
// The original games' HMs: CUT, FLY, SURF, STRENGTH, FLASH, ROCK SMASH, WATERFALL, DIVE.
inline const std::set<int>& Hms() { static const std::set<int> s = { 15, 19, 57, 70, 148, 249, 127, 291 }; return s; }

struct Advisor
{
    std::function<MoveData(int)> data;
    static constexpr float MIN_GAIN = 0.1f, MIN_GAIN_HM = 0.3f, TIE = 0.01f, TIER_SHARE = 0.08f, BASELINE = 30.0f;
    static bool Is(const MoveData& d, const char* e) { return !strcmp(EffectName(d.effect), e); }
    static bool Physical(int type) { return type < 9; } // (Normal ... Steel; Fire ... Dark are special)
    float StatShare(const Mon& m, int type) const
    {
        const int best = std::max(m.attack, m.spAttack);
        if (m.attack <= 0 || m.spAttack <= 0 || best <= 0) return 1.0f;
        return std::max(0.4f, (float)(Physical(type) ? m.attack : m.spAttack) / best);
    }
    // A hit's worth to this Pokemon before the type chart; < 0: a status move.
    float AttackValue(const Mon& m, int id) const
    {
        const MoveData d = data(id);
        if (!d.ok || d.power == 0) return -1;
        float p = (float)d.power;
        bool own = true;
        const std::string e = EffectName(d.effect);
        if (e == "MULTI_HIT") p *= 3;
        else if (e == "DOUBLE_HIT" || e == "TWINEEDLE") p *= 2;
        else if (e == "TRIPLE_KICK") p = 50;
        else if (e == "OHKO") { p = 90; own = false; }
        else if (e == "LEVEL_DAMAGE") { p = (float)std::clamp(m.level, 10, 100); own = false; }
        else if (e == "SONICBOOM" || e == "DRAGON_RAGE") { p = 35; own = false; }
        else if (e == "SUPER_FANG" || e == "PSYWAVE" || e == "ENDEAVOR" || e == "COUNTER" || e == "MIRROR_COAT" || e == "BIDE") { p = 40; own = false; }
        else if (e == "RETURN" || e == "FRUSTRATION") p = 70;
        else if (e == "FLAIL" || e == "LOW_KICK" || e == "MAGNITUDE" || e == "WEATHER_BALL" || e == "SPIT_UP" || e == "BEAT_UP" || e == "PRESENT") p = 55;
        else if (e == "HIDDEN_POWER") p = 50;
        else if (e == "ERUPTION") p *= 0.7f;
        else if (e == "RECHARGE" || e == "RAZOR_WIND" || e == "SKULL_BASH" || e == "SKY_ATTACK" || e == "SOLAR_BEAM" || e == "FOCUS_PUNCH") p *= 0.6f;
        else if (e == "SEMI_INVULNERABLE" || e == "RAMPAGE") p *= 0.85f;
        else if (e == "RECOIL" || e == "DOUBLE_EDGE" || e == "RECOIL_IF_MISS") p *= 0.9f;
        else if (e == "OVERHEAT" || e == "SUPERPOWER") p *= 0.8f;
        else if (e == "EXPLOSION") p *= 0.25f;
        else if (e == "FAKE_OUT" || e == "SNORE" || e == "DREAM_EATER") p *= 0.4f;
        else if (e == "FALSE_SWIPE" || e == "FUTURE_SIGHT") p *= 0.75f;
        else if (e == "ROLLOUT" || e == "FURY_CUTTER") p *= 1.5f;
        const float acc = d.accuracy == 0 ? 1.0f : d.accuracy / 100.0f;
        if (!own) return p * acc;
        const bool stab = std::find(m.types.begin(), m.types.end(), d.type) != m.types.end();
        return p * acc * (stab ? 1.5f : 1.0f) * StatShare(m, d.type) * (d.priority > 0 ? 1.1f : 1.0f);
    }
    static std::vector<int> Defending() { std::vector<int> t; for (int i = 0; i < 18; i++) if (i != 9 && CompanionTypes::Valid(i)) t.push_back(i); return t; }
    float AttackScore(const Mon& m, const std::vector<int>& moves) const
    {
        std::vector<std::pair<int, float>> hits;
        for (int id : moves) { const float v = AttackValue(m, id); if (v >= 0) hits.push_back({ data(id).type, v }); }
        if (hits.empty()) return 0;
        const auto defs = Defending();
        double sum = 0;
        for (int def : defs)
        {
            double best = 0;
            for (const auto& h : hits) best = std::max(best, h.second * CompanionTypes::Multiplier(h.first, def, def) / 100.0);
            sum += best;
        }
        return (float)(sum / defs.size());
    }
    enum Group { INFLICT, SETUP_ATK, SETUP_SPA, SETUP_OTHER, HEAL, SHIELD, DEBUFF, WEATHER, OTHER };
    static std::pair<Group, float> StatusTier(const std::string& e)
    {
        auto in = [&](std::initializer_list<const char*> l) { for (const char* s : l) if (e == s) return true; return false; };
        if (in({ "SLEEP", "PARALYZE", "TOXIC" })) return { INFLICT, 3 };
        if (in({ "WILL_O_WISP", "POISON", "YAWN", "LEECH_SEED" })) return { INFLICT, 2 };
        if (in({ "CONFUSE", "ATTRACT", "SWAGGER", "FLATTER", "TEETER_DANCE" })) return { INFLICT, 1 };
        if (in({ "ATTACK_UP_2", "DRAGON_DANCE", "BULK_UP", "BELLY_DRUM", "CURSE" })) return { SETUP_ATK, 3 };
        if (e == "ATTACK_UP") return { SETUP_ATK, 1.5f };
        if (in({ "CALM_MIND", "SPECIAL_ATTACK_UP_2" })) return { SETUP_SPA, 3 };
        if (e == "SPECIAL_ATTACK_UP") return { SETUP_SPA, 1.5f };
        if (in({ "SPEED_UP_2", "COSMIC_POWER", "DEFENSE_UP_2", "SPECIAL_DEFENSE_UP_2", "EVASION_UP", "MINIMIZE" })) return { SETUP_OTHER, 2 };
        if (in({ "DEFENSE_UP", "DEFENSE_CURL", "FOCUS_ENERGY", "CHARGE", "STOCKPILE" })) return { SETUP_OTHER, 1 };
        if (in({ "RESTORE_HP", "SOFTBOILED", "MOONLIGHT", "MORNING_SUN", "SYNTHESIS", "WISH" })) return { HEAL, 3 };
        if (e == "PAIN_SPLIT") return { HEAL, 2 };
        if (e == "REST") return { HEAL, 1.5f };
        if (in({ "INGRAIN", "SWALLOW" })) return { HEAL, 1 };
        if (in({ "SUBSTITUTE", "REFLECT", "LIGHT_SCREEN" })) return { SHIELD, 2 };
        if (e == "PROTECT") return { SHIELD, 1.5f };
        if (in({ "ENDURE", "SAFEGUARD", "MIST" })) return { SHIELD, 0.7f };
        if (in({ "ATTACK_DOWN_2", "DEFENSE_DOWN_2", "SPEED_DOWN_2", "SPECIAL_DEFENSE_DOWN_2" })) return { DEBUFF, 1.5f };
        if (e == "ACCURACY_DOWN") return { DEBUFF, 1 };
        if (in({ "ATTACK_DOWN", "DEFENSE_DOWN", "SPEED_DOWN", "EVASION_DOWN", "TICKLE" })) return { DEBUFF, 0.5f };
        if (in({ "RAIN_DANCE", "SUNNY_DAY", "SANDSTORM", "HAIL" })) return { WEATHER, 1 };
        if (in({ "HEAL_BELL", "BATON_PASS", "TAUNT", "ENCORE", "DISABLE", "SPIKES", "HAZE", "PERISH_SONG", "DESTINY_BOND" })) return { OTHER, 1 };
        if (in({ "SPLASH", "TELEPORT", "WATER_SPORT", "MUD_SPORT", "CAMOUFLAGE", "CONVERSION", "CONVERSION_2", "SKETCH", "FOLLOW_ME", "HELPING_HAND",
                 "GRUDGE", "SNATCH", "IMPRISON", "RECYCLE", "ROLE_PLAY", "NIGHTMARE" })) return { OTHER, 0 };
        return { OTHER, 0.5f };
    }
    std::pair<Group, float> TierFor(const Mon& m, const std::string& e) const
    {
        auto t = StatusTier(e);
        if (m.attack > 0 && m.spAttack > 0)
        {
            if (t.first == SETUP_ATK && m.attack < m.spAttack * 0.8f) t.second = 0.5f;
            if (t.first == SETUP_SPA && m.spAttack < m.attack * 0.8f) t.second = 0.5f;
        }
        return t;
    }
    static bool SideEffect(const std::string& e, std::pair<Group, float>& out)
    {
        auto in = [&](std::initializer_list<const char*> l) { for (const char* s : l) if (e == s) return true; return false; };
        if (in({ "PARALYZE_HIT", "THUNDER", "FREEZE_HIT", "POISON_FANG" })) { out = { INFLICT, 3 }; return true; }
        if (in({ "BURN_HIT", "BLAZE_KICK", "POISON_HIT", "POISON_TAIL", "TRI_ATTACK" })) { out = { INFLICT, 2 }; return true; }
        if (e == "CONFUSE_HIT") { out = { INFLICT, 1 }; return true; }
        if (in({ "FLINCH_HIT", "FLINCH_MINIMIZE_HIT" })) { out = { OTHER, 1 }; return true; }
        if (in({ "ATTACK_DOWN_HIT", "DEFENSE_DOWN_HIT", "SPEED_DOWN_HIT", "SPECIAL_ATTACK_DOWN_HIT", "SPECIAL_DEFENSE_DOWN_HIT", "ACCURACY_DOWN_HIT" })) { out = { DEBUFF, 1 }; return true; }
        if (in({ "ATTACK_UP_HIT", "DEFENSE_UP_HIT", "ALL_STATS_UP_HIT" })) { out = { SETUP_OTHER, 1 }; return true; }
        return false;
    }
    bool EffectTier(const Mon& m, int id, std::pair<Group, float>& out) const
    {
        const MoveData d = data(id);
        if (!d.ok) return false;
        if (d.power == 0) { out = TierFor(m, EffectName(d.effect)); return true; }
        if (!SideEffect(EffectName(d.effect), out)) return false;
        out.second *= std::clamp(d.chance, 0, 100) / 100.0f;
        return true;
    }
    float StatusScore(const Mon& m, const std::vector<int>& moves, float ref) const
    {
        std::vector<std::pair<Group, float>> tiers;
        for (int id : moves) { std::pair<Group, float> t; if (EffectTier(m, id, t)) tiers.push_back(t); }
        std::sort(tiers.begin(), tiers.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        std::map<int, int> seen;
        double sum = 0;
        for (const auto& t : tiers) sum += t.second * (++seen[t.first] == 1 ? 1.0f : 0.3f);
        return (float)sum * TIER_SHARE * ref;
    }
    float Score(const Mon& m, const std::vector<int>& moves, float ref) const { return AttackScore(m, moves) + StatusScore(m, moves, ref); }
    bool IsStatus(int id) const { return data(id).power == 0; }
    std::set<int> SuperTypes(const Mon& m, const std::vector<int>& moves) const
    {
        std::set<int> out;
        for (int def : Defending())
            for (int id : moves)
                if (AttackValue(m, id) >= 0 && CompanionTypes::Multiplier(data(id).type, def, def) > 100) { out.insert(def); break; }
        return out;
    }
    std::string WhyNew(const Mon& m, int move, int out, const std::vector<int>& current) const
    {
        const MoveData d = data(move);
        std::vector<int> after = current;
        if (out) std::replace(after.begin(), after.end(), out, move); else after.push_back(move);
        if (!IsStatus(move))
        {
            const auto now = SuperTypes(m, current), then = SuperTypes(m, after);
            for (int t : then) if (!now.count(t)) return "NEW SUPER EFFECTIVE HITS";
            if (out && !IsStatus(out) && data(out).type == d.type && d.power > data(out).power) return "STRONGER, SAME TYPE";
            if (std::find(m.types.begin(), m.types.end(), d.type) != m.types.end()) return "ITS OWN TYPE: x1.5";
            return "HITS HARDER OVERALL";
        }
        return TierFor(m, EffectName(d.effect)).second >= 2 ? "STRONG EFFECT" : "A USEFUL EFFECT";
    }
    std::string WhySkip(const Mon& m, int move, const std::vector<int>& current) const
    {
        const MoveData d = data(move);
        if (!IsStatus(move))
        {
            const float mine = std::max(0.0f, AttackValue(m, move));
            for (int id : current)
                if (!IsStatus(id) && data(id).type == d.type && AttackValue(m, id) >= mine) return "A MOVE IT KNOWS IS BETTER";
            if (StatShare(m, d.type) < 0.8f) return "RUNS ON ITS WEAKER STAT";
            if (d.accuracy >= 1 && d.accuracy <= 84) return "LOW ACCURACY";
        }
        else if (TierFor(m, EffectName(d.effect)).second <= 1) return "WEAK EFFECT";
        return "ITS MOVES ARE BETTER";
    }
    Advice Advise(const Mon& m, int move) const
    {
        Advice a;
        if (!data(move).ok) return a;
        for (int id : m.moves) if (id && !data(id).ok) return a;
        if (std::find(m.moves.begin(), m.moves.end(), move) != m.moves.end()) { a.verdict = KNOWN; return a; }
        std::vector<int> current;
        for (int id : m.moves) if (id) current.push_back(id);
        float ref = AttackScore(m, current);
        if (ref <= 0) ref = BASELINE;
        const float now = Score(m, current, ref);
        if (current.size() < 4)
        {
            std::vector<int> with = current; with.push_back(move);
            a.verdict = LEARN; a.gain = (Score(m, with, ref) - now) / ref; a.why = "A FREE SLOT";
            return a;
        }
        std::vector<std::pair<int, float>> options;
        for (int out : current)
        {
            if (Hms().count(out)) continue;
            std::vector<int> swapped = current;
            std::replace(swapped.begin(), swapped.end(), out, move);
            options.push_back({ out, Score(m, swapped, ref) });
        }
        const std::pair<int, float>* best = nullptr;
        if (!options.empty())
        {
            float top = options[0].second;
            for (const auto& o : options) top = std::max(top, o.second);
            const int newType = data(move).type;
            for (const auto& o : options)
            {
                if ((top - o.second) / ref > TIE) continue;
                if (!best) { best = &o; continue; }
                auto rank = [&](int id) { return !IsStatus(id) && data(id).type == newType ? 0 : 1; };
                auto val = [&](int id) { const float v = AttackValue(m, id); return v < 0 ? 1e9f : v; };
                if (rank(o.first) < rank(best->first) || (rank(o.first) == rank(best->first) && val(o.first) < val(best->first))) best = &o;
            }
        }
        a.gain = best ? (best->second - now) / ref : 0;
        const bool hm = Hms().count(move) != 0;
        if (!best || a.gain < (hm ? MIN_GAIN_HM : MIN_GAIN))
        {
            a.verdict = SKIP;
            a.why = hm && a.gain >= MIN_GAIN ? "AN HM: IT STAYS FOR GOOD" : WhySkip(m, move, current);
            return a;
        }
        a.verdict = REPLACE; a.forget = best->first; a.why = WhyNew(m, move, best->first, current);
        return a;
    }
};
}
