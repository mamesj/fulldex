#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace Shortcuts
{
// Read the quoted directory assignments written by xdg-user-dirs-update.
// Decode its shell escapes without evaluating commands or arbitrary variables.
inline bool ParseDesktopPath(const std::string& line, const std::string& home, std::string& result)
{
    size_t at = line.find_first_not_of(" \t");
    const std::string key = "XDG_DESKTOP_DIR";
    if (at == std::string::npos || line.compare(at, key.size(), key) != 0) return false;
    at += key.size();
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) ++at;
    if (at == line.size() || line[at++] != '=') return false;
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) ++at;
    if (at == line.size() || line[at++] != '"') return false;
    std::string path;
    if (line.compare(at, 5, "$HOME") == 0 && at + 5 < line.size() && (line[at + 5] == '/' || line[at + 5] == '"'))
    { path = home; at += 5; }
    bool closed = false;
    while (at < line.size())
    {
        char c = line[at++];
        if (c == '"') { closed = true; break; }
        if (c == '$' || c == '`' || c == '\r' || c == '\n') return false;
        if (c == '\\' && at < line.size())
        {
            const char next = line[at];
            if (next == '\\' || next == '"' || next == '$' || next == '`') { path += next; ++at; continue; }
        }
        path += c;
    }
    if (!closed) return false;
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t' || line[at] == '\r' || line[at] == '\n')) ++at;
    if (at < line.size() && line[at] != '#') return false;
    if (!path.empty() && !std::filesystem::u8path(path).is_absolute()) return false;
    result = std::move(path);
    return true;
}

inline std::string ReadDesktopPath(const std::string& config, const std::string& home)
{
    std::string desktop = home + "/Desktop", line;
    std::ifstream file(std::filesystem::u8path(config) / "user-dirs.dirs");
    while (std::getline(file, line)) ParseDesktopPath(line, home, desktop);
    return desktop;
}
}
