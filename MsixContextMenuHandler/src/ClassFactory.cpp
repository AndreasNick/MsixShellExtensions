#include "ClassFactory.h"
#include "ContextMenuHandler.h"
#include <new>

extern long g_cDllRef;

CClassFactory::CClassFactory() : m_cRef(1) {}
CClassFactory::~CClassFactory() {}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_POINTER;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CClassFactory::Release()
{
    ULONG ref = InterlockedDecrement(&m_cRef);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_POINTER;
    *ppvObj = nullptr;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    CContextMenuHandler* handler = new (std::nothrow) CContextMenuHandler();
    if (!handler) return E_OUTOFMEMORY;

    HRESULT hr = handler->QueryInterface(riid, ppvObj);
    handler->Release();
    return hr;
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock)
{
    if (fLock) InterlockedIncrement(&g_cDllRef);
    else       InterlockedDecrement(&g_cDllRef);
    return S_OK;
}
