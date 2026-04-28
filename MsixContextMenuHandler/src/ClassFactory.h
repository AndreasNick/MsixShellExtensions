#pragma once
#include <windows.h>

class CClassFactory : public IClassFactory
{
public:
    CClassFactory();
    virtual ~CClassFactory();

    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef()  override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    long m_cRef;
};
