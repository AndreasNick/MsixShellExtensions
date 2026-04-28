#include "ModernMenuRoot.h"
#include "ModernMenuEntry.h"
#include "ModernMenuEnum.h"
#include "Log.h"
#include <shlwapi.h>

ModernMenuRoot::ModernMenuRoot(const Config& cfg, const std::wstring& dllPath)
    : m_cfg(cfg), m_dllPath(dllPath)
{
}

STDMETHODIMP ModernMenuRoot::QueryInterface(REFIID riid, void** ppv)
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

STDMETHODIMP_(ULONG) ModernMenuRoot::Release()
{
    LONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ModernMenuRoot::GetTitle(IShellItemArray*, LPWSTR* ppszName)
{
    if (!ppszName) return E_POINTER;
    std::wstring title = m_cfg.menuTitle.empty() ? L"WinRAR" : m_cfg.menuTitle;
    return SHStrDupW(title.c_str(), ppszName);
}

STDMETHODIMP ModernMenuRoot::GetIcon(IShellItemArray*, LPWSTR* ppszIcon)
{
    if (!ppszIcon) return E_POINTER;

    // Icon spec format: "path,index"
    std::wstring iconSource = m_cfg.icon.empty() ? m_cfg.executable : m_cfg.icon;
    if (iconSource.empty()) {
        *ppszIcon = nullptr;
        return S_FALSE;
    }

    std::wstring spec = iconSource + L"," + std::to_wstring(m_cfg.iconIndex);
    return SHStrDupW(spec.c_str(), ppszIcon);
}

STDMETHODIMP ModernMenuRoot::GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip)
{
    if (!ppszInfotip) return E_POINTER;
    *ppszInfotip = nullptr;
    return S_FALSE;
}

STDMETHODIMP ModernMenuRoot::GetCanonicalName(GUID* pguidCommandName)
{
    if (!pguidCommandName) return E_POINTER;
    *pguidCommandName = GUID_NULL;
    return S_OK;
}

STDMETHODIMP ModernMenuRoot::GetState(IShellItemArray* psia, BOOL, EXPCMDSTATE* pCmdState)
{
    if (!pCmdState) return E_POINTER;
    DWORD count = 0;
    if (psia) psia->GetCount(&count);
    MCMH_LOGF("Root::GetState items=%lu", (unsigned long)count);
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

STDMETHODIMP ModernMenuRoot::Invoke(IShellItemArray*, IBindCtx*)
{
    // Root node is not directly invocable — it opens the submenu.
    return E_NOTIMPL;
}

STDMETHODIMP ModernMenuRoot::GetFlags(EXPCMDFLAGS* pFlags)
{
    if (!pFlags) return E_POINTER;
    MCMH_LOG("Root::GetFlags");
    *pFlags = ECF_HASSUBCOMMANDS;
    return S_OK;
}

STDMETHODIMP ModernMenuRoot::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    if (!ppEnum) return E_POINTER;

    MCMH_LOGF("Root::EnumSubCommands building %zu entries", m_cfg.entries.size());

    std::vector<IExplorerCommand*> cmds;
    cmds.reserve(m_cfg.entries.size());
    for (const auto& entry : m_cfg.entries) {
        auto* cmd = new (std::nothrow) ModernMenuEntry(entry, m_cfg);
        if (cmd) cmds.push_back(cmd);
    }

    // ModernMenuEnum ctor AddRef's each item it receives, becoming the owner.
    // We must Release our local refs after the enum takes ownership.
    auto* enumObj = new (std::nothrow) ModernMenuEnum(cmds);
    for (auto* p : cmds) p->Release();

    if (!enumObj) return E_OUTOFMEMORY;
    *ppEnum = enumObj;
    return S_OK;
}
