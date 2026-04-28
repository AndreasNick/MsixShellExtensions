# MsixModernContextMenuHandler

A config-file-driven shell extension DLL that adds entries to the **Windows 11 modern context menu** (the top section, directly visible without clicking "Show more options").

**Minimum OS: Windows 11 21H2 (Build 22000).** The modern context menu does not exist on Windows 10. The `desktop4`/`desktop5` manifest namespaces are schema-valid on late Windows 10 builds so packaging succeeds, but Explorer never activates the handler.

Companion to [MsixContextMenuHandler](../MsixContextMenuHandler/) (classic IContextMenu, "Show more options"). Both share the same JSON config format.

## Architecture

| | MsixContextMenuHandler (classic) | MsixModernContextMenuHandler (modern) |
|---|---|---|
| COM interface | `IContextMenu` + `IShellExtInit` | `IExplorerCommand` + `IEnumExplorerCommand` |
| Host process | dllhost.exe (out-of-process surrogate) | explorer.exe (in-process, Apartment) |
| Selection input | `IDataObject` | `IShellItemArray` |
| Registry hook | `ContextMenuHandlers` | `ExplorerCommandHandler` |
| Submenu | Popup via `QueryContextMenu` | `ECF_HASSUBCOMMANDS` + `EnumSubCommands` |
| Dynamic labels | Limited | `GetTitle(IShellItemArray*)` receives live selection |

### IExplorerCommand objects

```
ModernMenuRoot          IExplorerCommand root — GetFlags() returns ECF_HASSUBCOMMANDS
  └── ModernMenuEntry   one per config "entries" array entry
ModernMenuEnum          IEnumExplorerCommand returned by ModernMenuRoot::EnumSubCommands()
ClassFactory            IClassFactory registered under the configured CLSID
```

### Self-registration

When loaded outside an MSIX container (i.e. during `regsvr32` or first Explorer load),
the DLL detects the non-package context via `GetCurrentPackageFamilyName` and registers
itself under `HKCU\Software\Classes\CLSID\{...}\InprocServer32` on a background thread.
No elevation is required.

## Config file

`MsixModernContextMenuHandler.json` must be placed next to the DLL. The DLL uses
`GetModuleFileNameW` to locate it — not `GetCurrentPackagePath()`, so it works both
inside and outside MSIX containers.

### Fields

| Field | Type | Description |
|---|---|---|
| `clsid` | string | GUID for this handler (overrides compiled-in default) |
| `menuTitle` | string | Root menu entry label (e.g. "WinRAR") |
| `executable` | string | Full path to the target application |
| `psfLauncher` | string | Optional: PSFLauncher64.exe path (MSIX PSF deployments) |
| `icon` | string | Path to file containing the icon (exe or dll); empty = use executable |
| `iconIndex` | int | Icon index within the icon file |
| `iconSize` | int | Icon size in pixels; 0 = system default |
| `archiveExtension` | string | Default archive extension for computed archive name (e.g. ".rar") |
| `allFiles` | bool | Show on all files (`*`) |
| `folders` | bool | Show on folders and drives |
| `background` | bool | Show on directory background |
| `recycleBin` | bool | Show on Recycle Bin |
| `fileTypes` | string[] | Specific file extensions to register (e.g. [".rar", ".zip"]) |
| `debug` | bool | Write debug output to OutputDebugString |
| `entries` | array | Menu entries (see below) |

### Entry fields

| Field | Type | Description |
|---|---|---|
| `id` | string | Unique identifier (not shown to user) |
| `label` | string | Menu label for multiple-file selection |
| `labelWithFile` | string | Menu label when exactly 1 item is selected; `{archive}` is replaced with the computed archive name |
| `args` | string | Command-line arguments; `{archive}`, `{files}`, `{folder}` are replaced |
| `filesOnly` | bool | Hide entry when folders are in the selection |
| `useShellExecute` | bool | Use `ShellExecuteW` instead of `CreateProcessW` |
| `extensions` | string[] | Only show when at least one selected item has a matching extension; empty = always show |

