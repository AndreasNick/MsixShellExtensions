#include "ShellItems.h"
#include <filesystem>

std::vector<std::wstring> GetFilesFromShellItemArray(IShellItemArray* psia)
{
    std::vector<std::wstring> files;
    if (!psia) return files;

    DWORD count = 0;
    if (FAILED(psia->GetCount(&count))) return files;

    for (DWORD i = 0; i < count; ++i) {
        IShellItem* psi = nullptr;
        if (FAILED(psia->GetItemAt(i, &psi))) continue;

        wchar_t* pszPath = nullptr;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
            files.push_back(pszPath);
            CoTaskMemFree(pszPath);
        }
        psi->Release();
    }
    return files;
}

std::wstring ComputeArchiveName(const std::vector<std::wstring>& files,
                                const std::wstring& archiveExt)
{
    namespace fs = std::filesystem;

    std::wstring ext = archiveExt;
    if (!ext.empty() && ext[0] != L'.') ext = L"." + ext;

    if (files.empty()) return L"archive" + ext;

    if (files.size() == 1) {
        fs::path p(files[0]);
        return p.stem().wstring() + ext;
    }

    // Multiple items: use parent folder name
    fs::path parent = fs::path(files[0]).parent_path();
    std::wstring folderName = parent.filename().wstring();
    if (folderName.empty()) folderName = L"archive";
    return folderName + ext;
}
