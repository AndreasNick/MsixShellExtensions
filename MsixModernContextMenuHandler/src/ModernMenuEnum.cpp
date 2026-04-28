#include "ModernMenuEnum.h"

ModernMenuEnum::ModernMenuEnum(std::vector<IExplorerCommand*> items)
    : m_items(std::move(items))
{
    for (auto* p : m_items) p->AddRef();
}

ModernMenuEnum::~ModernMenuEnum()
{
    for (auto* p : m_items) p->Release();
}

STDMETHODIMP ModernMenuEnum::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IEnumExplorerCommand) {
        *ppv = static_cast<IEnumExplorerCommand*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ModernMenuEnum::Release()
{
    LONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ModernMenuEnum::Next(ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched)
{
    if (!pUICommand) return E_POINTER;

    ULONG fetched = 0;
    while (fetched < celt && m_index < m_items.size()) {
        pUICommand[fetched] = m_items[m_index];
        pUICommand[fetched]->AddRef();
        ++fetched;
        ++m_index;
    }

    if (pceltFetched) *pceltFetched = fetched;
    return fetched == celt ? S_OK : S_FALSE;
}

STDMETHODIMP ModernMenuEnum::Skip(ULONG celt)
{
    m_index = min(m_index + celt, m_items.size());
    return (m_index < m_items.size()) ? S_OK : S_FALSE;
}

STDMETHODIMP ModernMenuEnum::Reset()
{
    m_index = 0;
    return S_OK;
}

STDMETHODIMP ModernMenuEnum::Clone(IEnumExplorerCommand** ppenum)
{
    if (!ppenum) return E_POINTER;
    // The constructor AddRef's each item it receives, which is the correct ownership transfer.
    auto* clone = new (std::nothrow) ModernMenuEnum(m_items);
    if (!clone) return E_OUTOFMEMORY;
    clone->m_index = m_index;
    *ppenum = clone;
    return S_OK;
}
