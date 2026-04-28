#include "ModernMenuEntry.h"
#include "ShellItems.h"
#include "Placeholders.h"
#include "Log.h"
#include <filesystem>
#include <algorithm>
#include <shlwapi.h>

ModernMenuEntry::ModernMenuEntry(const ConfigEntry& entry, const Config& cfg)
    : m_entry(entry), m_cfg(cfg)
{
}

STDMETHODIMP ModernMenuEntry::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
        *ppv = static_cast<IExplorerCommand*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ModernMenuEntry::Release()
{
    LONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ModernMenuEntry::GetTitle(IShellItemArray* psia, LPWSTR* ppszName)
{
    if (!ppszName) return E_POINTER;

    std::wstring title = m_entry.label;

    // Use labelWithFile when exactly one item is selected and the field is set
    if (!m_entry.labelWithFile.empty() && psia) {
        DWORD count = 0;
        if (SUCCEEDED(psia->GetCount(&count)) && count == 1) {
            auto files = GetFilesFromShellItemArray(psia);
            std::wstring archiveName = ComputeArchiveName(files, m_cfg.archiveExtension);
            title = ReplacePlaceholders(m_entry.labelWithFile, archiveName, files);
        }
    }

    return SHStrDupW(title.c_str(), ppszName);
}

STDMETHODIMP ModernMenuEntry::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (!ppszIcon) return E_POINTER;
    // Return the same icon as the root entry. Returning S_FALSE/nullptr for leaf entries
    // causes an access violation in Windows.UI.FileExplorer.dll (offset 0x81f3) before
    // the menu opens — the shell tries to dereference the icon pointer unconditionally.
    std::wstring iconSource = m_cfg.icon.empty() ? m_cfg.executable : m_cfg.icon;
    if (iconSource.empty()) {
        *ppszIcon = nullptr;
        return S_FALSE;
    }
    std::wstring spec = iconSource + L"," + std::to_wstring(m_cfg.iconIndex);
    return SHStrDupW(spec.c_str(), ppszIcon);
}

STDMETHODIMP ModernMenuEntry::GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip)
{
    if (!ppszInfotip) return E_POINTER;
    *ppszInfotip = nullptr;
    return S_FALSE;
}

STDMETHODIMP ModernMenuEntry::GetCanonicalName(GUID* pguidCommandName)
{
    if (!pguidCommandName) return E_POINTER;
    *pguidCommandName = GUID_NULL;
    return S_OK;
}

bool ModernMenuEntry::IsApplicable(IShellItemArray* psia) const
{
    if (!psia) return true;

    auto files = GetFilesFromShellItemArray(psia);

    // filesOnly: hide entry when folders are in the selection
    if (m_entry.filesOnly) {
        for (const auto& f : files) {
            if (PathIsDirectoryW(f.c_str())) return false;
        }
    }

    // extension filter: at least one selected item must match
    if (!m_entry.extensions.empty()) {
        bool anyMatch = false;
        for (const auto& f : files) {
            std::wstring ext = std::filesystem::path(f).extension().wstring();
            for (const auto& allowed : m_entry.extensions) {
                // case-insensitive compare
                if (_wcsicmp(ext.c_str(), allowed.c_str()) == 0) { anyMatch = true; break; }
            }
            if (anyMatch) break;
        }
        if (!anyMatch) return false;
    }

    return true;
}

STDMETHODIMP ModernMenuEntry::GetState(IShellItemArray* psia, BOOL /*fOkToBeSlow*/,
                                       EXPCMDSTATE* pCmdState)
{
    if (!pCmdState) return E_POINTER;
    bool applicable = IsApplicable(psia);
    *pCmdState = applicable ? ECS_ENABLED : ECS_HIDDEN;
    MCMH_LOGF("Entry '%ls': %s", m_entry.id.c_str(), applicable ? "ENABLED" : "HIDDEN");
    return S_OK;
}

STDMETHODIMP ModernMenuEntry::Invoke(IShellItemArray* psia, IBindCtx*)
{
    auto files = GetFilesFromShellItemArray(psia);
    std::wstring archiveName = ComputeArchiveName(files, m_cfg.archiveExtension);
    std::wstring args = ReplacePlaceholders(m_entry.args, archiveName, files);

    MCMH_LOGW(L"Invoke: " + m_entry.id + L" args=" + args);

    std::wstring exe;
    std::wstring fullArgs;

    if (!m_cfg.psfLauncher.empty() && !m_entry.useShellExecute) {
        exe      = m_cfg.psfLauncher;
        fullArgs = args;
    } else {
        exe      = m_cfg.executable;
        fullArgs = args;
    }

    MCMH_LOGW(L"Invoke exe: " + exe);

    // Always use ShellExecuteW. CreateProcessW from the Desktop Extension Host
    // bypasses MSIX activation: the child process runs without VFS and registry
    // virtualisation, so PSFLauncher and WinRAR both see the raw host filesystem.
    // ShellExecuteW triggers the MSIX app model for any executable inside
    // WindowsApps, giving the child the correct package context and fixups.
    ShellExecuteW(nullptr, L"open", exe.c_str(),
                  fullArgs.empty() ? nullptr : fullArgs.c_str(),
                  nullptr, SW_SHOWNORMAL);
    return S_OK;
}

STDMETHODIMP ModernMenuEntry::GetFlags(EXPCMDFLAGS* pFlags)
{
    if (!pFlags) return E_POINTER;
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

STDMETHODIMP ModernMenuEntry::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    if (!ppEnum) return E_POINTER;
    *ppEnum = nullptr;
    return E_NOTIMPL;
}
