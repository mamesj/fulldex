#pragma once
// Read complete text settings before a helper rewrites any part of the file.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ConfigLines
{
template <class Visit>
inline bool ForEach(const std::string& path, Visit visit)
{
    const auto file = std::filesystem::u8path(path);
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open())
    {
        std::error_code ec;
        const bool present = std::filesystem::exists(file, ec);
        if (ec || present) return false;
        return true; // (A genuinely missing settings file can be created.)
    }
    std::string line;
    while (std::getline(input, line))
    {
        if (line.find('\0') != std::string::npos) return false;
        while (!line.empty() && line.back() == '\r') line.pop_back();
        visit(line);
    }
    const bool read = input.eof() && !input.bad();
    input.clear(); // (Clean EOF sets failbit too; check close separately.)
    input.close();
    return read && !input.fail();
}
inline bool Read(const std::string& path, std::vector<std::string>& lines)
{
    std::vector<std::string> complete;
    if (!ForEach(path, [&](const std::string& line) { complete.push_back(line); })) return false;
    lines.swap(complete);
    return true;
}
}
