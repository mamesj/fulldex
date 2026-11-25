#include "folders.h"
#include "memory_link.h"
// MANAGE SAVES (a game's menu): one game's saves on a page of their own, the Hub's page 3 (HubMode
// 3), its list on the left and its save states on the right (below, on an upright phone).
//  - Game Save: when it was saved, its size, whether it looks right (Health) and how it stands with
//    the cloud; A on it: sync now, choose which save to keep, or go back to the newest good backup.
//  - Restore A Backup (the backups folder beside the saves, as the game makes them), Export Save (a
//    copy in the exports folder), Import Save (from the import folder, or a file chosen on Windows),
//    Memory Link (Black 2 and White 2: a Black or White save's memories, memory_link.h),
//    Play From Quick Save, Undo Delete (a slot deleted here), Open Saves Folder, Delete Save.
//  - The ten slots of the game's SAVE and LOAD pages as cards, with their pictures: Play From Slot
//    starts the game straight from one (--state-load), Delete Slot keeps it as .deleted until Undo.
// Nothing here changes a ROM. A save is backed up before anything replaces or deletes it, and a
// save changed here goes up to the cloud at once when cloud saves are on (a deleted one is taken
// out of the cloud too: Google Drive keeps it in its bin). Save states stay on each device.
// The platform does what only it can (Saves().request): open a folder, choose a file (Windows),
// start the game from a state (PlayState). Included by launcher.h, inside namespace Launcher.

enum { SV_SAVE, SV_RESTORE, SV_EXPORT, SV_IMPORT, SV_QUICK, SV_UNDO, SV_FOLDER, SV_DELETE, SV_LINK };
struct SaveHealth { int state = 0; std::string label, why; }; // 0 no save yet, 1 looks right, 2 a problem
struct SavesState
{
    std::string rom, name, save, cloudKey; // the game's ROM and name, its save, that save as cloud saves name it
    bool ds = true;
    bool link = false;      // Black 2 or White 2: the Memory Link row
    size_t want = 0;        // the save's size when not a DS or GBA game's (a 3DS game's: X and Y 405 KB, Omega Ruby and Alpha Sapphire 472 KB, Ultra Sun and Ultra Moon 435 KB)
    std::string deleted;    // the slot deleted last (kept as <slot>.deleted until Undo Delete)
    std::string request;    // for the platform: "#folder:<dir>", "#play:<state>", "#replay" ("" nothing)
    std::string msg;        // what the last thing done here came to
    std::string playAnyway; // a ROM played once more without the save check (Play Anyway)
    // (The slots as drawn, for taps and the arrows: columns, where the first card is, a card's size.)
    int cols = 5, gx = 0, gy = 0, cw = 0, ch = 0, cg = 0, firstRow = 0, visRows = 2;
    bool stacked = false;
    Glass::Picture thumb[10];
    long long thumbAt[10] = {};
    long long healthAt = -2; SaveHealth health; // (worked out again when the save changes)
    long long linkAt = -2; std::string linkSub;  // (the Memory Link row's line, the same way)
};
inline SavesState& Saves() { static SavesState s; return s; }
inline bool SavesDs() { return Saves().ds; }
// The state Play From Slot starts a game from (taken by the platform's launch, then cleared).
inline std::string& PlayState() { static std::string s; return s; }

inline long long SvTime(const std::string& p)
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(std::filesystem::u8path(p), ec);
    return ec ? 0 : (long long)t.time_since_epoch().count();
}
inline bool SvExists(const std::string& p) { std::error_code ec; return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); }
inline long long SvSize(const std::string& p) { std::error_code ec; const auto n = std::filesystem::file_size(std::filesystem::u8path(p), ec); return ec ? -1 : (long long)n; }
inline std::string SvDir(const std::string& save) { return std::filesystem::u8path(save).parent_path().u8string(); }
inline std::string SvKB(long long n) { return n >= 1024 ? std::to_string(n / 1024) + " KB" : std::to_string(n) + " bytes"; }
inline std::string SavesStatePath() { return FolderSet::StateOf(Saves().save); } // (FOLDERS)
inline std::string SavesSlot(int i) { return StateSlots::Path(SavesStatePath(), i); }
// "4 Nov 2026, 12:44" from a backup's "2026-11-04 12:44:55".
inline std::string SvBackupWhen(const std::string& l)
{
    static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    if (l.size() < 16) return l;
    const int m = std::max(1, std::min(12, atoi(l.substr(5, 2).c_str())));
    return std::to_string(atoi(l.substr(8, 2).c_str())) + " " + months[m - 1] + " " + l.substr(0, 4) + ", " + l.substr(11, 5);
}

// Whether a save looks right for the game: the size it uses (DS Pokemon: 512 KB; GBA: 128 KB, and
// 16 bytes more with a clock), and not blank. (What is in it is the game's to check.)
inline SaveHealth SaveHealthOfData(const std::vector<unsigned char>& d, bool ds, bool exists, size_t own = 0)
{
    SaveHealth h;
    if (d.empty())
    {
        if (exists) { h.state = 2; h.label = "Problem"; h.why = "It is empty or cannot be read."; }
        else h.label = "No Save Yet";
        return h;
    }
    const size_t want = own ? own : ds ? 524288 : 131072;
    if (d.size() != want && !(!ds && !own && d.size() == want + 16))
    {
        h.state = 2; h.label = "Problem";
        h.why = "It is " + SvKB((long long)d.size()) + ", but this game's saves are " + SvKB((long long)want) + ".";
        return h;
    }
    bool ff = true, zero = true;
    for (size_t i = 0; i < want && (ff || zero); i++) { ff = ff && d[i] == 0xFF; zero = zero && d[i] == 0; }
    if (ff || zero) { h.state = 2; h.label = "Problem"; h.why = "It is blank: nothing has been saved in it."; return h; }
    h.state = 1; h.label = "Healthy"; h.why = "It is the size this game uses, with a game saved in it.";
    return h;
}
inline SaveHealth SaveHealthOf(const std::string& save, bool ds, size_t own = 0)
{
    SaveHealth h = SaveHealthOfData(SaveBackups::Read(save), ds, SvExists(save), own);
    // (Blank, and no backup ever had a game in it: a new game not saved yet (a GBA game makes its
    // blank save at once), not a lost one, so no "Problem" and no question before Play.)
    if (h.state == 2 && h.why.rfind("It is blank", 0) == 0)
    {
        for (const SaveBackups::Entry& e : SaveBackups::List(save))
            if (SaveHealthOfData(SaveBackups::Read(e.path), ds, true, own).state == 1) return h;
        h = SaveHealth();
        h.label = "No Save Yet";
    }
    return h;
}
inline const SaveHealth& SavesHealth()
{
    SavesState& s = Saves();
    const long long at = SvTime(s.save) ^ SvSize(s.save);
    if (at != s.healthAt) { s.healthAt = at; s.health = SaveHealthOf(s.save, s.ds, s.want); }
    return s.health;
}
// The newest backup that looks right ("" none), and when it was made.
inline std::string SavesBestBackup(std::string& when)
{
    for (const SaveBackups::Entry& e : SaveBackups::List(Saves().save))
        if (SaveHealthOf(e.path, Saves().ds, Saves().want).state == 1) { when = SvBackupWhen(e.label); return e.path; }
    return "";
}
// A copy of the save in the backups folder, named as the game names its own (so Restore A Backup
// lists it): <name>-YYYYMMDD-HHMMSS.sav. "" if it could not be made (or there is no save).
inline std::string SavesBackUp(const std::string& save)
{
    namespace fs = std::filesystem;
    const std::vector<unsigned char> d = SaveBackups::Read(save);
    if (d.empty()) return "";
    const fs::path p = fs::u8path(save), dir = p.parent_path() / "backups";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const time_t t = time(nullptr);
    std::tm lt{};
    if (const std::tm* p = std::localtime(&t)) lt = *p;
    char when[32];
    strftime(when, sizeof when, "%Y%m%d-%H%M%S", &lt);
    const std::string out = (dir / fs::u8path(p.stem().u8string() + "-" + when + ".sav")).u8string();
    std::string error;
    if (SvExists(out)) return out; // (made this second: the same save)
    if (FILE* f = fopen(out.c_str(), "wb"))
    {
        const bool ok = fwrite(d.data(), 1, d.size(), f) == d.size();
        if (fclose(f) == 0 && ok) return out;
    }
    remove(out.c_str());
    return "";
}

