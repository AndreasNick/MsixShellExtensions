# MsixContextMenuHandler — Design Document

## Problem Statement

Classic Win32 shell extensions (IContextMenu) packaged in MSIX fail when registered as
`com:SurrogateServer`. The DLL runs in `dllhost.exe`, which has no MSIX activation context.
The DLL cannot locate the packaged application's executable through the VFS, so menu entries
appear but do nothing when clicked.

**Example: WinRAR**
- `rarext.dll` is a `com:SurrogateServer` shell extension
- `dllhost.exe` has no access to `VFS\ProgramFilesX64\WinRAR\WinRAR.exe`
- COM surrogate fails silently → context menu entries are dead

**Why 7-Zip works**
7-Zip's shell extension (`7-zip.dll`) is registered as `com:InProcessServer`. It is loaded
directly into `Explorer.exe`. MSIX sets up the COM activation context so that in-process
COM objects registered via the manifest can resolve the package executable path via Windows
activation APIs. 7-Zip's DLL uses `GetCurrentPackagePath()` for this.

## Solution: Generic MSIX-Aware COM InProcess Shell Extension

Build a single configurable C++ DLL that:
1. Implements `IShellExtInit`, `IContextMenu`, `IContextMenu2`, `IContextMenu3`
2. Is registered as `com:InProcessServer` in the MSIX manifest
3. Reads a JSON config file from the package root using `GetCurrentPackagePath()`
4. On initialization receives the selected files via `IShellExtInit::Initialize`
5. Builds dynamic menu labels using the actual selected filenames (e.g. "Add to 'archive.rar'")
6. Launches the configured executable using `CreateProcess` or `ShellExecuteEx`

This DLL is **application-agnostic** — the same binary works for WinRAR, any archiver,
any tool that needs a context menu.

---

## Context Menu Variants

### Classic Win32 IContextMenu

Registered via `desktop9:fileExplorerClassicContextMenuHandler`. The DLL is loaded
into `Explorer.exe` (in-process). Full `IContextMenu` / `IContextMenu2` / `IContextMenu3`
support: submenus, icons, owner-draw, dynamic labels.

**Platform behaviour (source: Tim Mangan, tmurgent.com/TmBlog/?p=3376, Jan 2022):**
- **Windows 10:** `desktop9:fileExplorerClassicContextMenuHandler` is silently ignored.
  The extension is registered but never called. No context menu appears.
- **Windows 11:** The extension works. However, it appears only under "Show more options"
  (the secondary context menu reached with an extra click), not in the primary modern menu.

This is a platform limitation, not a bug in the implementation.

### Modern Windows 11 Context Menu (IExplorerCommand)

The native Windows 11 context menu (primary, no extra click) uses `IExplorerCommand` /
`IExplorerCommandProvider` instead of `IContextMenu`. To appear there natively:
- Implement `IExplorerCommand` and `IExplorerCommandProvider` in the same DLL
- Register via `shell\ContextMenuHandlers` pointing to the InProcessServer CLSID

Alternatively, a second CLSID in the same DLL handles `IExplorerCommand`. The JSON
config drives both paths identically.

