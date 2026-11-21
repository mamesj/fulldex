#pragma once
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace RuntimeKit
{
inline std::filesystem::path NativePath(const std::filesystem::path& input, std::error_code& ec)
{
#ifdef _WIN32
    auto path = std::filesystem::absolute(input, ec).lexically_normal();
    if (ec) return {};
    const std::wstring wide = path.make_preferred().wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    if (wide.rfind(L"\\\\", 0) == 0) return std::filesystem::path(L"\\\\?\\UNC\\" + wide.substr(2));
    return std::filesystem::path(L"\\\\?\\" + wide);
#else
    (void)ec;
    return input;
#endif
}

// Whether two files hold the same bytes (false when either is missing or unreadable).
inline bool SameBytes(const std::filesystem::path& a, const std::filesystem::path& b)
{
    std::error_code ec;
    const auto na = NativePath(a, ec);
    if (ec) return false;
    const auto nb = NativePath(b, ec);
    if (ec) return false;
    const auto size = std::filesystem::file_size(na, ec);
    if (ec || std::filesystem::file_size(nb, ec) != size || ec) return false;
    std::ifstream fa(na, std::ios::binary), fb(nb, std::ios::binary);
    if (!fa || !fb) return false;
    char ba[16384], bb[16384];
    for (;;)
    {
        fa.read(ba, sizeof ba);
        fb.read(bb, sizeof bb);
        const std::streamsize n = fa.gcount();
        if (n != fb.gcount() || std::memcmp(ba, bb, (size_t)n) != 0) return false;
        if (n < (std::streamsize)sizeof ba) return true;
    }
}

// A copy of from in place of to, which may exist: copied beside it, then moved over it, so a failed copy never
// leaves to missing. (MinGW's copy_file refuses overwrite_existing on Windows: "File exists".)
inline bool ReplaceWith(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    namespace fs = std::filesystem;
    ec.clear();
    const auto src = NativePath(from, ec);
    if (ec) return false;
    const auto dst = NativePath(to, ec);
    if (ec) return false;
    fs::path tmp = dst;
    tmp += ".new";
    std::error_code ignore;
    fs::remove(tmp, ignore);
    if (!fs::copy_file(src, tmp, ec) || ec) { fs::remove(tmp, ignore); return false; }
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        ec = std::error_code((int)GetLastError(), std::system_category());
        fs::remove(tmp, ignore);
        return false;
    }
#else
    fs::rename(tmp, dst, ec);
    if (ec) { fs::remove(tmp, ignore); return false; }
#endif
    return true;
}

// Retry an interrupted kit installation even if its executable was copied first.
// Existing files can contain a prepared game or player settings: never replace them.
inline bool CopyMissing(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    namespace fs = std::filesystem;
    ec.clear();
    const auto nativeFrom = NativePath(from, ec);
    if (ec) return false;
    const auto nativeTo = NativePath(to, ec);
    if (ec) return false;
    fs::create_directories(nativeTo, ec);
    if (ec) return false;
    auto prefix = nativeFrom.native();
    if (prefix.empty()) { ec = std::make_error_code(std::errc::invalid_argument); return false; }
    if (prefix.back() != fs::path::preferred_separator) prefix += fs::path::preferred_separator;
    for (fs::recursive_directory_iterator it(nativeFrom, ec), end; !ec && it != end; it.increment(ec))
    {
        // Validate the iterator's source prefix before stripping it. Both relative path
        // operations misparse extended Windows paths in some MinGW libraries.
        const auto entry = it->path().native();
        if (entry.size() <= prefix.size() || entry.compare(0, prefix.size(), prefix) != 0)
        { ec = std::make_error_code(std::errc::invalid_argument); break; }
        const fs::path relative(entry.substr(prefix.size()));
        const fs::path dest = nativeTo / relative;
        const bool directory = it->is_directory(ec);
        if (ec) break;
        if (directory) fs::create_directories(dest, ec);
        else
        {
            const bool exists = fs::exists(dest, ec);
            if (ec) break;
            if (!exists)
            {
                fs::copy_file(it->path(), dest, ec);
#ifndef _WIN32
                if (!ec)
                {
                    const auto permissions = fs::status(it->path(), ec).permissions();
                    if (!ec) fs::permissions(dest, permissions, ec);
                }
#endif
            }
        }
        // iterator.increment(ec) clears ec on success; do not let it hide a failed copy.
        if (ec) break;
    }
    return !ec;
}
}
