#include <windows.h>
#include <appmodel.h>
#include <olectl.h>
#include <shlwapi.h>
#include "Guid.h"
#include "Config.h"
#include "ClassFactory.h"
#include "PackagePath.h"
#include "Log.h"

HMODULE g_hModule = nullptr;
Config  g_config;
bool    g_debugLog = false;

// ---------------------------------------------------------------------------
// Self-registration helpers
// ---------------------------------------------------------------------------

static bool SetRegString(HKEY hRoot, const wchar_t* subKey,
                         const wchar_t* valueName, const wchar_t* data)
{
    HKEY hKey = nullptr;
    LONG r = RegCreateKeyExW(hRoot, subKey, 0, nullptr, 0,
                              KEY_WRITE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) return false;
    r = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(data),
                       static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

// Reads the CLSID from config or falls back to the compiled-in GUID.
static std::wstring GetConfiguredClsid()
{
    if (!g_config.clsid.empty()) return g_config.clsid;

    wchar_t buf[64] = {};
    StringFromGUID2(CLSID_ModernContextMenuHandler, buf, 64);
    return buf;
}

// ---------------------------------------------------------------------------
// Background-thread self-registration
// When the DLL is loaded by a non-MSIX process (e.g. Explorer before the
// shellex key tells it where the DLL lives), we register ourselves under
// HKCU\Software\Classes\CLSID so no elevation is required.
// ---------------------------------------------------------------------------

static void RegisterClsid(const wchar_t* dllPath, const std::wstring& clsid)
{
    std::wstring keyBase  = L"Software\\Classes\\CLSID\\" + clsid;
    std::wstring keyInpro = keyBase + L"\\InprocServer32";
    SetRegString(HKEY_CURRENT_USER, keyBase.c_str(),  nullptr,           L"MsixModernContextMenuHandler");
    SetRegString(HKEY_CURRENT_USER, keyInpro.c_str(), nullptr,           dllPath);
    SetRegString(HKEY_CURRENT_USER, keyInpro.c_str(), L"ThreadingModel", L"Apartment");
}

static DWORD WINAPI SelfRegisterThread(LPVOID)
{
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    std::wstring clsid = GetConfiguredClsid();
    RegisterClsid(dllPath, clsid);
    MCMH_LOGW(L"Self-registered CLSID: " + clsid);

    return 0;
}

static bool IsRunningInsideMsixContainer()
{
    // Check 1: current process has package identity (standard MSIX app process).
    wchar_t familyName[256] = {};
    UINT32  len = 256;
    if (GetCurrentPackageFamilyName(&len, familyName) == ERROR_SUCCESS)
        return true;

    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);
    wchar_t lower[MAX_PATH] = {};
    wcsncpy_s(lower, dllPath, MAX_PATH);
    CharLowerBuffW(lower, (DWORD)wcslen(lower));

    // Check 2: DLL was loaded from the standard WindowsApps directory.
    // The Desktop Extension Host (DEH) has no package identity itself but loads
    // the DLL from the package root in WindowsApps.
    if (wcsstr(lower, L"\\windowsapps\\") != nullptr)
        return true;

    // Check 3: AppxManifest.xml exists next to the DLL.
    // Our com:Class/@Path is a bare filename → package root → AppxManifest.xml
    // is in the same directory. Catches DEH on non-standard paths (e.g. different
    // drive, junction, staging area) where the WindowsApps check would miss.
    wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
    if (lastSlash) {
        std::wstring manifestPath(dllPath, lastSlash + 1);
        manifestPath += L"AppxManifest.xml";
        DWORD attrs = GetFileAttributesW(manifestPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hInst;
        DisableThreadLibraryCalls(hInst);

        std::wstring dir = GetConfigDirectory(hInst);
        std::wstring cfgPath = dir + L"\\MsixModernContextMenuHandler.json";

        g_config   = ReadConfig(cfgPath);
        g_debugLog = g_config.debug;

        // Resolve relative paths for executable, icon and psfLauncher against
        // the DLL's own directory. This way the JSON works both for testing
        // (DLL and exe next to each other) and inside MSIX (VFS paths provided
        // as absolute paths in the deployed JSON).
        auto resolveRelative = [&](std::wstring& path) {
            if (!path.empty() && PathIsRelativeW(path.c_str()))
                path = dir + L"\\" + path;
        };
        resolveRelative(g_config.executable);
        resolveRelative(g_config.icon);
        resolveRelative(g_config.psfLauncher);

        MCMH_LOGW(L"Loaded from: " + dir);
        MCMH_LOGF("Config valid: %d, entries: %zu", (int)g_config.valid, g_config.entries.size());

        // Register ourselves when loaded outside an MSIX container so Explorer
        // can find the DLL after the shellex keys are written by the install script.
        // If SelfRegisterThread fires unexpectedly inside MSIX it writes to real HKCU,
        // which triggers a shell notification and causes Explorer to flicker/restart.
        bool inMsix = IsRunningInsideMsixContainer();
        MCMH_LOGF("IsRunningInsideMsixContainer: %d  -> SelfRegisterThread: %s",
                  (int)inMsix, inMsix ? "suppressed" : "FIRING");
        if (!inMsix) {
            HANDLE hThread = CreateThread(nullptr, 0, SelfRegisterThread, nullptr, 0, nullptr);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// COM exports
// ---------------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!ppv) return E_POINTER;

    wchar_t clsidStr[64] = {};
    StringFromGUID2(rclsid, clsidStr, 64);
    MCMH_LOGW(L"DllGetClassObject CLSID=" + std::wstring(clsidStr));

    // Accept the compiled-in file CLSID, directory CLSID, and any override from config.
    CLSID cfgClsid = CLSID_ModernContextMenuHandler;
    if (!g_config.clsid.empty()) {
        CLSIDFromString(g_config.clsid.c_str(), &cfgClsid);
    }

    if (rclsid != CLSID_ModernContextMenuHandler &&
        rclsid != cfgClsid) {
        MCMH_LOG("DllGetClassObject: CLSID mismatch -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    MCMH_LOGF("DllGetClassObject hr=0x%08X", (unsigned)hr);
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return S_OK; // simplified — no lock counter needed for this use case
}

STDAPI DllRegisterServer()
{
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    std::wstring clsid = GetConfiguredClsid();
    RegisterClsid(dllPath, clsid);

    return S_OK;
}

STDAPI DllUnregisterServer()
{
    std::wstring clsid = GetConfiguredClsid();
    RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\CLSID\\" + clsid).c_str());

    return S_OK;
}
