// INITGUID causes DEFINE_GUID to emit the actual storage for all GUIDs included
// in this translation unit. Must appear before any guiddef.h / windows.h include.
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include <new>
#include "ClassFactory.h"
#include "Config.h"
#include "Guid.h"
#include "Log.h"
#include "PackagePath.h"

HMODULE g_hModule  = nullptr;
long    g_cDllRef  = 0;
bool    g_debugLog = false;

// CLSID resolved at first use from MsixContextMenuHandler.json ("clsid" field).
// Falls back to the compiled-in CLSID_ContextMenuHandler when absent.
static CLSID s_clsid            = {};
static bool  s_clsidInitialized = false;

static const CLSID& GetConfiguredClsid()
{
    if (!s_clsidInitialized) {
        s_clsidInitialized = true;
        std::wstring dir  = GetConfigDirectory(g_hModule);
        std::wstring path = dir + L"\\MsixContextMenuHandler.json";
        Config cfg = ReadConfig(path);
        g_debugLog = cfg.debug;
        CMH_LOGF(L"GetConfiguredClsid: config=%s valid=%d debug=%d",
                 path.c_str(), (int)cfg.valid, (int)cfg.debug);
        bool parsed = !cfg.clsid.empty() &&
                      SUCCEEDED(CLSIDFromString(cfg.clsid.c_str(), &s_clsid));
        if (!parsed) {
            s_clsid = CLSID_ContextMenuHandler; // compiled-in fallback
            CMH_LOG(L"GetConfiguredClsid: using compiled-in fallback CLSID");
        }
    }
    return s_clsid;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CMH_LOG(L"DLL_PROCESS_ATTACH");
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// COM exports
// ---------------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, GetConfiguredClsid()))
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* factory = new (std::nothrow) CClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

// ---------------------------------------------------------------------------
// Registration helpers
// ---------------------------------------------------------------------------

static HRESULT SetRegString(HKEY hRoot, LPCWSTR subKey,
                            LPCWSTR valueName, LPCWSTR data)
{
    HKEY hKey = nullptr;
    LSTATUS ls = RegCreateKeyExW(hRoot, subKey, 0, nullptr,
                                 REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                 nullptr, &hKey, nullptr);
    if (ls != ERROR_SUCCESS) return HRESULT_FROM_WIN32(ls);

    ls = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(data),
                        (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(ls);
}

// ---------------------------------------------------------------------------
// DllRegisterServer — registers the COM InprocServer32 under HKCU.
// No administrator rights required.
// File-type associations are handled by Install-ContextMenuHandler.ps1.
// ---------------------------------------------------------------------------

STDAPI DllRegisterServer()
{
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    wchar_t clsidStr[64] = {};
    StringFromGUID2(GetConfiguredClsid(), clsidStr, 64);

    // HKCU\Software\Classes\CLSID\{...}
    wchar_t keyClsid[160];
    StringCchPrintfW(keyClsid, 160, L"Software\\Classes\\CLSID\\%s", clsidStr);
    HRESULT hr = SetRegString(HKEY_CURRENT_USER, keyClsid,
                              nullptr, L"MsixContextMenuHandler");
    if (FAILED(hr)) return hr;

    // HKCU\Software\Classes\CLSID\{...}\InprocServer32
    wchar_t keyInproc[200];
    StringCchPrintfW(keyInproc, 200,
                     L"Software\\Classes\\CLSID\\%s\\InprocServer32", clsidStr);
    hr = SetRegString(HKEY_CURRENT_USER, keyInproc, nullptr, dllPath);
    if (FAILED(hr)) return hr;
    hr = SetRegString(HKEY_CURRENT_USER, keyInproc, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

// ---------------------------------------------------------------------------
// DllUnregisterServer — removes the CLSID registration from HKCU.
// File-type associations are removed by Uninstall-ContextMenuHandler.ps1.
// ---------------------------------------------------------------------------

STDAPI DllUnregisterServer()
{
    wchar_t clsidStr[64] = {};
    StringFromGUID2(GetConfiguredClsid(), clsidStr, 64);

    wchar_t keyClsid[160];
    StringCchPrintfW(keyClsid, 160, L"Software\\Classes\\CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CURRENT_USER, keyClsid);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
