# MsixContextMenuHandler

A config-driven C++ DLL that adds entries to the **classic Windows context menu** ("Show more options" on Windows 11) from within an MSIX package.

**Minimum OS: Windows 11 21H2 (Build 22000).** On Windows 10 the required `desktop9` manifest extension is silently ignored by the shell — the handler is never loaded.

Companion to [MsixModernContextMenuHandler](../MsixModernContextMenuHandler/) which targets the Windows 11 primary context menu (no extra click) instead.

## Why this exists

Classic shell extensions (`IContextMenu`) inside MSIX packages are registered as `com:SurrogateServer`, which means Windows hosts them in `dllhost.exe`. That process has no MSIX activation context, so the DLL cannot find the packaged executable through the VFS — menu entries appear but do nothing when clicked.

This DLL is registered as `com:InProcessServer` so it runs inside `explorer.exe` with full MSIX activation context. It reads its configuration from a JSON file next to the DLL, needing no registry access at all.

## What it does

- Implements `IShellExtInit`, `IContextMenu`, `IContextMenu2`, `IContextMenu3`
- Reads menu entries, icons, and the target executable from `MsixContextMenuHandler.json`
- Builds dynamic menu labels based on the selected files (e.g. "Add to 'archive.rar'")
- Launches the configured application via `CreateProcess` or optionally via a PSF launcher
- Works for files, folders, folder backgrounds — controlled by the JSON config

## Platform support

| OS | Build | Behaviour |
|---|---|---|
| Windows 11 21H2 | 22000 | Appears under "Show more options" (secondary menu, extra click required) |
| Windows 11 22H2 | 22621 | Same |
| Windows 11 23H2+ | 22631+ | Same |
| Windows 10 (any) | — | `desktop9:fileExplorerClassicContextMenuHandler` silently ignored — handler never called |

> `desktop9` was introduced with Windows 11. On Windows 10 the manifest remains valid (packaging does not fail), but the shell never activates the handler.

For the Windows 11 primary context menu (no extra click needed), use [MsixModernContextMenuHandler](../MsixModernContextMenuHandler/) instead.

## Building

Requirements: Visual Studio 2022, Windows SDK 10.0.22000+, C++17.

```powershell
.\Build-ContextMenuHandler.ps1          # increments patch version, builds Release|x64
.\Build-ContextMenuHandler.ps1 -Clean   # clean build
```

Output: `bin\Release\MsixContextMenuHandler.dll`

## Config file

Place `MsixContextMenuHandler.json` next to the DLL in the package. Example:

```json
{
  "clsid": "{YOUR-GUID-HERE}",
  "menuTitle": "MyApp",
  "executable": "VFS\\ProgramFilesX64\\MyApp\\MyApp.exe",
  "icon": "VFS\\ProgramFilesX64\\MyApp\\MyApp.exe",
  "iconIndex": 0,
  "archiveExtension": ".zip",
  "allFiles": true,
  "folders": true,
  "background": false,
  "entries": [
    {
      "id": "addToZip",
      "label": "Add to ZIP archive",
      "labelWithFile": "Add to \"{archive}.zip\"",
      "args": "a \"{folder}\\{archive}.zip\" {files}",
      "filesOnly": false,
      "extensions": []
    },
    {
      "id": "extractHere",
      "label": "Extract here",
      "args": "x {files} \"{folder}\\\"",
      "filesOnly": true,
      "extensions": [".zip", ".rar", ".7z"]
    }
  ]
}
```

### Placeholders in `args` and `labelWithFile`

| Placeholder | Replaced with |
|---|---|
| `{archive}` | Stem of the selected file + `archiveExtension`; for multiple files: parent folder name |
| `{files}` | Space-separated quoted full paths of all selected items |
| `{folder}` | Parent folder of the first selected item |

## MSIX manifest

Generate a unique CLSID for your package and add to `AppxManifest.xml`:

```xml
<com:Extension Category="windows.comServer">
  <com:ComServer>
    <com:InProcessServer>
      <com:Path>MsixContextMenuHandler.dll</com:Path>
      <com:Class Id="YOUR-GUID-HERE" ThreadingModel="Apartment"/>
    </com:InProcessServer>
  </com:ComServer>
</com:Extension>

<desktop9:Extension Category="windows.fileExplorerClassicContextMenuHandler">
  <desktop9:FileExplorerClassicContextMenuHandler>
    <desktop9:Clsid>{YOUR-GUID-HERE}</desktop9:Clsid>
  </desktop9:FileExplorerClassicContextMenuHandler>
</desktop9:Extension>
```

Required namespace declarations on `<Package>`:
- `xmlns:com="http://schemas.microsoft.com/appx/manifest/com/windows10"`
- `xmlns:desktop9="http://schemas.microsoft.com/appx/manifest/desktop/windows10/9"`

## Testing outside MSIX

```powershell
# 1. Build Release|x64 (copies DLL to Test\)

# 2. Place the target executable (e.g. WinRAR.exe) in Test\ so relative paths resolve.

# 3. Install (requires admin for HKLM):
.\Test\Install-ContextMenuHandler.ps1 -RestartExplorer

# 4. Right-click a file -> "Show more options" -> your submenu.

# 5. Uninstall:
.\Test\Uninstall-ContextMenuHandler.ps1 -RestartExplorer
```

## PSF

The DLL itself requires no PSF. If the application it launches needs PSF fixups (RegLegacy, MFR), set `"psfLauncher"` in the JSON to route invocations through PSFLauncher:

```json
{
  "psfLauncher": "MYAPP_PsfLauncherA.exe",
  ...
}
```

When `psfLauncher` is empty or absent, the application is launched directly.

## License

Copyright (c) 2026 Andreas Nick. Use at your own risk — no warranty, no support obligations. See [LICENSE](../LICENSE).
