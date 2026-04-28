#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <vector>

// IEnumExplorerCommand implementation.
// Holds a snapshot of IExplorerCommand* pointers and enumerates them in order.
// Each pointer is AddRef'd on insertion and Released on destruction.
class ModernMenuEnum : public IEnumExplorerCommand
{
public:
    explicit ModernMenuEnum(std::vector<IExplorerCommand*> items);
    ~ModernMenuEnum();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef()  override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override;

    // IEnumExplorerCommand
    STDMETHODIMP Next(ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched) override;
    STDMETHODIMP Skip(ULONG celt) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumExplorerCommand** ppenum) override;

private:
    LONG                          m_ref   = 1;
    std::vector<IExplorerCommand*> m_items;
    size_t                        m_index = 0;
};
