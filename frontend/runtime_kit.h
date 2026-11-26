#pragma once
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <memory>
#include <vector>
#endif
#include "runtime_kit_windows.h"

namespace RuntimeKit
{
inline std::filesystem::path NativePath(const std::filesystem::path& input, std::error_code& ec)
{
#ifdef _WIN32
    // Already extended paths must not pass through MinGW's absolute-path parser.
    if (input.native().rfind(L"\\\\?\\", 0) == 0)
    { auto path = input; return path.make_preferred(); }
    auto path = std::filesystem::absolute(input, ec).lexically_normal();
    if (ec) return {};
    const std::wstring wide = path.make_preferred().wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    // (\\?\ is only needed for long paths. A mapped or virtual drive rejects \\?\D:\... with
    // ERROR_INVALID_PARAMETER, and every 3DS kit path is short.)
    if (wide.size() < 248) return path;
    if (wide.rfind(L"\\\\", 0) == 0) return std::filesystem::path(L"\\\\?\\UNC\\" + wide.substr(2));
    return std::filesystem::path(L"\\\\?\\" + wide);
#else
    (void)ec;
    return input;
#endif
}

#ifdef _WIN32
inline std::filesystem::path OrdinaryPath(const std::filesystem::path& input)
{
    auto text = input.native();
    if (text.rfind(L"\\\\?\\UNC\\", 0) == 0) text = L"\\\\" + text.substr(8);
    else if (text.rfind(L"\\\\?\\", 0) == 0) text = text.substr(4);
    return std::filesystem::path(text);
}
#endif

inline std::filesystem::path GuardPath(const std::filesystem::path& input, std::error_code& ec)
{
#ifdef _WIN32
    namespace fs = std::filesystem;
    // MinGW's weakly_canonical misparses extended paths. Resolve the nearest
    // existing ancestor through Windows instead, including junction targets.
    auto path = fs::absolute(OrdinaryPath(input), ec).lexically_normal();
    if (ec) return {};
    std::vector<fs::path> missing;
    for (;;)
    {
        const auto native = NativePath(path, ec);
        if (ec) return {};
        const bool exists = fs::exists(native, ec);
        if (ec) return {};
        if (exists) break;
        const auto parent = path.parent_path();
        if (parent.empty() || parent == path)
        { ec = std::make_error_code(std::errc::no_such_file_or_directory); return {}; }
        missing.push_back(path.filename());
        path = parent;
    }
    const auto native = NativePath(path, ec);
    if (ec) return {};
    HANDLE raw = CreateFileW(native.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (raw == INVALID_HANDLE_VALUE)
    { ec = std::error_code(GetLastError(), std::system_category()); return {}; }
    std::unique_ptr<void, decltype(&CloseHandle)> handle(raw, &CloseHandle);
    std::wstring buffer(256, L'\0');
    for (;;)
    {
        const DWORD length = GetFinalPathNameByHandleW(handle.get(), buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!length)
        { ec = std::error_code(GetLastError(), std::system_category()); return {}; }
        if (length < buffer.size()) { buffer.resize(length); break; }
        buffer.resize(static_cast<size_t>(length) + 1);
    }
    auto resolved = OrdinaryPath(fs::path(buffer));
    for (auto it = missing.rbegin(); it != missing.rend(); ++it) resolved /= *it;
    return resolved.lexically_normal();
#else
    return std::filesystem::weakly_canonical(input, ec);
#endif
}

// Compare existing physical ancestors, so aliases and Windows case differences
// cannot turn a sibling check into a copy back into the source tree.
inline bool ContainsPath(const std::filesystem::path& outer, std::filesystem::path inner, std::error_code& ec)
{
    namespace fs = std::filesystem;
    const auto nativeOuter = NativePath(outer, ec);
    if (ec) return false;
    const bool outerExists = fs::exists(nativeOuter, ec);
    if (ec || !outerExists) return false;
    auto root = inner.root_path();
#ifdef _WIN32
    // A UNC share is the filesystem root; do not probe its server as a folder.
    if (inner.native().rfind(L"\\\\", 0) == 0 && !inner.relative_path().empty())
        root /= *inner.relative_path().begin();
#endif
    for (;;)
    {
        const auto nativeInner = NativePath(inner, ec);
        if (ec) return false;
        const bool exists = fs::exists(nativeInner, ec);
        if (ec) return false;
        if (exists)
        {
            if (fs::equivalent(nativeOuter, nativeInner, ec)) return true;
            if (ec) return false;
        }
        if (inner == root) return false;
        const auto parent = inner.parent_path();
        if (parent.empty() || parent == inner) return false;
        inner = parent;
    }
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
inline void Fail(const char* stage, const std::filesystem::path& path, const std::error_code& ec)
{
    std::fprintf(stderr, "[3ds] kit copy: %s [%s]: %d %s\n", stage, path.u8string().c_str(), ec.value(), ec.message().c_str());
}

inline bool CopyMissing(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    namespace fs = std::filesystem;
    ec.clear();
    bool reported = false; // (a failure already logged here: do not log it twice at the end)
    const auto nativeFrom = NativePath(from, ec);
    if (ec) { Fail("source path", from, ec); return false; }
    const auto nativeTo = NativePath(to, ec);
    if (ec) { Fail("destination path", to, ec); return false; }
    if (!fs::is_directory(nativeFrom, ec))
    {
        if (!ec) ec = std::make_error_code(std::errc::not_a_directory);
        Fail("source folder", nativeFrom, ec);
        return false;
    }
    // (The destination may not be inside the source, nor the source inside it: the copy would copy its own
    // output until the path got too long.)
    const auto source = GuardPath(from, ec);
    if (ec) { Fail("resolve source", from, ec); return false; }
    const auto target = GuardPath(to, ec);
    if (ec) { Fail("resolve destination", to, ec); return false; }
    const bool intoSource = ContainsPath(source, target, ec);
    if (ec) { Fail("compare paths", to, ec); return false; }
    const bool aroundSource = ContainsPath(target, source, ec);
    if (ec) { Fail("compare paths", from, ec); return false; }
    if (intoSource || aroundSource)
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        Fail("destination overlaps the source", to, ec);
        return false;
    }
    fs::create_directories(nativeTo, ec);
    if (ec) { Fail("destination folder", nativeTo, ec); return false; }
#ifdef _WIN32
    // (Windows: its own walker, FindFirstFileW / FindNextFileW over extended paths: MinGW's recursive iterator
    // gave names from the working folder for a long kit folder.)
    if (!CopyWindowsTree(nativeFrom, nativeTo, ec))
    {
        Fail("copy (Windows walker)", nativeFrom, ec);
        return false;
    }
    return true;
#else
    auto prefix = nativeFrom.native();
    if (prefix.empty()) { ec = std::make_error_code(std::errc::invalid_argument); Fail("empty source path", nativeFrom, ec); return false; }
    if (prefix.back() != fs::path::preferred_separator) prefix += fs::path::preferred_separator;
    fs::path last;
    for (fs::recursive_directory_iterator it(nativeFrom, ec), end; !ec && it != end; it.increment(ec))
    {
        last = it->path();
        // Validate the iterator's source prefix before stripping it. Both relative path
        // operations misparse extended Windows paths in some MinGW libraries.
        const auto entry = it->path().native();
        if (entry.size() <= prefix.size() || entry.compare(0, prefix.size(), prefix) != 0)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            std::fprintf(stderr, "[3ds] kit copy: entry outside the source folder: entry [%s], expected prefix [%s], source [%s]\n",
                         fs::path(entry).u8string().c_str(), fs::path(prefix).u8string().c_str(), nativeFrom.u8string().c_str());
            reported = true;
            break;
        }
        const fs::path relative(entry.substr(prefix.size()));
        const fs::path dest = nativeTo / relative;
        const bool directory = it->is_directory(ec);
        if (ec) { Fail("inspect source entry", it->path(), ec); reported = true; break; }
        if (directory)
        {
            fs::create_directories(dest, ec);
            if (ec) { Fail("make destination folder", dest, ec); reported = true; break; }
        }
        else
        {
            const bool exists = fs::exists(dest, ec);
            if (ec) { Fail("check destination", dest, ec); reported = true; break; }
            if (exists)
            {
                const auto status = fs::status(dest, ec);
                if (ec) { Fail("check destination", dest, ec); reported = true; break; }
                // An existing directory or special file is not an installed tool.
                // Keep it, but report the obstruction instead of claiming success.
                if (!fs::is_regular_file(status))
                {
                    ec = std::make_error_code(fs::is_directory(status) ? std::errc::is_a_directory : std::errc::invalid_argument);
                    Fail("destination is not a file", dest, ec);
                    reported = true;
                    break;
                }
            }
            if (!exists)
            {
                fs::copy_file(it->path(), dest, ec);
                if (ec)
                {
                    std::fprintf(stderr, "[3ds] kit copy: copy [%s] -> [%s]: %d %s\n",
                                 it->path().u8string().c_str(), dest.u8string().c_str(), ec.value(), ec.message().c_str());
                    reported = true;
                    break;
                }
#ifndef _WIN32
                const auto permissions = fs::status(it->path(), ec).permissions();
                if (!ec) fs::permissions(dest, permissions, ec);
                if (ec) { Fail("copy permissions", dest, ec); reported = true; break; }
#endif
            }
        }
        // iterator.increment(ec) clears ec on success; do not let it hide a failed copy.
        if (ec) break;
    }
    // (An error from the iterator itself, not from a file: name the last entry reached.)
    if (ec && !reported)
        std::fprintf(stderr, "[3ds] kit copy: walking [%s] stopped at [%s]: %d %s\n",
                     nativeFrom.u8string().c_str(), last.u8string().c_str(), ec.value(), ec.message().c_str());
    return !ec;
#endif
}
}
