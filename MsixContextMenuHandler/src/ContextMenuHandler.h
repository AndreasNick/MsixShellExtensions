#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include "Config.h"

class CContextMenuHandler
    : public IShellExtInit
    , public IContextMenu3
{
public:
    CContextMenuHandler();
    virtual ~CContextMenuHandler();

    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef()  override;
    STDMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder,
                            IDataObject* pdtobj, HKEY hkeyProgID) override;

    // IContextMenu
    STDMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu,
                                  UINT idCmdFirst, UINT idCmdLast, UINT uFlags) override;
    STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType,
                                  UINT* pReserved, CHAR* pszName, UINT cchMax) override;

    // IContextMenu2
    STDMETHODIMP HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // IContextMenu3
    STDMETHODIMP HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                LRESULT* plResult) override;

private:
    long                      m_cRef;
    std::vector<std::wstring> m_selectedFiles;
    Config                    m_config;
    std::wstring              m_configDir;
    HBITMAP                   m_hMenuBitmap;

    void         LoadConfig();
    std::wstring ComputeArchiveName() const;
    HBITMAP      LoadMenuBitmap() const;
};