**Priority for initial implementation:** Classic IContextMenu first (Win11 "Show more
options"). IExplorerCommand for the primary Win11 menu can be added later in the same
DLL without breaking changes.

### Combination with static uap3:Verb entries

For registered file types, `uap3:Verb` entries appear in the modern Win11 primary menu
without any extra click. These verbs are static (no dynamic labels, no submenu) but
provide immediate discoverability. The combination works well:

| Surface | Mechanism | Dynamic labels | Submenu |
|---|---|---|---|
| Modern Win11 primary menu | `uap3:Verb` (manifest) | No | No |
| Win11 "Show more options" | `MsixContextMenuHandler.dll` | Yes | Yes |
| Win10 | `MsixContextMenuHandler.dll` | — | — (silently ignored) |

On Windows 10, the static verbs (`uap2:SupportedVerbs`) provide the only context menu
integration available in an MSIX container without a full shell extension rewrite.

---

## Dynamic Menu Labels

The key sequence:

```
IShellExtInit::Initialize(pidlFolder, IDataObject*, hkeyProgID)
    → extract selected file paths from IDataObject (CFSTR_SHELLIDLIST or CF_HDROP)
    → store paths in member variable m_selectedFiles
    ↓
IContextMenu::QueryContextMenu(hMenu, ...)
    → compute labels using m_selectedFiles[0].stem()
    → e.g. "Add to 'archive.rar'" if one file selected
    → e.g. "Add to archive..." if multiple files selected
```

### Archive Name Computation

```cpp
std::wstring ComputeArchiveName(const std::vector<std::wstring>& files)
{
    if (files.size() == 1) {
        std::filesystem::path p(files[0]);
        return p.stem().wstring() + L".rar";   // configurable extension
    }
    // Multiple files: use parent folder name
    std::filesystem::path p(files[0]);
    return p.parent_path().filename().wstring() + L".rar";
}
```

The archive extension and whether to append it are controlled by the JSON config.

---

## Architecture

### COM Registration in Manifest

```xml
<com:Extension Category="windows.comServer">
  <com:ComServer>
    <com:InProcessServer>
      <com:Path>MsixContextMenuHandler.dll</com:Path>
      <com:Class Id="{CLSID-per-package}" ThreadingModel="Apartment"/>
    </com:InProcessServer>
  </com:ComServer>
</com:Extension>

<desktop9:Extension Category="windows.fileExplorerClassicContextMenuHandler">
  <desktop9:FileExplorerClassicContextMenuHandler>
    <desktop9:Clsid>{CLSID-per-package}</desktop9:Clsid>
  </desktop9:FileExplorerClassicContextMenuHandler>
</desktop9:Extension>
```

The CLSID must be unique per packaged application (generate a new GUID for each use).

### Config File: `MsixContextMenuHandler.json`

Placed in the package root (next to the DLL).

```json
{
  "menuTitle": "WinRAR",
  "executable": "VFS\\ProgramFilesX64\\WinRAR\\WinRAR.exe",
  "icon": "VFS\\ProgramFilesX64\\WinRAR\\WinRAR.exe",
  "iconIndex": 0,
  "archiveExtension": ".rar",
  "entries": [
    {
      "id": "add",
      "label": "Add to archive...",
      "labelWithFile": "Add to '{archive}'...",
      "args": "a \"{archive}\" \"{files}\""
    },
    {
      "id": "addEmail",
      "label": "Add and send by email...",
      "labelWithFile": "Add '{archive}' and send by email...",
      "args": "a -tk \"{archive}\" \"{files}\""
    },
    {
      "id": "extract",
      "label": "Extract here",
      "args": "x \"{files}\""
    },
    {
      "id": "extractTo",
      "label": "Extract to folder...",
      "args": "e -ad \"{files}\""
    }
  ],
  "fileTypes": [".rar", ".zip", ".7z", ".tar", ".gz"],
  "folders": true,
  "background": false
}
```

**Placeholder substitution in `args` and `labelWithFile`:**

| Placeholder | Replaced with |
|---|---|
| `{archive}` | Computed archive name (`stem + archiveExtension`) |
| `{files}` | Space-separated quoted paths of selected files |
| `{folder}` | Parent folder of selected file(s) |

**`fileTypes`** — handler is shown when right-clicking these extensions.
**`folders`** — handler is shown when right-clicking a folder.
**`background`** — handler is shown when right-clicking the folder background.

---

## Key C++ Implementation Points

### Finding the Package Root

```cpp
#include <appmodel.h>

std::wstring GetPackageRoot()
{
    UINT32 length = 0;
    GetCurrentPackagePath(&length, nullptr);
    std::wstring path(length, L'\0');
    GetCurrentPackagePath(&length, path.data());
    while (!path.empty() && path.back() == L'\0') path.pop_back();
    return path;
}
```

Works inside `Explorer.exe` for in-process COM objects registered via the MSIX manifest.

### IShellExtInit::Initialize — Capture Selected Files

```cpp
HRESULT Initialize(PCIDLIST_ABSOLUTE pidlFolder,
                   IDataObject* pdtobj, HKEY hkeyProgID)
{
    m_selectedFiles.clear();
    if (!pdtobj) return S_OK;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pdtobj->GetData(&fmt, &stg))) return S_OK;

    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
        std::wstring path(len + 1, L'\0');
        DragQueryFileW(hDrop, i, path.data(), len + 1);
        path.resize(len);
        m_selectedFiles.push_back(path);
    }
    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);
    return S_OK;
}
```

### IContextMenu::QueryContextMenu — Build the Submenu with Dynamic Labels

```cpp
HRESULT QueryContextMenu(HMENU hMenu, UINT indexMenu,
                         UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    std::wstring archiveName = ComputeArchiveName(m_selectedFiles);
    HMENU hSubMenu = CreatePopupMenu();

    for (size_t i = 0; i < m_config.entries.size(); ++i) {
        std::wstring label = (m_selectedFiles.size() == 1 && !m_config.entries[i].labelWithFile.empty())
            ? ReplacePlaceholders(m_config.entries[i].labelWithFile, archiveName)
            : m_config.entries[i].label;

        InsertMenuW(hSubMenu, (UINT)i, MF_BYPOSITION | MF_STRING,
                    idCmdFirst + i, label.c_str());
    }

    // Load icon from exe/ico
    HBITMAP hBitmap = LoadIconAsBitmap(m_config.icon, m_config.iconIndex);

    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask      = MIIM_SUBMENU | MIIM_STRING | MIIM_ID | MIIM_BITMAP;
    mii.hSubMenu   = hSubMenu;
    mii.dwTypeData = const_cast<LPWSTR>(m_config.menuTitle.c_str());
    mii.wID        = idCmdFirst + (UINT)m_config.entries.size();
    mii.hbmpItem   = hBitmap;
    InsertMenuItemW(hMenu, indexMenu, TRUE, &mii);

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)m_config.entries.size() + 1);
}
```

### IContextMenu::InvokeCommand — Launch the App

```cpp
HRESULT InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    UINT idx = LOWORD(pici->lpVerb);
    if (idx >= m_config.entries.size()) return E_INVALIDARG;

    std::wstring archiveName = ComputeArchiveName(m_selectedFiles);
    std::wstring exe     = m_packageRoot + L"\\" + m_config.executable;
    std::wstring args    = ReplacePlaceholders(m_config.entries[idx].args,
                                               archiveName, m_selectedFiles);
    std::wstring cmdLine = L"\"" + exe + L"\" " + args;

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
                   FALSE, 0, nullptr, nullptr, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return S_OK;
}
```

### DllGetClassObject and DllCanUnloadNow

Standard COM factory pattern. The class factory creates `CContextMenuHandler` instances.
Use a global reference count (`InterlockedIncrement`/`InterlockedDecrement`) for
`DllCanUnloadNow`.

### Threading Model

Use `"Apartment"` threading model. Shell extensions run on the STA thread of Explorer.

---

## Project Structure

```
MsixContextMenuHandler/
  Design.md                  <- this file
  src/
    dllmain.cpp              <- DLL entry point, DllGetClassObject, DllCanUnloadNow
    ClassFactory.h/cpp       <- IClassFactory implementation
    ContextMenuHandler.h/cpp <- IShellExtInit + IContextMenu/2/3 implementation
    Config.h/cpp             <- JSON config reader (manual parser, no external deps)
    PackagePath.h/cpp        <- GetCurrentPackagePath() wrapper
    Placeholders.h/cpp       <- {archive}/{files}/{folder} substitution
    resource.rc              <- version info
  MsixContextMenuHandler.vcxproj
  MsixContextMenuHandler.sln
```

---

## Combining with PsfFtaCom / PsfLauncher

When the packaged app is already shimmed with PSF (FileRedirectionFixup, RegLegacyFixup,
EnvVarFixup etc.), the context menu must launch the app **through the PSF launcher**, not
directly. Otherwise the fixups are not active for the process started from the context menu.

### InvokeCommand — launch via PsfLauncher

Instead of calling `WinRAR.exe` directly, call `<AppId>_PsfLauncherA.exe`. The PSF launcher
reads `config.json`, applies fixups, then starts the real executable with the correct
arguments.

```cpp
// Build the command line through the PSF launcher
std::wstring launcher = m_packageRoot + L"\\" + m_config.psfLauncher;
// e.g. "WINRAR_PsfLauncherA.exe"
std::wstring args = ReplacePlaceholders(m_config.entries[idx].args, archiveName, m_selectedFiles);
// The PSF launcher forwards unknown args to the configured executable
std::wstring cmdLine = L"\"" + launcher + L"\" " + args;
```

The JSON config gains a `psfLauncher` field (optional):

```json
{
  "menuTitle": "WinRAR",
  "psfLauncher": "WINRAR_PsfLauncherA.exe",
  "executable": "VFS\\ProgramFilesX64\\WinRAR\\WinRAR.exe",
  ...
}
```

When `psfLauncher` is absent, `InvokeCommand` launches `executable` directly (no PSF).

### PsfFtaCom interaction

`PsfFtaCom64.exe` is a separate PSF component that handles file type association verbs
defined in `config.json`. It is not involved in the IContextMenu path. Both can coexist:
- `PsfFtaCom` handles verbs triggered by `uap3:Verb` / FTA entries in the modern menu
- `MsixContextMenuHandler.dll` handles the classic IContextMenu submenu

The `processes[]` entry `.*_PsfFtaCom.*` in `config.json` ensures PsfFtaCom itself does
not receive fixup DLL injection (see `Add-MSXIXPSFShim` in MSIXForcelets).

---

## Anti-Disregardation (Tim Mangan, Feb 2024)

When a single COM server DLL must be registered under multiple CLSIDs (e.g. multiple
TypeLibs for the same file), MSIX rejects the manifest because `com:Path` must be unique
per `InProcessServer` block. The workaround: duplicate the DLL under different filenames.
MSIX uses single-instance storage (CIMfs / hash-based deduplication) so duplicates consume
no additional disk space on client systems.

Reference: tmurgent.com/TmBlog/?p=3721 — relevant if the same DLL needs to serve
multiple CLSIDs in the manifest.

---

## Build Requirements

- Visual Studio 2022 (or VS Build Tools 2022) with **C++ ATL** component
- Windows SDK 10.0.22000.0 or later (for `appmodel.h` / `GetCurrentPackagePath`)
- C++17 or later (`std::filesystem` for path manipulation)
- Target: x64 Release, static runtime (`/MT`) to avoid VC++ redist dependency
- No external libraries — JSON config is parsed manually (avoid nlohmann/json to keep the
  DLL free of external dependencies and small)

---

## MSIX Integration via MSIXForcelets

A new function `Add-MSIXContextMenuHandler` should:
1. Copy `MsixContextMenuHandler.dll` into the package root
2. Generate a unique CLSID for the package (`[System.Guid]::NewGuid()`)
3. Add `com:InProcessServer` extension to the manifest
4. Add `desktop9:fileExplorerClassicContextMenuHandler` extension
5. Write `MsixContextMenuHandler.json` with the app-specific config
6. Call `Add-MSIXManifestNamespace` for `com` and `desktop9`

---

## Comparison with Alternatives

| Approach | Submenu | Dynamic labels | Win11 native | Works in MSIX |
|---|---|---|---|---|
| com:SurrogateServer (e.g. rarext.dll) | Yes | Yes | Yes | No — dllhost.exe has no package context |
| PsfFtaCom | No | No | No | Yes — verb-based EXE launcher |
| uap3:Verb (static) | No | No | Yes | Yes — modern menu, contextual |
| **MsixContextMenuHandler (classic)** | **Yes** | **Yes** | No (Show more options) | **Yes** |
| MsixContextMenuHandler + IExplorerCommand | Yes | Yes | **Yes** | Yes — requires additional interface |
| App's own InProcessServer (e.g. 7-Zip) | Yes | Yes | Yes | Yes — requires app to use MSIX APIs |

---

## References

- [Shell Extension Handlers (MSDN)](https://learn.microsoft.com/en-us/windows/win32/shell/shell-exs)
- [Packaged COM servers (MSIX)](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-extensions#integrate-with-file-explorer)
- [GetCurrentPackagePath function](https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-getcurrentpackagepath)
- [IContextMenu interface](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-icontextmenu)
- [IExplorerCommand interface](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorercommand)
- 7-Zip source: `CPanelExt.cpp`, `ContextMenu.cpp` in `CPP/7zip/UI/Explorer/`