// The cloud, for this game's save: 0 off, 1 synced, 2 different from the cloud's, 3 not synced
// yet (or not reached), 4 syncing.
inline int SavesCloud(std::string& label)
{
    HubLoad();
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (!HubOnLocked(h)) { label = "Cloud Off"; return 0; }
    if (h.busy) { label = "Syncing"; return 4; }
    const auto it = h.results.find(Saves().cloudKey);
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Conflict) { label = "Different"; return 2; }
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Failed) { label = "Not Reached"; return 3; }
    if (h.synced.count(std::filesystem::u8path(Saves().cloudKey).filename().u8string())) { label = "Synced"; return 1; }
    label = "Not Synced"; return 3;
}
inline bool SavesCloudOn() { HubLoad(); HubState& h = Hub(); std::lock_guard<std::mutex> l(h.m); return HubOnLocked(h); }
inline std::string SavesProvider() { HubLoad(); HubState& h = Hub(); std::lock_guard<std::mutex> l(h.m); return HubProvider(h.set.provider); }
// Sync this save now (resolve -1), or keep this device's (0) or the cloud's (1).
inline bool SavesSync(int resolve)
{
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (h.busy || !HubOnLocked(h)) return false;
    h.busy = 2; h.cancel = false; h.msg = "Syncing..."; h.version++;
    const std::vector<std::string> paths = { Saves().cloudKey };
    HubStart([paths, resolve] { HubSyncJob(paths, resolve); });
    return true;
}
// (On the worker: the save taken out of the cloud, after it was deleted here.)
inline void SavesForgetJob(std::string path)
{
    HubState& h = Hub();
    CloudSaves::Settings s;
    std::string dir;
    { std::lock_guard<std::mutex> l(h.m); s = h.set; dir = h.dir; }
    std::unique_ptr<CloudSaves::Sync> sync = CloudSaves::FromSettings(s, dir + "/cloud-state.txt", HubDevice());
    CloudSaves::Result r;
    if (sync) r = sync->Forget(path);
    std::lock_guard<std::mutex> l(h.m);
    h.busy = 0;
    h.results.erase(path);
    HubReadState(h);
    h.msg = !sync ? "Deleted here. Cloud saves are not set up."
          : r.outcome == CloudSaves::Outcome::Failed ? "Deleted here, but not in the cloud: " + r.message + ". Delete it again when it can be reached."
          : "Deleted here and in " + std::string(HubProvider(s.provider)) + ".";
    h.version++;
}
inline void SavesSay(const std::string& m) { Saves().msg = m; Hub().version++; }

// Fills in the game the page is for (also for the check before Play, from the game list).
inline void SavesFor(const Game& g, const std::string& save)
{
    SavesState& s = Saves();
    if (s.rom != g.rom || s.save != save) { s.deleted.clear(); for (long long& t : s.thumbAt) t = -1; s.healthAt = -2; s.linkAt = -2; }
    s.rom = g.rom; s.name = ShortName(g); s.save = save; s.cloudKey = HubSavePath(g); s.ds = g.platform == DS; s.want = g.platform == N3DS ? (g.code == "EKJA" || g.code == "EK2A" ? 0x65600 : g.code == "A2AA" || g.code == "A2BA" ? 0x6CC00 : 0x76000) : 0;
    s.link = g.platform == DS && (g.code.rfind("IRE", 0) == 0 || g.code.rfind("IRD", 0) == 0);
}
// Manage Saves chosen in a game's menu: its page.
inline void OpenSaves(const Game& g, const std::string& save)
{
    SavesFor(g, save);
    SavesState& s = Saves();
    s.request.clear(); s.msg.clear(); s.firstRow = 0;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(SvDir(save) + "/import"), ec); // (where Import Save looks)
    GlassFocus& F = Focus();
    F.page = SIDE_HUB; F.side = -1; F.popup = -1; F.banner = 0; F.prow = 0; F.pact = -1; F.rScroll = 0; F.nScroll = 0; F.dScroll = 0;
    Mover().open = false;
    HubMode() = 3;
    HubLoad();
    { HubState& h = Hub(); std::lock_guard<std::mutex> l(h.m); if (!h.busy) h.msg.clear(); }
    Hub().version++;
}

