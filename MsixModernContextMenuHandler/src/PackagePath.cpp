#include "PackagePath.h"

// Returns the directory that contains this DLL.
// GetModuleFileNameW gives the real on-disk path of the loaded module —
// inside an MSIX container this includes the full VFS subpath under WindowsApps.
// MsixModernContextMenuHandler.json is placed next to the DLL, so this is correct.
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
