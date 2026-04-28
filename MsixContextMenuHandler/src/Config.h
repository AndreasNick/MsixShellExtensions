#pragma once
#include <string>
#include <vector>

struct ConfigEntry {
    std::wstring              id;
    std::wstring              label;
    std::wstring              labelWithFile;  // shown when exactly one file is selected; may be empty
    std::wstring              args;           // supports {archive}, {files}, {folder}
    bool                      filesOnly       = false; // hide when selection contains only directories
    bool                      useShellExecute = false; // use ShellExecuteW instead of CreateProcessW
    std::vector<std::wstring> extensions;              // if non-empty: show only when ALL selected files match
};

struct Config {
    std::wstring              clsid;           // optional: overrides the compiled-in CLSID
    std::wstring              menuTitle        = L"App";
    std::wstring              executable;      // relative to package/DLL root, or absolute
    std::wstring              psfLauncher;     // optional: launch via PSF launcher instead
    std::wstring              icon;            // exe or ico path for the submenu icon
    int                       iconIndex        = 0;
    int                       iconSize         = 0; // px; 0 = auto (SM_CXSMICON)
    std::wstring              archiveExtension = L".zip";
    std::vector<ConfigEntry>  entries;
    std::vector<std::wstring> fileTypes;       // extensions to register for (.rar, .zip, ...)
    bool                      allFiles         = false; // register under HKCU\Classes\*
    bool                      folders          = true;
    bool                      background       = false;
    bool                      recycleBin       = false; // register for Recycle Bin ({645FF040-...})
    bool                      debug            = false; // emit OutputDebugString traces (DebugView)
    bool                      valid            = false;
};

Config ReadConfig(const std::wstring& path);
