#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include "Config.h"

// Represents one leaf command in the modern (W11) context menu.
// Implements IExplorerCommand. The parent ModernMenuRoot holds a ref-counted
// list of these objects and returns them via IEnumExplorerCommand.
class ModernMenuEntry : public IExplorerCommand
{
public:
    ModernMenuEntry(const ConfigEntry& entry, const Config& cfg);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef()  override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override;

    // IExplorerCommand
    STDMETHODIMP GetTitle(IShellItemArray* psia, LPWSTR* ppszName) override;
    STDMETHODIMP GetIcon(IShellItemArray* psia, LPWSTR* ppszIcon) override;
    STDMETHODIMP GetToolTip(IShellItemArray* psia, LPWSTR* ppszInfotip) override;
    STDMETHODIMP GetCanonicalName(GUID* pguidCommandName) override;
    STDMETHODIMP GetState(IShellItemArray* psia, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override;
    STDMETHODIMP Invoke(IShellItemArray* psia, IBindCtx* pbc) override;
    STDMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override;
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override;

private:
    LONG        m_ref  = 1;
    ConfigEntry m_entry;
    Config      m_cfg;

    bool IsApplicable(IShellItemArray* psia) const;
};