inline std::vector<int> SavesRowIds()
{
    const SavesState& s = Saves();
    std::vector<int> r = { SV_SAVE, SV_RESTORE, SV_EXPORT, SV_IMPORT };
    if (s.link) r.push_back(SV_LINK);
    if (s.ds && StateSlots::Exists(SavesStatePath())) r.push_back(SV_QUICK);
    if (!s.deleted.empty() && SvExists(s.deleted + ".deleted")) r.push_back(SV_UNDO);
    r.push_back(SV_FOLDER);
    r.push_back(SV_DELETE);
    return r;
}
inline int SavesRows() { return (int)SavesRowIds().size(); }
inline int SavesRowId(int row) { const std::vector<int> r = SavesRowIds(); return row >= 0 && row < (int)r.size() ? r[row] : -1; }
inline bool SavesCanPick()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}
inline void SavesDialog(int d, const std::string& t, const std::string& m, std::vector<std::string> items, std::vector<std::string> paths, int first)
{
    GlassFocus& F = Focus();
    F.dTitle = t; F.dMsg = m; F.dItems = std::move(items); F.dPaths = std::move(paths); F.dialog = d; F.drow = first;
}
// The files Import Save offers: the import folder's .sav and .dsv.
inline std::vector<std::string> SavesImports()
{
    std::vector<std::string> r;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(std::filesystem::u8path(SvDir(Saves().save) + "/import"), ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec)) continue;
        std::string e = it->path().extension().u8string();
        for (char& ch : e) ch = (char)tolower((unsigned char)ch);
        if (e == ".sav" || e == ".dsv") r.push_back(it->path().u8string());
    }
    std::sort(r.begin(), r.end());
    return r;
}
inline std::string SvFile(const std::string& p) { const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1); }
// MEMORY LINK: the Black and White saves the row can read (the games' own, any in the saves folder
// whose ROM is not listed, and any in the import folder), each said as who played it.
struct SvLinkFrom { std::string label, path; };
inline std::vector<SvLinkFrom> SavesLinkSources()
{
    std::vector<std::pair<std::string, std::string>> files; // (where it is from, its path)
    auto bw = [](const std::string& code) { return code.rfind("IRB", 0) == 0 || code.rfind("IRA", 0) == 0; };
    for (const HubGame& g : HubGames()) if (g.g->platform == DS && bw(g.g->code)) files.push_back({ g.name, g.path });
    for (const KnownGame& k : KnownGames())
        if (bw(k.code)) { std::string n = k.name; if (n.rfind("Pokemon ", 0) == 0) n = n.substr(8); files.push_back({ n, SvDir(Saves().save) + "/" + k.save }); }
    for (const std::string& f : SavesImports()) files.push_back({ SvFile(f), f });
    std::vector<SvLinkFrom> r;
    std::vector<std::string> seen;
    for (const auto& f : files)
    {
        const std::string key = std::filesystem::u8path(f.second).lexically_normal().u8string();
        if (r.size() >= 6 || std::find(seen.begin(), seen.end(), key) != seen.end() || !SvExists(f.second)) continue;
        seen.push_back(key);
        MemoryLink::Source src;
        std::string why;
        if (!MemoryLink::ReadBlackWhite(SaveBackups::Read(f.second), src, why)) continue;
        const std::string v = MemoryLink::VersionName(src.version);
        r.push_back({ src.name + "'s " + v + (f.first == v ? std::string() : "  \xC2\xB7  " + f.first), f.second });
    }
    return r;
}
// Who a save is linked with: "Hilbert's Black" ("" nobody).
inline std::string SavesLinkedWith(const std::vector<unsigned char>& save)
{
    std::string with;
    for (const MemoryLink::Player& p : MemoryLink::LinkedWith(save)) with += (with.empty() ? "" : " and ") + p.name + "'s " + MemoryLink::VersionName(p.version);
    return with;
}
inline const std::string& SavesLinkSub()
{
    SavesState& s = Saves();
    const long long at = SvTime(s.save) ^ SvSize(s.save);
    if (at != s.linkAt)
    {
        s.linkAt = at;
        const std::string with = SavesLinkedWith(SaveBackups::Read(s.save));
        s.linkSub = with.empty() ? "A Black or White save's memories, as the Unova Link brings them" : "Linked with " + with;
    }
    return s.linkSub;
}

