#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include "Config.h"

// Root IExplorerCommand object registered under the CLSID.
// Returns ECF_HASSUBCOMMANDS so Explorer shows a submenu arrow.
// EnumSubCommands() returns one ModernMenuEntry per ConfigEntry.
class ModernMenuRoot : public IExplorerCommand
{
public:
    explicit ModernMenuRoot(const Config& cfg, const std::wstring& dllPath);

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
    LONG         m_ref = 1;
    Config       m_cfg;
    std::wstring m_dllPath; // full path to this DLL, used for icon spec
};