### Placeholders

| Placeholder | Replaced with |
|---|---|
| `{archive}` | Computed archive name: for 1 file → `stem + archiveExtension`; for N files → `parent-folder-name + archiveExtension` |
| `{files}` | Space-separated quoted full paths of all selected items |
| `{folder}` | Parent folder of the first selected item |

## Building

Open `MsixModernContextMenuHandler.sln` in Visual Studio 2022. Build `Release|x64`.

The DLL output goes to `bin\Release\MsixModernContextMenuHandler.dll`.

**Requirements:** Visual Studio 2022 (v143 toolset), Windows 11 SDK, C++17.

## Testing outside MSIX

The Windows 11 modern context menu host only loads `IExplorerCommand` handlers that are
registered via an MSIX manifest — plain Win32 registry entries are ignored for the top section.
Outside MSIX, the install script registers the handler in HKLM, which causes it to appear in
the classic "Show more options" flyout instead. This is sufficient to verify that the DLL
loads correctly and that menu entries, labels, and command invocation work.

> Requires Windows 11 21H2 (Build 22000) or later for testing. On Windows 10 the handler
> is never called even in the classic flyout.
> NOT FULL WORKING OUTSIDE OF AN MSIX!

```powershell
# 1. Build Release|x64 (copies DLL to Test\)

# 2. Copy WinRAR.exe next to the DLL so relative paths in the JSON resolve correctly.

# 3. Install (requires admin for HKLM):
.\Test\Install-ModernContextMenuHandler.ps1 -RestartExplorer

# 4. Right-click a file or folder -> "Show more options" -> WinRAR submenu.
#    The modern top-section placement requires MSIX deployment (see above).

# 5. Uninstall:
.\Test\Uninstall-ModernContextMenuHandler.ps1 -RestartExplorer
```

## Deployment inside MSIX

The modern Windows 11 context menu only shows `IExplorerCommand` handlers that are
registered via an MSIX manifest. Plain Win32 registry entries (even in HKLM) are
ignored by the modern menu host.

### How it works

```
Explorer reads AppModel manifest → finds desktop4:FileExplorerContextMenus entry
  → activates CLSID via AppModel COM
  → com4:InProcessServer → loads DLL in-process inside explorer.exe
  → IExplorerCommand calls → modern menu entries appear
```

User.dat (the virtual HKCU inside the MSIX container) is **never visible** to
`explorer.exe`. The manifest is the only way to get the handler into Explorer.
No startup script, no UnvirtualizedResources, no registry workarounds.

### Manifest