// A on a row of the page.
inline bool SavesEnter(int row)
{
    SavesState& s = Saves();
    const int id = SavesRowId(row);
    const bool has = SvExists(s.save), cloud = SavesCloudOn();
    if (id == SV_SAVE)
    {
        const SaveHealth& h = SavesHealth();
        std::string cl;
        const int cs = SavesCloud(cl);
        std::string msg = !has ? "There is no save for " + s.name + " yet: the game makes one the first time you save in it."
                               : "Saved " + StateSlots::Friendly(s.save) + ", " + SvKB(SvSize(s.save)) + ". " + h.why;
        std::vector<std::string> items, paths;
        if (h.state == 2)
        {
            std::string when;
            const std::string best = SavesBestBackup(when);
            if (!best.empty()) { items.push_back("Restore " + when); paths.push_back("#restore:" + best); msg += " The backup from " + when + " looks right."; }
        }
        if (cs == 2)
        {
            msg += " The save here and the one in the cloud have both changed since they last synced: choose which to keep. The other is kept in the backups.";
            items.push_back("Keep This One"); paths.push_back("#keepthis");
            items.push_back("Keep The Cloud's"); paths.push_back("#keepcloud");
        }
        else if (cs == 1 || cs == 3) { msg += cs == 1 ? " It is synced with " + SavesProvider() + "." : " It has not synced with " + SavesProvider() + " yet."; items.push_back("Sync Now"); paths.push_back("#sync"); }
        else if (cs == 4) msg += " Syncing now.";
        items.push_back("Close"); paths.push_back("#close");
        SavesDialog(17, "Game Save", msg, items, paths, 0);
    }
    else if (id == SV_RESTORE)
    {
        const std::vector<SaveBackups::Entry> list = SaveBackups::List(s.save);
        if (list.empty()) { SavesSay("No backups yet: one is made every time the game starts."); return true; }
        std::vector<std::string> items, paths;
        for (size_t k = 0; k < list.size() && k < 6; k++)
        {
            const SaveHealth bh = SaveHealthOf(list[k].path, s.ds, s.want);
            items.push_back("Restore " + SvBackupWhen(list[k].label) + (bh.state == 2 ? "  (problem)" : ""));
            paths.push_back("#restore:" + list[k].path);
        }
        items.push_back("Close"); paths.push_back("#close");
        SavesDialog(17, "Restore A Backup", "Fulldex backs up this save every time the game starts, and before anything here changes it. Choose one to go back to it.", items, paths, 0);
    }
    else if (id == SV_EXPORT)
    {
        if (!has) { SavesSay("There is no save to export yet."); return true; }
        namespace fs = std::filesystem;
        const std::string dir = SvDir(s.save) + "/exports";
        std::error_code ec;
        fs::create_directories(fs::u8path(dir), ec);
        const time_t t = time(nullptr);
        std::tm lt{};
        if (const std::tm* p = std::localtime(&t)) lt = *p;
        char when[32];
        strftime(when, sizeof when, "%Y-%m-%d %H%M", &lt);
        const std::string out = dir + "/" + fs::u8path(s.save).stem().u8string() + " " + when + ".sav";
        fs::copy_file(fs::u8path(s.save), fs::u8path(out), fs::copy_options::overwrite_existing, ec);
        if (ec) SavesSay("Could not export it: " + ec.message() + ".");
        else { SavesSay("Exported as " + SvFile(out) + ", in the exports folder beside your saves."); s.request = "#folder:" + dir; }
    }
    else if (id == SV_IMPORT)
    {
        const std::vector<std::string> files = SavesImports();
        std::vector<std::string> items, paths;
        if (SavesCanPick()) { items.push_back("Choose A File..."); paths.push_back("#pick"); }
        for (size_t k = 0; k < files.size() && k < 6; k++) { items.push_back(SvFile(files[k])); paths.push_back("#import:" + files[k]); }
        items.push_back("Close"); paths.push_back("#close");
        SavesDialog(17, "Import Save", std::string(files.empty() ? "Put a .sav (or a DeSmuME .dsv) in the import folder beside your saves, then choose it here."
                                                                : "Choose the save to bring in.")
                                       + " Your save as it is now is backed up first" + (cloud ? ", and the new one goes up to the cloud." : "."), items, paths, 0);
    }
    else if (id == SV_LINK)
    {
        if (!has) { SavesSay("There is no save for " + s.name + " yet: play it and save once, then link it."); return true; }
        std::vector<std::string> items, paths;
        for (const SvLinkFrom& f : SavesLinkSources()) { items.push_back(f.label); paths.push_back("#link:" + f.path); }
        std::string msg;
        if (items.empty())
        {
            msg = "There is no Black or White save here to link. Play Black or White and save in it, or put a Black or White save (.sav) in the import folder beside your saves.";
            items.push_back("Open Import Folder"); paths.push_back("#folder:" + SvDir(s.save) + "/import");
        }
        else msg = "The Unova Link's Memory Link, without a second DS: choose your Black or White save, and " + s.name + " gets its memories as it does over the wireless. Your save is backed up first" +
                   (cloud ? ", and goes up to the cloud." : ".") + " The Black or White save is only read.";
        items.push_back("Close"); paths.push_back("#close");
        SavesDialog(17, "Memory Link", msg, items, paths, 0);
    }
    else if (id == SV_QUICK) s.request = "#play:" + SavesStatePath();
    else if (id == SV_UNDO)
    {
        std::error_code ec;
        const std::string p = s.deleted;
        if (SvExists(p)) { SavesSay("That slot has a new state in it: Undo Delete left it as it is."); s.deleted.clear(); return true; }
        std::filesystem::rename(std::filesystem::u8path(p + ".deleted"), std::filesystem::u8path(p), ec);
        if (!ec && SvExists(p + ".bmp.deleted")) std::filesystem::rename(std::filesystem::u8path(p + ".bmp.deleted"), std::filesystem::u8path(p + ".bmp"), ec);
        SavesSay(ec ? "Could not put it back: " + ec.message() + "." : "The slot is back.");
        s.deleted.clear();
    }
    else if (id == SV_FOLDER) s.request = "#folder:" + SvDir(s.save);
    else if (id == SV_DELETE)
    {
        if (!has) { SavesSay("There is no save to delete."); return true; }
        SavesDialog(18, "Delete " + s.name + "'s Save?",
                    "It is backed up first, so Restore A Backup can bring it back." +
                        std::string(cloud ? " It is deleted in " + SavesProvider() + " too (Google Drive keeps it in its bin). Your other devices keep their own copy." : "") +
                        " Save states are not deleted.",
                    { "Delete Save", "Cancel" }, { "#delete", "#close" }, 1);
    }
    Hub().version++;
    return true;
}
// A on a slot: Play From Slot, Delete Slot.
inline std::string SavesSlotDo(int slot)
{
    if (!Saves().ds || slot < 0 || slot >= 10) return "";
    const std::string p = SavesSlot(slot), n = std::to_string(slot + 1);
    if (!StateSlots::Exists(p)) return "SLOT " + n + " IS EMPTY: SAVE TO IT FROM THE GAME'S MENU";
    SavesDialog(17, "Slot " + n, "Saved " + StateSlots::Friendly(p) + ". Play starts " + Saves().name + " straight from this slot. Your game save is backed up first, as always.",
                { "Play From Slot " + n, "Delete Slot", "Cancel" }, { "#play:" + p, "#delslot:" + p, "#close" }, 0);
    return "";
}
// Before Play, from the list or here: a save with a problem says so first (true: the dialog is up,
// do not play yet). Play Anyway plays once without asking.
inline bool SavesPlayCheck(const Game& g, const std::string& save)
{
    SavesState& s = Saves();
    if (s.playAnyway == g.rom) { s.playAnyway.clear(); return false; }
    const SaveHealth h = SaveHealthOf(save, g.platform == DS);
    if (h.state != 2) return false;
    SavesFor(g, save);
    std::string when;
    const std::string best = SavesBestBackup(when);
    std::vector<std::string> items, paths;
    if (!best.empty()) { items.push_back("Restore " + when); paths.push_back("#restore:" + best); }
    items.push_back("Play Anyway"); paths.push_back("#playanyway");
    items.push_back("Cancel"); paths.push_back("#close");
    SavesDialog(18, "This Save Has A Problem", h.why + (best.empty() ? " There is no backup that looks right." : " The backup from " + when + " looks right.") +
                " Playing may make the game start over and save over it.", items, paths, best.empty() ? 1 : 0);
    return true;
}
// A row of one of the page's dialogs (17 and 18). The dialog closes or becomes the next one; what
// the platform must do is left in request.
inline void SavesPick(const std::string& pick)
{
    SavesState& s = Saves();
    GlassFocus& F = Focus();
    namespace fs = std::filesystem;
    auto close = [&]() { F.dialog = 0; Hub().version++; };
    auto after = [&](const std::string& said) { close(); SavesSay(said); if (SavesCloudOn()) SavesSync(-1); };
    std::error_code ec;
    if (pick.empty() || pick == "#close") { close(); return; }
    if (pick == "#sync" || pick == "#keepthis" || pick == "#keepcloud")
    {
        close();
        if (!SavesSync(pick == "#sync" ? -1 : pick == "#keepthis" ? 0 : 1)) SavesSay("The cloud is busy: try again in a moment.");
        return;
    }
    if (pick == "#playanyway") { close(); s.playAnyway = s.rom; s.request = "#replay"; return; }
    if (pick.rfind("#restore:", 0) == 0)
    {
        const std::string b = pick.substr(9);
        std::string when;
        for (const SaveBackups::Entry& e : SaveBackups::List(s.save)) if (e.path == b) when = SvBackupWhen(e.label);
        const SaveHealth bh = SaveHealthOf(b, s.ds, s.want);
        SavesDialog(18, "Go Back To " + when + "?", (bh.state == 2 ? "This backup has a problem too: " + bh.why + " " : std::string()) +
                    "Your save as it is now is kept as a .before-restore copy" + (SavesCloudOn() ? ", and the restored one goes up to the cloud." : "."),
                    { "Restore Backup", "Cancel" }, { "#dorestore:" + b, "#close" }, 1);
        return;
    }
    if (pick.rfind("#dorestore:", 0) == 0)
    {
        std::string b = pick.substr(11);
        // (The list's own spelling of it: Restore checks it is one of the backups by name.)
        for (const SaveBackups::Entry& e : SaveBackups::List(s.save)) { std::error_code eq; if (std::filesystem::equivalent(std::filesystem::u8path(e.path), std::filesystem::u8path(b), eq)) b = e.path; }
        const long long now = SvSize(s.save), was = SvSize(b);
        std::vector<unsigned char> restored;
        std::string error;
        const bool ok = SaveBackups::Restore(s.save, b, (size_t)(now > 0 ? now : was), restored, error);
        fprintf(stderr, "[launcher] restore %s from %s: %s\n", s.save.c_str(), b.c_str(), ok ? "done" : error.c_str());
        if (ok) after("Save restored. The one before is kept as a .before-restore copy.");
        else { close(); SavesSay("Not restored: " + error); }
        return;
    }
    if (pick.rfind("#import:", 0) == 0)
    {
        const std::string f = pick.substr(8);
        std::vector<unsigned char> d = SaveBackups::Read(f);
        const size_t want = s.ds ? 524288 : 131072;
        if (s.ds && d.size() == want + 122) d.resize(want); // (DeSmuME's .dsv: the save, then its own footer)
        const SaveHealth ih = SaveHealthOfData(d, s.ds, true, s.want);
        if (ih.state != 1) { close(); SavesSay("Not imported: " + SvFile(f) + ". " + (ih.why.empty() ? std::string("It cannot be read.") : ih.why)); return; }
        SavesDialog(18, "Import " + SvFile(f) + "?", "It replaces " + s.name + "'s save. Yours is backed up first, so Restore A Backup can bring it back" +
                    std::string(SavesCloudOn() ? ", and the new one goes up to the cloud." : "."), { "Import Save", "Cancel" }, { "#doimport:" + f, "#close" }, 1);
        return;
    }
    if (pick.rfind("#doimport:", 0) == 0)
    {
        const std::string f = pick.substr(10);
        std::vector<unsigned char> d = SaveBackups::Read(f);
        const size_t want = s.ds ? 524288 : 131072;
        if (s.ds && d.size() == want + 122) d.resize(want);
        std::string error;
        if (SvExists(s.save) && SavesBackUp(s.save).empty()) { close(); SavesSay("Not imported: your save could not be backed up first."); return; }
        if (!SaveBackups::Replace(s.save, d, error)) { close(); SavesSay("Not imported: " + error); return; }
        fprintf(stderr, "[launcher] imported %s as %s\n", f.c_str(), s.save.c_str());
        after("Imported " + SvFile(f) + ". Your save before it is in the backups.");
        return;
    }
    if (pick.rfind("#link:", 0) == 0 || pick.rfind("#linkanyway:", 0) == 0)
    {
        const bool anyway = pick[5] != ':';
        const std::string f = pick.substr(anyway ? 12 : 6);
        MemoryLink::Source src;
        std::string why;
        if (!MemoryLink::ReadBlackWhite(SaveBackups::Read(f), src, why)) { close(); SavesSay("Not linked: " + SvFile(f) + ". " + why); return; }
        const std::string who = src.name + "'s " + MemoryLink::VersionName(src.version);
        std::vector<unsigned char> d = SaveBackups::Read(s.save);
        const MemoryLink::Result r = MemoryLink::Link(d, src, anyway, MemoryLink::NewSeed());
        if (r.outcome == MemoryLink::Failed) { close(); SavesSay("Not linked: " + r.why); return; }
        if (r.outcome == MemoryLink::AsksFirst)
        {
            // (As the game asks: another player's save would take the place of the one linked.)
            SavesDialog(18, "Link " + who + "?", s.name + " is linked with " + SavesLinkedWith(d) + ". " + who + " is another player's save: its memories replace theirs where the two differ. Your save is backed up first.",
                        { "Link " + who, "Cancel" }, { "#linkanyway:" + f, "#close" }, 1);
            return;
        }
        if (r.outcome == MemoryLink::Unchanged) { close(); SavesSay(s.name + " already has everything from " + who + ": nothing changed."); return; }
        if (SavesBackUp(s.save).empty()) { close(); SavesSay("Not linked: your save could not be backed up first."); return; }
        std::string error;
        if (!SaveBackups::Replace(s.save, d, error)) { close(); SavesSay("Not linked: " + error); return; }
        fprintf(stderr, "[launcher] memory link: %s into %s\n", f.c_str(), s.save.c_str());
        after("Linked with " + who + (r.newMemories ? ": new memories are open." : ".") + " Your save before it is in the backups.");
        return;
    }
    if (pick.rfind("#folder:", 0) == 0) { close(); s.request = pick; return; }
    if (pick == "#delete")
    {
        {
            HubState& h = Hub();
            std::lock_guard<std::mutex> l(h.m);
            if (h.busy) { F.dialog = 0; s.msg = "The cloud is busy: try again in a moment."; h.version++; return; }
        }
        if (SavesBackUp(s.save).empty()) { close(); SavesSay("Not deleted: it could not be backed up first."); return; }
        fs::remove(fs::u8path(s.save), ec);
        if (ec) { close(); SavesSay("Not deleted: " + ec.message() + "."); return; }
        fprintf(stderr, "[launcher] deleted %s (backed up first)\n", s.save.c_str());
        close();
        SavesSay("Save deleted. It is in the backups if you want it back.");
        if (SavesCloudOn())
        {
            HubState& h = Hub();
            std::lock_guard<std::mutex> l(h.m);
            h.busy = 2; h.msg = "Deleting it in the cloud..."; h.version++;
            const std::string key = s.cloudKey;
            HubStart([key] { SavesForgetJob(key); });
        }
        return;
    }
    if (pick.rfind("#delslot:", 0) == 0)
    {
        const std::string p = pick.substr(9);
        fs::remove(fs::u8path(p + ".deleted"), ec);
        fs::remove(fs::u8path(p + ".bmp.deleted"), ec);
        ec.clear();
        fs::rename(fs::u8path(p), fs::u8path(p + ".deleted"), ec);
        if (!ec && SvExists(p + ".bmp")) fs::rename(fs::u8path(p + ".bmp"), fs::u8path(p + ".bmp.deleted"), ec);
        close();
        if (ec) SavesSay("Could not delete it: " + ec.message() + ".");
        else { s.deleted = p; SavesSay("Slot deleted. Undo Delete brings it back."); Focus().pact = -1; }
        return;
    }
    if (pick.rfind("#play:", 0) == 0) { close(); s.request = pick; return; }
    close();
}
// B on the page: off a slot first, then back to the games.
inline bool SavesBack()
{
    GlassFocus& F = Focus();
    if (F.pact >= 0) { F.pact = -1; Hub().version++; return true; }
    HubMode() = 0; F.page = -1; F.side = -1; F.pact = -1; F.prow = 0;
    Hub().version++;
    return true;
}
// The arrows on the slots: a grid (left from the first column, or up from the first row when
// upright, back to the list).
inline bool SavesMove(int dir)
{
    GlassFocus& F = Focus();
    const SavesState& s = Saves();
    const int cols = std::max(1, s.cols);
    if (F.pact < 0)
    {
        if (s.ds && ((dir == 2 && !s.stacked) || (dir == 4 && s.stacked && F.prow >= SavesRows() - 1))) { F.pact = 0; Hub().version++; return true; }
        return false;
    }
    if (dir == 3) { if (F.pact >= cols) F.pact -= cols; else if (s.stacked) F.pact = -1; }
    else if (dir == 4) { if (F.pact + cols < 10) F.pact += cols; }
    else if (dir == 1) { if (F.pact % cols == 0) F.pact = -1; else F.pact--; }
    else if (dir == 2) { if (F.pact % cols < cols - 1 && F.pact + 1 < 10) F.pact++; }
    Hub().version++;
    return true;
}
inline int SavesSlotAt(int x, int y)
{
    const SavesState& s = Saves();
    if (!s.ds || s.cw <= 0 || x < s.gx || y < s.gy) return -1;
    const int col = (x - s.gx) / (s.cw + s.cg), r = (y - s.gy) / (s.ch + s.cg);
    if (col >= s.cols || (x - s.gx) % (s.cw + s.cg) >= s.cw || (y - s.gy) % (s.ch + s.cg) >= s.ch || r >= s.visRows) return -1;
    const int k = (r + s.firstRow) * s.cols + col;
    return k < 10 ? k : -1;
}
// A slot's picture: the BMP the game writes beside it (24 or 32 bits, as SDL_SaveBMP makes them).
inline bool SavesLoadBmp(const std::string& path, Glass::Picture& p)
{
    std::vector<unsigned char> d;
    if (!ReadFile(path, d) || d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
    auto u32 = [&](size_t o) { return (uint32_t)d[o] | (uint32_t)d[o + 1] << 8 | (uint32_t)d[o + 2] << 16 | (uint32_t)d[o + 3] << 24; };
    const uint32_t off = u32(10), hdr = u32(14), comp = u32(30);
    const int w = (int)u32(18), h0 = (int)u32(22), bpp = d[28] | d[29] << 8;
    const int h = h0 < 0 ? -h0 : h0;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (bpp != 24 && bpp != 32) || (comp != 0 && comp != 3)) return false;
    uint32_t rm = 0x00FF0000u, gm = 0x0000FF00u, bm = 0x000000FFu;
    if (bpp == 32 && comp == 3 && d.size() >= 14 + hdr + 12 && hdr >= 40) { const size_t mo = hdr >= 52 ? 14 + 40 : 14 + hdr; rm = u32(mo); gm = u32(mo + 4); bm = u32(mo + 8); }
    auto shift = [](uint32_t m) { int s = 0; while (m && !(m & 1)) { m >>= 1; s++; } return s; };
    const int rs = shift(rm), gs = shift(gm), bs = shift(bm);
    const size_t stride = ((size_t)w * (bpp / 8) + 3) & ~(size_t)3;
    if (off + stride * h > d.size()) return false;
    p.w = w; p.h = h; p.px.assign((size_t)w * h, 0);
    for (int y = 0; y < h; y++)
    {
        const unsigned char* row = &d[off + stride * (size_t)(h0 > 0 ? h - 1 - y : y)];
        for (int x = 0; x < w; x++)
        {
            uint32_t r, g, b;
            if (bpp == 24) { b = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2]; }
            else { const uint32_t v = (uint32_t)row[x * 4] | (uint32_t)row[x * 4 + 1] << 8 | (uint32_t)row[x * 4 + 2] << 16 | (uint32_t)row[x * 4 + 3] << 24; r = (v & rm) >> rs; g = (v & gm) >> gs; b = (v & bm) >> bs; }
            p.px[(size_t)y * w + x] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    }
    return true;
}

