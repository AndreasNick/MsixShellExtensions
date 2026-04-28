#pragma once
#include <string>
#include <vector>

struct ConfigEntry
{
    std::wstring id;
    std::wstring label;           // static label
    std::wstring labelWithFile;   // label when exactly one file is selected, {archive} replaced
    std::wstring args;            // arguments template
    bool         filesOnly     = false;
    bool         useShellExecute = false;
    std::vector<std::wstring> extensions; // e.g. {L".rar", L".zip"} — empty = all
};

struct Config
{
    // Registration
    std::wstring clsid;            // overrides compiled-in CLSID when non-empty
    std::wstring menuTitle;        // root submenu label

    // Launch
    std::wstring executable;       // full path to the target exe
    std::wstring psfLauncher;      // optional: path to PSFLauncher64.exe

    // Icon (shell icon for the root menu entry)
    std::wstring icon;             // path to exe/dll containing the icon
    int          iconIndex = 0;    // icon index within that file
    int          iconSize  = 0;    // 0 = use system default (16px for classic, 20px for modern)

    // Behaviour
    std::wstring archiveExtension; // default archive extension for ComputeArchiveName
    bool         allFiles    = false;
    bool         folders     = false;
    bool         background  = false;
    bool         recycleBin  = false;
    bool         debug       = false;
    bool         valid       = false;

    std::vector<std::wstring> fileTypes; // specific extensions for file association
    std::vector<ConfigEntry>  entries;
};

Config ReadConfig(const std::wstring& path);