1. Include `MsixModernContextMenuHandler.dll` and `MsixModernContextMenuHandler.json`
   in the package at the same VFS path (e.g. `VFS\ProgramFilesX64\WinRAR\`).

2. Add the following to `AppxManifest.xml` (declare namespaces on the root `<Package>`):

```xml
<!-- com4 namespace: http://schemas.microsoft.com/appx/manifest/com/windows10/4  -->
<!-- Path is on InProcessServer (not on Class); Id is uppercase GUID without braces -->
<com4:Extension Category="windows.comServer">
  <com4:ComServer>
    <com4:InProcessServer Path="VFS\ProgramFilesX64\WinRAR\MsixModernContextMenuHandler.dll">
      <com4:Class Id="4A7B3C1D-E2F5-4689-ABCD-EF1234567891" ThreadingModel="STA"/>
    </com4:InProcessServer>
  </com4:ComServer>
</com4:Extension>

<!-- desktop4/5 namespaces as usual -->
<!-- Use desktop5:ItemType for both * and Directory (supports all types).         -->
<!-- Use DISTINCT Verb Ids per ItemType — same Id causes Explorer to create two   -->
<!-- instances of the handler (observed; MS Learn Answers Q1120506).              -->
<desktop4:Extension Category="windows.fileExplorerContextMenus">
  <desktop4:FileExplorerContextMenus>
    <desktop5:ItemType Type="*">
      <desktop5:Verb Id="ModernMenuFile" Clsid="4A7B3C1D-E2F5-4689-ABCD-EF1234567891"/>
    </desktop5:ItemType>
    <desktop5:ItemType Type="Directory">
      <desktop5:Verb Id="ModernMenuDir" Clsid="4A7B3C1D-E2F5-4689-ABCD-EF1234567891"/>
    </desktop5:ItemType>
  </desktop4:FileExplorerContextMenus>
</desktop4:Extension>
```

> **Reference:** [How to create a shell extension using IExplorerCommand (MS Learn Answers)](https://learn.microsoft.com/en-us/answers/questions/1120506/how-to-create-a-shell-extension-using-iexplorercom)

### Avoiding duplicate menu entries

Two independent causes can produce duplicate context menu entries:

**Cause 1 — Same Verb Id across ItemTypes**

Using the same `Id` attribute on both `desktop5:ItemType Type="*"` and
`desktop5:ItemType Type="Directory"` causes Explorer to instantiate the handler twice
for each right-click. Fix: always use distinct Ids (e.g. `ModernMenuFile` / `ModernMenuDir`).
This is shown in the manifest example above.

**Cause 2 — PSF shimming processes the handler extensions (package-specific)**

If the target MSIX package is also being patched with a PSF framework (e.g. 
PSFLauncher for registry fixups), and the `windows.comServer` and
`windows.fileExplorerContextMenus` extensions sit under the same Application as the PSF
shimmed executable, PSF will create a PSFLauncher entry for those extensions as well.
The resulting launcher starts with the wrong `ApplicationId` and cannot find its launch
target in `config.json` — it exits with "No Matches" and nothing happens.

**Solution**: move both `com4:InProcessServer` and `desktop4:FileExplorerContextMenus`
into a separate hidden Application (`AppListEntry="none"`) **after** PSF shimming has
been applied to the main application. PSF never processes the hidden Application, so
no unwanted launcher is created.

```xml
<!-- Hidden Application — hosts the modern context menu handler only.        -->
<!-- AppListEntry="none" suppresses the Start menu entry.                    -->
<!-- Both com4 and desktop4 extensions go here when PSF is active.           -->
<!-- The DEH rule (extensions under visible Application) applies to          -->
<!-- desktop9 only — desktop4/5 has no DEH involvement.                     -->
<Application Id="MenuHandlerHelper" Executable="..." EntryPoint="Windows.FullTrustApplication">
  <uap:VisualElements AppListEntry="none" DisplayName="..." .../>
  <Extensions>
    <com4:Extension Category="windows.comServer">
      <com4:ComServer>
        <com4:InProcessServer Path="...MsixModernContextMenuHandler.dll">
          <com4:Class Id="..." ThreadingModel="STA"/>
        </com4:InProcessServer>
      </com4:ComServer>
    </com4:Extension>
    <desktop4:Extension Category="windows.fileExplorerContextMenus">
      <desktop4:FileExplorerContextMenus>
        <desktop5:ItemType Type="*">
          <desktop5:Verb Id="ModernMenuFile" Clsid="..."/>
        </desktop5:ItemType>
        <desktop5:ItemType Type="Directory">
          <desktop5:Verb Id="ModernMenuDir" Clsid="..."/>
        </desktop5:ItemType>
      </desktop4:FileExplorerContextMenus>
    </desktop4:Extension>
  </Extensions>
</Application>
```

This hidden Application pattern is **only needed when PSF is active** on the same package.
For packages without PSF, both extensions can sit under the visible main Application.

### Automation with MSIXForcelets

The [MSIXForcelets](https://github.com/AndreasNick/MSIXForcelets) PowerShell module includes
`Add-MSIXFixWinRARModernShell`, which applies all of the above steps automatically for WinRAR MSIX
packages. For other applications, use it as a reference implementation.

## Debugging

Enable `"debug": true` in `MsixModernContextMenuHandler.json` to activate
`OutputDebugString` logging in the DLL.

Capture output with [Sysinternals DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview):

```
DebugView → Capture → Capture Global Win32
```

Log prefix: `[MsixModernContextMenu]`

### What to look for

| Symptom | Likely cause |
|---------|-------------|
| No DebugView output at all on right-click | DLL is not being loaded — check manifest (com4:InProcessServer Path, CLSID) |
| `[MsixModernContextMenu] Init` appears but no menu entries | JSON not found next to DLL, or `allFiles`/`folders` are false |
| Menu entries appear but click does nothing | `executable` path in JSON wrong or not found |
| Explorer crashes on right-click | DLL bug — check Event Viewer → Windows Logs → Application, source: Application Error |
| Context menu completely frozen | Wrong ThreadingModel, or DLL blocks during load |

### Verifying the manifest after packaging

```powershell
# Extract and inspect the manifest
$msix = 'C:\path\to\WinRAR_fixed.msix'
$tmp  = "$env:TEMP\msix_inspect"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($msix, $tmp)
notepad "$tmp\AppxManifest.xml"
```

Check that the manifest contains:
- `com4:InProcessServer` with correct `Path` and `ThreadingModel="STA"`
- `desktop4:FileExplorerContextMenus` with two `desktop5:ItemType` entries
- Both under the **visible** Application entry (not under `AppListEntry="none"`)

## PSF requirements

| Component | Needed? | Reason |
|---|---|---|
| PsfFtaCom | **No** | Was a workaround for RarExt.dll running outside the container via dllhost. MsixModernContextMenuHandler runs in-process in Explorer and reads config from JSON, not from the virtual registry. |
| PSFLauncher (for handler DLL) | **No** | The DLL itself needs no PSF shim. |
| PSF RegLegacy / MFR | Depends | Still needed for **WinRAR.exe** itself to run correctly inside the container, but this is independent of the context menu handler. |

### Why PsfFtaCom is no longer needed

The previous approach (V2, `Add-MSIXFixWinRARv2`) routed `RarExt.dll` through PsfFtaCom
to host the classic `IContextMenu` interface inside the container:

```
RarExt.dll (IContextMenu)
  -> com:SurrogateServer
    -> dllhost.exe runs OUTSIDE the container
      -> cannot see User.dat (virtual HKCU)
        -> PsfFtaCom as in-container surrogate to fix this
```

MsixModernContextMenuHandler eliminates this chain entirely:

```
MsixModernContextMenuHandler.dll (IExplorerCommand)
  -> com:InProcessServer in Explorer.exe
    -> reads config from JSON file (no registry access needed)
      -> launches WinRAR.exe via ShellExecuteW (triggers MSIX app model activation)
```

Because the config comes from a JSON file next to the DLL, the virtual registry is
never consulted. PsfFtaCom, com:SurrogateServer, and the loader-search-path override
are all unnecessary.

## Differences from MsixContextMenuHandler

- Uses `IExplorerCommand` instead of `IContextMenu` — entries appear in the **modern**
  top section of the Windows 11 context menu, not in the "Show more options" flyout.
- No icon bitmap rendering — icons are specified as `"path,index"` strings returned by
  `GetIcon()`, which Explorer renders natively.
- Selection is received as `IShellItemArray*` via `GetTitle`/`GetState`/`Invoke` —
  no `IShellExtInit::Initialize` step.
- Loader-search-path override and DynamicLibraryFixup are not needed since the DLL
  runs in-process inside Explorer rather than via dllhost.

## License

Copyright (c) 2026 Andreas Nick. Use at your own risk — no warranty, no support obligations. See [LICENSE](../LICENSE).
