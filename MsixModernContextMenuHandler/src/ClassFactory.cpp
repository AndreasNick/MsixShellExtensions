#include "ClassFactory.h"
#include "ModernMenuRoot.h"
#include "Config.h"
#include "PackagePath.h"
#include "Log.h"
#include <new>

extern HMODULE g_hModule;
extern Config  g_config;

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::Release()
{
    LONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    MCMH_LOG("CreateInstance");

    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    auto* root = new (std::nothrow) ModernMenuRoot(g_config, dllPath);
    if (!root) return E_OUTOFMEMORY;

    HRESULT hr = root->QueryInterface(riid, ppv);
    root->Release();
    MCMH_LOGF("CreateInstance hr=0x%08X", (unsigned)hr);
    return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL /*fLock*/)
{
    return S_OK;
}