// SAVES FOLDER (the settings, desktops): everything in the saves folder copied into the one chosen, nothing
// there replaced. The number of files copied, or -1 (said: why).
inline int CopySavesTo(const std::string& from, const std::string& to, std::string& said)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (to.empty() || from.empty()) { said = "NO FOLDER CHOSEN"; return -1; }
    const fs::path a = fs::weakly_canonical(fs::u8path(from), ec);
    if (ec || !fs::is_directory(a, ec) || ec) { said = "CANNOT READ THE CURRENT SAVES FOLDER"; return -1; }
    const fs::path b = fs::weakly_canonical(fs::u8path(to), ec);
    if (ec) { said = "CANNOT RESOLVE THAT FOLDER"; return -1; }
    // Compare actual ancestors, rather than filename prefixes (saves-backup is a sibling).
    // equivalent also handles case-insensitive Windows paths and existing aliases.
    for (fs::path parent = b; !parent.empty();)
    {
        const bool same = fs::equivalent(a, parent, ec);
        ec.clear(); // A not-yet-created destination/ancestor is allowed.
        if (same)
        {
            said = parent == b ? "THAT IS THE SAVES FOLDER ALREADY" : "CHOOSE A FOLDER OUTSIDE THE SAVES FOLDER";
            return -1;
        }
        const fs::path next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }
    fs::create_directories(b, ec);
    if (ec) { said = "CANNOT MAKE THAT FOLDER"; return -1; }
    int n = 0;
    for (fs::recursive_directory_iterator it(a, ec), end; !ec && it != end; it.increment(ec))
    {
        // Keep each entry's name: relative() resolves links and can produce ../ paths.
        const fs::path rel = it->path().lexically_relative(a);
        const bool linked = it->is_symlink(ec);
        if (ec) { said = "CANNOT READ " + rel.u8string(); return -1; }
        if (linked) { said = "CANNOT COPY LINKED PATH " + rel.u8string(); return -1; }
        const fs::path dst = b / rel;
        const auto destination = fs::symlink_status(dst, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) { said = "CANNOT CHECK " + rel.u8string(); return -1; }
        ec.clear();
        if (fs::is_symlink(destination)) { said = "CANNOT COPY INTO LINKED PATH " + rel.u8string(); return -1; }
        const bool directory = it->is_directory(ec);
        if (ec) { said = "CANNOT READ " + rel.u8string(); return -1; }
        if (directory)
        {
            fs::create_directories(dst, ec);
            if (ec) { said = "CANNOT MAKE " + rel.u8string(); return -1; }
            continue;
        }
        const bool regular = it->is_regular_file(ec);
        if (ec) { said = "CANNOT READ " + rel.u8string(); return -1; }
        if (!regular) continue;
        const bool exists = fs::exists(dst, ec);
        if (ec) { said = "CANNOT CHECK " + rel.u8string(); return -1; }
        if (exists)
        {
            if (!fs::is_regular_file(destination)) { said = "A FILE IS BLOCKED BY " + rel.u8string(); return -1; }
            continue;
        }
        fs::copy_file(it->path(), dst, ec);
        if (ec) { said = "COULD NOT COPY " + rel.u8string(); return -1; }
        n++;
    }
    if (ec) { said = "CANNOT READ THE CURRENT SAVES FOLDER"; return -1; }
    return n;
}
inline std::vector<std::string> WrapLines(const std::string& msg0, float px, int w, bool bold); // (below)
// The page, in DrawPage's place for the Hub's page 3.
inline void DrawSavesPage(Canvas& c, bool solid, float u, int L, int R, int top, int bottom)
{
    GlassFocus& F = Focus();
    SavesState& s = Saves();
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint, green = 0xFF8FE3A2u, red = 0xFFF2A08Au, amber = 0xFFF2C46Bu, blue = 0xFF8CC8FFu, grey = 0xFFAEB7C4u;
    const bool stk = PhoneLook() && c.h > c.w;
    s.stacked = stk;
    const std::vector<int> ids = SavesRowIds();
    const int n = (int)ids.size();
    F.prow = std::max(0, std::min(F.prow, n - 1));
    if (!s.ds) F.pact = -1;
    const int gap = (int)(18 * u), pad = (int)(14 * u), rg = (int)((stk ? 6 : 8) * u);
    // (Nine rows, Black 2's Memory Link with a quick save and Undo Delete, close up to fit the panel.)
    const int rowH = stk ? (int)(46 * u) : std::max((int)(46 * u), std::min((int)(60 * u), (bottom - top - 2 * pad + rg) / std::max(1, n) - rg));
    const int lw = stk ? R - L : std::max((int)(330 * u), (int)((R - L - gap) * 0.42f));
    const int lh = stk ? std::min(bottom - top, 2 * pad + n * (rowH + rg) - rg) : bottom - top;
    const SaveHealth& hl = SavesHealth();
    std::string cl;
    const int cs = SavesCloud(cl);
    const bool cloud = cs != 0;
    std::string hubMsg;
    { HubState& h = Hub(); std::lock_guard<std::mutex> l(h.m); hubMsg = h.msg; }
    const std::vector<SaveBackups::Entry> backups = SaveBackups::List(s.save);
    Menu::FrostBox(c, solid, L, top, lw, lh, 18 * u);
    F.pX = L + pad; F.pY = top + pad; F.pW = lw - 2 * pad; F.pRow = rowH + rg; F.rVis = n; F.rScroll = 0; F.aW = 0;
    auto chip = [&](int right, int mid, const std::string& t, uint32_t col) {
        const int w = GW(t, 12 * u, true) + (int)(20 * u), x = right - w;
        Glass::Round(c, x, mid - (int)(11 * u), w, (int)(22 * u), 11 * u, 0x24FFFFFFu);
        GT(c, x + (int)(10 * u), mid, t, 12 * u, col, true);
        return x;
    };
    for (int i = 0; i < n; i++)
    {
        const int id = ids[i];
        const int rx = L + pad, ry = top + pad + i * (rowH + rg), rww = lw - 2 * pad, mid = ry + rowH / 2;
        if (i == F.prow && F.pact < 0)
        {
            if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 14 * u, u);
            Glass::Round(c, rx, ry, rww, rowH, 14 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
        }
        else Glass::Round(c, rx, ry, rww, rowH, 14 * u, SettingsArt::Row, 0x3DFFFFFF, 1.0f * u);
        std::string name, sub;
        int icon = Glass::I_SAVE;
        uint32_t ink = text;
        switch (id)
        {
        case SV_SAVE: name = "Game Save"; sub = SvExists(s.save) ? "Saved " + StateSlots::Friendly(s.save) + "   \xC2\xB7   " + SvKB(SvSize(s.save)) : "None yet: the game makes it"; break;
        case SV_RESTORE: name = "Restore A Backup"; icon = Glass::I_REWIND; sub = backups.empty() ? "None yet: one is made each time the game starts" : std::to_string(backups.size()) + (backups.size() == 1 ? " backup" : " backups") + "   \xC2\xB7   newest " + SvBackupWhen(backups[0].label); break;
        case SV_EXPORT: name = "Export Save"; icon = Glass::I_CHEVRON_U; sub = "A copy in the exports folder"; break;
        case SV_IMPORT: name = "Import Save"; icon = Glass::I_CHEVRON_D; sub = SavesCanPick() ? "A file you choose, or from the import folder" : "From the import folder beside your saves"; break;
        case SV_LINK: name = "Memory Link"; icon = Glass::I_SPARKLE; sub = SavesLinkSub(); break;
        case SV_QUICK: name = "Play From Quick Save"; icon = Glass::I_PLAY; sub = "Saved " + StateSlots::Friendly(SavesStatePath()); break;
        case SV_UNDO: name = "Undo Delete"; icon = Glass::I_REWIND; sub = "The slot deleted last comes back"; break;
        case SV_FOLDER: name = "Open Saves Folder"; icon = Glass::I_LAYERS; sub = SvDir(s.save); break;
        default: name = "Delete Save"; icon = Glass::I_BACK; ink = red; sub = cloud ? "Here and in " + SavesProvider() + ". A backup is kept" : "Asks first. A backup is kept"; break;
        }
        Glass::DrawIcon(c, icon, rx + 28 * u, mid, (stk ? 10 : 12) * u, ink);
        int right = rx + rww - (int)(14 * u);
        if (id == SV_SAVE)
        {
            right = chip(right, mid, hl.label, hl.state == 1 ? green : hl.state == 2 ? red : grey) - (int)(6 * u);
            if (cloud) right = chip(right, mid, cl, cs == 1 ? green : cs == 2 ? amber : cs == 4 ? blue : grey) - (int)(6 * u);
        }
        else if (id == SV_RESTORE && !backups.empty()) right = chip(right - (int)(16 * u), mid, std::to_string(backups.size()), grey) - (int)(6 * u);
        if (id != SV_SAVE) GT(c, rx + rww - (int)(24 * u), mid, ">", 15 * u, hint, true);
        if (stk) GTFit(c, rx + (int)(54 * u), mid, name, 16 * u, right - (rx + (int)(54 * u)) - (int)(8 * u), ink, true);
        else
        {
            GTFit(c, rx + (int)(56 * u), mid - 10 * u, name, 17 * u, right - (rx + (int)(56 * u)) - (int)(8 * u), ink, true);
            GTFit(c, rx + (int)(56 * u), mid + 13 * u, sub, 12 * u, right - (rx + (int)(56 * u)) - (int)(8 * u), hint, false);
        }
    }
    if (!stk)
    {
        // (Under the rows: what the page promises.)
        int y = top + pad + n * (rowH + rg) + (int)(10 * u);
        for (const std::string& line : WrapLines("Fulldex backs up this save every time the game starts and before anything here changes it. Your ROM is never changed.", 12 * u, lw - 2 * pad - (int)(12 * u), false))
        {
            if (y + 16 * u > top + lh - pad) break;
            GT(c, L + pad + (int)(6 * u), y + 8 * u, line, 12 * u, hint, false);
            y += (int)(18 * u);
        }
    }

    // The save states: the ten slots as cards.
    const int dx = stk ? L : L + lw + gap, dy = stk ? top + lh + gap : top, dw = stk ? R - L : R - dx, dh = bottom - dy;
    if (dh < (int)(80 * u)) { s.cw = 0; return; }
    Menu::FrostBox(c, solid, dx, dy, dw, dh, 18 * u);
    const int ip = (int)(20 * u);
    int y = dy + (int)(18 * u);
    GT(c, dx + ip, y + 12 * u, "Save States", (stk ? 18 : 20) * u, text, true);
    if (cloud) { const std::string t = "They stay on this device"; GT(c, dx + dw - ip - GW(t, 12 * u, false), y + 12 * u, t, 12 * u, hint, false); }
    y += (int)(40 * u);
    const std::string msg = !s.msg.empty() ? s.msg : hubMsg;
    const int footH = stk ? (msg.empty() ? 0 : (int)(40 * u)) : (int)(96 * u);
    if (!s.ds)
    {
        s.cw = 0;
        for (const std::string& line : WrapLines(s.want ? "3DS games have no save states: they save from the game's own menu. Save state slots are for the DS games."
                                                        : "The GBA games keep one save state each, made and loaded from the game's menu. Save state slots are for the DS games.", 13 * u, dw - 2 * ip, false))
        { GT(c, dx + ip, y + 8 * u, line, 13 * u, hint, false); y += (int)(20 * u); }
    }
    else
    {
        const int cols = stk ? 3 : 5, cg = (int)((stk ? 8 : 12) * u);
        const int cw = (dw - 2 * ip - (cols - 1) * cg) / cols, ph = cw * 3 / 4, ch = ph + (int)(44 * u);
        const int rowsAll = (10 + cols - 1) / cols;
        const int vis = std::max(1, std::min(rowsAll, (dy + dh - footH - (int)(14 * u) - y + cg) / (ch + cg)));
        if (F.pact >= 0)
        {
            const int r = F.pact / cols;
            if (r < s.firstRow) s.firstRow = r;
            if (r >= s.firstRow + vis) s.firstRow = r - vis + 1;
        }
        s.firstRow = std::max(0, std::min(s.firstRow, rowsAll - vis));
        s.cols = cols; s.gx = dx + ip; s.gy = y; s.cw = cw; s.ch = ch; s.cg = cg; s.visRows = vis;
        for (int r = 0; r < vis; r++)
            for (int col = 0; col < cols; col++)
            {
                const int k = (s.firstRow + r) * cols + col;
                if (k >= 10) break;
                const int x = s.gx + col * (cw + cg), yy = y + r * (ch + cg);
                const std::string p = SavesSlot(k);
                const bool full = StateSlots::Exists(p);
                if (k == F.pact)
                {
                    if (F.side < 0) Glass::Glow(c, x, yy, cw, ch, 12 * u, u);
                    Glass::Round(c, x, yy, cw, ch, 12 * u, Menu::FrostChosen, Glass::ChosenEdge(), 1.5f * u);
                }
                else Glass::Round(c, x, yy, cw, ch, 12 * u, full ? SettingsArt::Row : 0x0DFFFFFFu, full ? 0x3DFFFFFFu : 0x1FFFFFFFu, 1.0f * u);
                const int px = x + (int)(6 * u), py = yy + (int)(6 * u), pw = cw - (int)(12 * u), phh = ph - (int)(6 * u);
                const long long at = full && StateSlots::PreviewCurrent(p) ? SvTime(p + ".bmp") : 0;
                if (at != s.thumbAt[k]) { s.thumbAt[k] = at; s.thumb[k] = Glass::Picture(); if (at) SavesLoadBmp(p + ".bmp", s.thumb[k]); }
                if (full && !s.thumb[k].px.empty()) Glass::PictureIn(c, s.thumb[k], px, py, pw, phh, 8 * u, 255);
                else
                {
                    Glass::Round(c, px, py, pw, phh, 8 * u, full ? 0x30000000u : 0x08FFFFFFu, full ? 0x26FFFFFFu : 0x1AFFFFFFu, 1.0f * u);
                    if (full) GT(c, px + (pw - GW("No Picture", 11 * u, false)) / 2, py + phh * 0.5f, "No Picture", 11 * u, hint, false);
                }
                GT(c, x + (int)(10 * u), yy + ph + (int)(14 * u), "Slot " + std::to_string(k + 1), 14 * u, full ? text : hint, true);
                GTFit(c, x + (int)(10 * u), yy + ph + (int)(32 * u), full ? StateSlots::Friendly(p) : std::string("Empty"), 11 * u, cw - (int)(20 * u), hint, false);
            }
        if (s.firstRow > 0) Glass::DrawIcon(c, Glass::I_CHEVRON_U, dx + dw - ip - 8 * u, y - 10 * u, 6 * u, hint);
        if (s.firstRow + vis < rowsAll) Glass::DrawIcon(c, Glass::I_CHEVRON_D, dx + dw - ip - 8 * u, y + vis * (ch + cg) - cg + 8 * u, 6 * u, hint);
        y += vis * (ch + cg) - cg + (int)(18 * u);
    }
    // Under the slots: the chosen one, or what was done.
    if (!stk && s.ds)
    {
        Glass::Round(c, dx + ip, y - (int)(8 * u), dw - 2 * ip, 1, 0, 0x2EFFFFFFu);
        if (F.pact >= 0 && F.pact < 10)
        {
            const std::string p = SavesSlot(F.pact);
            const bool full = StateSlots::Exists(p);
            GT(c, dx + ip, y + 14 * u, "Slot " + std::to_string(F.pact + 1) + (full ? "   \xC2\xB7   " + StateSlots::Friendly(p) : std::string("   \xC2\xB7   Empty")), 17 * u, text, true);
            GT(c, dx + ip, y + 40 * u, full ? "A: play from it or delete it" : "Save to it from the game's menu", 13 * u, hint, false);
            y += (int)(58 * u);
        }
        else
        {
            GT(c, dx + ip, y + 12 * u, "Choose a slot to play from it, or delete it. Save to the slots from the game's menu.", 13 * u, hint, false);
            y += (int)(32 * u);
        }
    }
    if (!msg.empty())
    {
        const int my = stk ? dy + dh - (int)(30 * u) : std::min(y + (int)(4 * u), dy + dh - (int)(30 * u));
        GTFit(c, dx + ip, my + 10 * u, msg, 13 * u, dw - 2 * ip, cs == 4 ? blue : text, false);
    }
}
