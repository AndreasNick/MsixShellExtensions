#include "PackagePath.h"

// Returns the directory that contains this DLL.
// GetModuleFileNameW returns the real on-disk path of the loaded module —
// even inside an MSIX container this is the full path under WindowsApps
// (e.g. C:\Program Files\WindowsApps\...\VFS\ProgramFilesX64\WinRAR\).
// MsixContextMenuHandler.json is placed next to the DLL, so this is always correct.
// GetCurrentPackagePath() was previously used here but it returns the package root
// (C:\Program Files\WindowsApps\...\), which does NOT include the VFS subpath
// where the JSON actually lives.
std::wstring GetConfigDirectory(HMODULE hModule)
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(hModule, buf, MAX_PATH);
    std::wstring path(buf);
    size_t sep = path.rfind(L'\\');
    if (sep != std::wstring::npos)
        path = path.substr(0, sep);
    return path;
}
