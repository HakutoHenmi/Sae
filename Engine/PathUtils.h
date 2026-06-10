#pragma once
#include <string>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <algorithm>
#include <vector>

namespace Engine {

class PathUtils {
public:
    /**
     * @brief Unity's Application.dataPath equivalent. Returns the path to the "Resources" folder.
     * @return Absolute path in UTF-8.
     */
    static std::string GetAssetsPath() {
        std::filesystem::path root = GetRootPathInternal();
        return ToUTF8((root / "Resources").wstring());
    }

    /**
     * @brief Unity's Application.streamingAssetsPath equivalent. 
     * @return Absolute path in UTF-8.
     */
    static std::string GetStreamingAssetsPath() {
        std::filesystem::path root = GetRootPathInternal();
        return ToUTF8((root / "Resources" / "StreamingAssets").wstring());
    }

    /**
     * @brief Unity's Application.persistentDataPath equivalent.
     * Returns %APPDATA%/Local/neo_Engine.
     * @return Absolute path in UTF-8.
     */
    static std::string GetPersistentDataPath() {
        wchar_t* localAppData = nullptr;
        // Windows standard: AppData/Local
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData))) {
            std::filesystem::path p(localAppData);
            CoTaskMemFree(localAppData);
            p /= "neo_Engine";
            if (!std::filesystem::exists(p)) {
                std::filesystem::create_directories(p);
            }
            return ToUTF8(p.wstring());
        }
        // Fallback to exe directory/SaveData if AppData fails
        std::filesystem::path fallback = GetRootPathInternal() / "SaveData";
        if (!std::filesystem::exists(fallback)) {
            std::filesystem::create_directories(fallback);
        }
        return ToUTF8(fallback.wstring());
    }

    /**
     * @brief Path from either absolute or relative (from root) path.
     * Ensures result is absolute and uses '/' separators.
     * @param relPath Path string (UTF-8).
     * @return Absolute path in UTF-8.
     */
    static std::string GetUnifiedPath(const std::string& relPath) {
        if (relPath.empty()) return "";
        std::filesystem::path p = FromUTF8(relPath);
        if (p.is_absolute()) return ToUTF8(p.wstring());
        
        std::filesystem::path finalPath = GetRootPathInternal() / p;
        std::string result = ToUTF8(finalPath.wstring());
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }

    static std::wstring GetUnifiedPathW(const std::wstring& relPath) {
        if (relPath.empty()) return L"";
        std::filesystem::path p(relPath);
        if (p.is_absolute()) return relPath;
        
        std::filesystem::path finalPath = GetRootPathInternal() / p;
        std::wstring result = finalPath.wstring();
        std::replace(result.begin(), result.end(), L'\\', L'/');
        return result;
    }

    // --- Utility: UTF-8 <-> Wide String ---

    static std::string ToUTF8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
        return str;
    }

    static std::wstring FromUTF8(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring wstr(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        return wstr;
    }

    // For backward compatibility (deprecated: use GetAssetsPath or GetUnifiedPath)
    static std::string GetRootPath() {
        return ToUTF8(GetRootPathInternal().wstring());
    }

private:
    static bool HasProjectFile(const std::filesystem::path& dir) {
        try {
            if (std::filesystem::exists(dir / "DirectXGame_New.sln")) return true;
            if (std::filesystem::exists(dir / "neo_Engine.sln")) return true;
            if (std::filesystem::exists(dir / "DirectXGameApp.vcxproj")) return true;
        } catch (...) {}
        return false;
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:6262)
#endif
    static std::filesystem::path GetRootPathInternal() {
        static std::filesystem::path rootPath;
        if (!rootPath.empty()) return rootPath;

        // Use vector to allocate on heap to avoid C6262 stack warning
        const DWORD bufferSize = 32768;
        std::vector<wchar_t> buffer(bufferSize);
        DWORD length = GetModuleFileNameW(NULL, buffer.data(), bufferSize);
        if (length == 0 || length >= bufferSize) {
            // Fallback to current directory if failed
            rootPath = std::filesystem::current_path();
            return rootPath;
        }
        buffer[length] = L'\0'; // Ensure null-termination
        
        std::filesystem::path exeDir = std::filesystem::path(buffer.data()).parent_path();
        
        auto IsBuildFolder = [](const std::wstring& name) {
            std::wstring lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            return lower == L"generated" || lower == L"outputs" || lower == L"development" ||
                   lower == L"bin" || lower == L"obj" || lower == L"x64" || lower == L"debug" || lower == L"release";
        };

        // Phase 1: Search for project marker up to 10 levels
        std::filesystem::path current = exeDir;
        for (int i = 0; i < 10; ++i) {
            if (!IsBuildFolder(current.filename().wstring())) {
                try {
                    if (HasProjectFile(current) || std::filesystem::exists(current / ".git")) {
                        rootPath = current;
                        return rootPath;
                    }
                    // Check child "neo_Engine" folder
                    if (std::filesystem::exists(current / "neo_Engine") && 
                        (HasProjectFile(current / "neo_Engine") || std::filesystem::exists(current / "neo_Engine" / ".git"))) {
                        rootPath = current / "neo_Engine";
                        return rootPath;
                    }
                } catch (...) {}
            }
            if (current.has_parent_path() && current.parent_path() != current) {
                current = current.parent_path();
            } else break;
        }
        
        // Phase 2: Fallback search for Resources folder
        current = exeDir;
        for (int i = 0; i < 10; ++i) {
            try {
                if (std::filesystem::exists(current / "Resources")) {
                    rootPath = current;
                    return rootPath;
                }
            } catch (...) {}
            if (current.has_parent_path() && current.parent_path() != current) {
                current = current.parent_path();
            } else break;
        }
        
        rootPath = exeDir;
        return rootPath;
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};


} // namespace Engine
