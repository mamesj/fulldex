#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

namespace RuntimeKit
{
// MinGW's directory iterator can enumerate the working directory instead of a
// long extended-path source. Keep enumeration and names in the wide Win32 API.
inline bool CopyWindowsTree(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& ec)
{
    using Path = std::filesystem::path;
    std::vector<std::pair<Path, Path>> pending {{from, to}};
    auto fail = [&](DWORD error) { ec = std::error_code(error, std::system_category()); return false; };
    auto child = [](const Path& parent, const wchar_t* name) { return Path(parent.native() + L"\\" + name); };
    while (!pending.empty())
    {
        const auto folder = std::move(pending.back()); pending.pop_back();
        WIN32_FIND_DATAW data {};
        HANDLE raw = FindFirstFileW(child(folder.first, L"*").c_str(), &data);
        if (raw == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) continue;
            return fail(error);
        }
        std::unique_ptr<void, decltype(&FindClose)> search(raw, &FindClose);
        for (;;)
        {
            if (wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0)
            {
                const auto source = child(folder.first, data.cFileName), dest = child(folder.second, data.cFileName);
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!CreateDirectoryW(dest.c_str(), nullptr))
                    {
                        const DWORD error = GetLastError();
                        if (error != ERROR_ALREADY_EXISTS) return fail(error);
                        const DWORD attributes = GetFileAttributesW(dest.c_str());
                        if (attributes == INVALID_FILE_ATTRIBUTES) return fail(GetLastError());
                        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return fail(ERROR_DIRECTORY);
                    }
                    // Match recursive_directory_iterator's default: no directory-link traversal.
                    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) pending.emplace_back(source, dest);
                }
                else
                {
                    const DWORD attributes = GetFileAttributesW(dest.c_str());
                    if (attributes != INVALID_FILE_ATTRIBUTES)
                    {
                        if (attributes & FILE_ATTRIBUTE_DIRECTORY) return fail(ERROR_DIRECTORY);
                        // Existing game files and settings are deliberately preserved.
                    }
                    else
                    {
                        const DWORD error = GetLastError();
                        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return fail(error);
                        if (!CopyFileW(source.c_str(), dest.c_str(), TRUE)) return fail(GetLastError());
                    }
                }
            }
            if (FindNextFileW(search.get(), &data)) continue;
            const DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) return fail(error);
            break;
        }
    }
    return true;
}
}
#endif
