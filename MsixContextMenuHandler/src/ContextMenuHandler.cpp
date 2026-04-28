#include "ContextMenuHandler.h"
#include "Config.h"
#include "Log.h"
#include "PackagePath.h"
#include "Placeholders.h"
#include <filesystem>
#include <strsafe.h>
#include <new>

extern HMODULE g_hModule;
extern long    g_cDllRef;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CContextMenuHandler::CContextMenuHandler() : m_cRef(1), m_hMenuBitmap(nullptr)
{
    InterlockedIncrement(&g_cDllRef);
}

CContextMenuHandler::~CContextMenuHandler()
{
    if (m_hMenuBitmap) { DeleteObject(m_hMenuBitmap); m_hMenuBitmap = nullptr; }
    InterlockedDecrement(&g_cDllRef);
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------

STDMETHODIMP CContextMenuHandler::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_POINTER;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown)    ||
        IsEqualIID(riid, IID_IContextMenu) ||
        IsEqualIID(riid, IID_IContextMenu2)||
        IsEqualIID(riid, IID_IContextMenu3))
    {
        *ppvObj = static_cast<IContextMenu3*>(this);
    }
    else if (IsEqualIID(riid, IID_IShellExtInit))
    {
        *ppvObj = static_cast<IShellExtInit*>(this);
    }
    else
    {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CContextMenuHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CContextMenuHandler::Release()
{
    ULONG ref = InterlockedDecrement(&m_cRef);
    if (ref == 0) delete this;
    return ref;
}

// ---------------------------------------------------------------------------
// IShellExtInit
// ---------------------------------------------------------------------------

STDMETHODIMP CContextMenuHandler::Initialize(
    PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pdtobj, HKEY /*hkeyProgID*/)
{
    m_selectedFiles.clear();

    if (pdtobj) {
        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg = {};
        if (SUCCEEDED(pdtobj->GetData(&fmt, &stg))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
            if (hDrop) {
                UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    std::wstring path(len + 1, L'\0');
                    DragQueryFileW(hDrop, i, path.data(), len + 1);
                    path.resize(len);
                    m_selectedFiles.push_back(path);
                }
                GlobalUnlock(stg.hGlobal);
            }
            ReleaseStgMedium(&stg);
        }
    }

    // When no data object (e.g. folder tree-pane click): resolve pidlFolder to a real path.
    // Virtual shell folders (Recycle Bin, Desktop, Network, ...) have no file system path
    // and are intentionally excluded so no menu appears for them.
    if (m_selectedFiles.empty() && pidlFolder) {
        wchar_t path[MAX_PATH] = {};
        if (SHGetPathFromIDListW(pidlFolder, path) && path[0] != L'\0')
            m_selectedFiles.emplace_back(path);
    }

    CMH_LOGF(L"Initialize: %zu item(s) selected", m_selectedFiles.size());
    for (const auto& f : m_selectedFiles)
        CMH_LOGF(L"  -> %s", f.c_str());

    LoadConfig();
    return S_OK;
}

void CContextMenuHandler::LoadConfig()
{
    m_configDir = GetConfigDirectory(g_hModule);
    std::wstring configPath = m_configDir + L"\\MsixContextMenuHandler.json";
    m_config = ReadConfig(configPath);
    CMH_LOGF(L"LoadConfig: path=%s valid=%d entries=%zu",
             configPath.c_str(), (int)m_config.valid, m_config.entries.size());
}

HBITMAP CContextMenuHandler::LoadMenuBitmap() const
{
    if (m_config.icon.empty()) return nullptr;

    std::wstring iconPath;
    if (m_config.icon.size() >= 2 &&
        (m_config.icon[1] == L':' || (m_config.icon[0] == L'\\' && m_config.icon[1] == L'\\')))
        iconPath = m_config.icon;
    else
        iconPath = m_configDir + L"\\" + m_config.icon;

    // Prefer large icon (32x32) as source so DrawIconEx scales down with better quality.
    HICON hIcon = nullptr;
    ExtractIconExW(iconPath.c_str(), m_config.iconIndex, &hIcon, nullptr, 1);
    if (!hIcon)
        ExtractIconExW(iconPath.c_str(), m_config.iconIndex, nullptr, &hIcon, 1);
    if (!hIcon) return nullptr;

    // Render the icon into a 32bpp DIB for use as a menu bitmap.
    // ZeroMemory ensures the alpha channel starts at 0 (transparent),
    // DrawIconEx fills in both colour and alpha correctly for ARGB menus.
    const int sz = (m_config.iconSize > 0) ? m_config.iconSize
                                            : GetSystemMetrics(SM_CXSMICON);
    BITMAPINFOHEADER bih = {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = sz;
    bih.biHeight      = -sz;   // top-down
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    void*   pvBits = nullptr;
    HBITMAP hBmp   = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bih),
                                      DIB_RGB_COLORS, &pvBits, nullptr, 0);
    if (hBmp && pvBits) {
        ZeroMemory(pvBits, sz * sz * 4);
        HDC     hdc  = CreateCompatibleDC(nullptr);
        HGDIOBJ hOld = SelectObject(hdc, hBmp);
        DrawIconEx(hdc, 0, 0, hIcon, sz, sz, 0, nullptr, DI_NORMAL);
        SelectObject(hdc, hOld);
        DeleteDC(hdc);
    }

    DestroyIcon(hIcon);
    return hBmp;
}

std::wstring CContextMenuHandler::ComputeArchiveName() const
{
    namespace fs = std::filesystem;
    const std::wstring& ext = m_config.archiveExtension;

    if (m_selectedFiles.empty())   return L"archive" + ext;
    if (m_selectedFiles.size() == 1)
        return fs::path(m_selectedFiles[0]).stem().wstring() + ext;

    // Multiple files: name after the parent folder
    return fs::path(m_selectedFiles[0]).parent_path().filename().wstring() + ext;
}

// ---------------------------------------------------------------------------
// IContextMenu
// ---------------------------------------------------------------------------

STDMETHODIMP CContextMenuHandler::QueryContextMenu(
    HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    CMH_LOGF(L"QueryContextMenu: uFlags=0x%X files=%zu valid=%d",
             uFlags, m_selectedFiles.size(), (int)m_config.valid);
    if (uFlags & CMF_DEFAULTONLY)    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    if (!m_config.valid)             return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    if (m_config.entries.empty())    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    if (m_selectedFiles.empty())     return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

    std::wstring archiveName = ComputeArchiveName();

    // Detect if the entire selection consists of directories.
    // filesOnly entries are hidden in that case.
    bool allDirs = !m_selectedFiles.empty();
    for (const auto& p : m_selectedFiles) {
        DWORD attr = GetFileAttributesW(p.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            allDirs = false;
            break;
        }
    }
    if (m_selectedFiles.empty()) allDirs = true; // folder background / tree-pane

    HMENU hSubMenu = CreatePopupMenu();
    UINT  menuPos  = 0;

    for (size_t i = 0; i < m_config.entries.size(); ++i) {
        const auto& entry = m_config.entries[i];

        if (entry.filesOnly && allDirs) continue; // reserve ID slot but don't insert

        // extensions filter: show only when ALL selected files match one of the listed extensions
        if (!entry.extensions.empty()) {
            bool match = true;
            for (const auto& file : m_selectedFiles) {
                size_t dot = file.rfind(L'.');
                std::wstring ext = (dot != std::wstring::npos) ? file.substr(dot) : L"";
                for (auto& c : ext) c = towlower(c);
                bool found = false;
                for (const auto& allowed : entry.extensions) {
                    std::wstring a = allowed;
                    for (auto& c : a) c = towlower(c);
                    if (a == ext) { found = true; break; }
                }
                if (!found) { match = false; break; }
            }
            if (!match) continue;
        }

        std::wstring label;
        if (m_selectedFiles.size() == 1 && !entry.labelWithFile.empty())
            label = ReplacePlaceholders(entry.labelWithFile, archiveName, m_selectedFiles);
        else
            label = entry.label;

        InsertMenuW(hSubMenu, menuPos++, MF_BYPOSITION | MF_STRING,
                    idCmdFirst + (UINT)i, label.c_str());
    }

    if (!m_hMenuBitmap)
        m_hMenuBitmap = LoadMenuBitmap();

    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask         = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
    mii.hSubMenu      = hSubMenu;
    mii.dwTypeData    = const_cast<LPWSTR>(m_config.menuTitle.c_str());
    mii.wID           = idCmdFirst + (UINT)m_config.entries.size();
    if (m_hMenuBitmap) {
        mii.fMask    |= MIIM_BITMAP;
        mii.hbmpItem  = m_hMenuBitmap;
    }
    InsertMenuItemW(hMenu, indexMenu, TRUE, &mii);

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)(m_config.entries.size() + 1));
}

STDMETHODIMP CContextMenuHandler::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    if (!m_config.valid) return E_FAIL;
    if (HIWORD(pici->lpVerb) != 0) return E_INVALIDARG; // string verbs not supported

    UINT idx = LOWORD(pici->lpVerb);
    if (idx >= (UINT)m_config.entries.size()) return E_INVALIDARG;
    CMH_LOGF(L"InvokeCommand: idx=%u id=%s", idx, m_config.entries[idx].id.c_str());

    std::wstring archiveName = ComputeArchiveName();

    // Executable: prefer PSF launcher; fall back to direct executable.
    const std::wstring& exeName = m_config.psfLauncher.empty()
                                  ? m_config.executable
                                  : m_config.psfLauncher;

    // Absolute path wins; otherwise relative to config/package directory.
    std::wstring exe;
    if (exeName.size() >= 2 && (exeName[1] == L':' ||
        (exeName[0] == L'\\' && exeName[1] == L'\\')))
        exe = exeName;
    else
        exe = m_configDir + L"\\" + exeName;

    std::wstring args = ReplacePlaceholders(
        m_config.entries[idx].args, archiveName, m_selectedFiles);

    if (m_config.entries[idx].useShellExecute) {
        CMH_LOGF(L"InvokeCommand: ShellExecute exe=%s args=%s", exe.c_str(), args.c_str());
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb       = L"open";
        sei.lpFile       = exe.c_str();
        sei.lpParameters = args.empty() ? nullptr : args.c_str();
        sei.nShow        = SW_NORMAL;
        if (!ShellExecuteExW(&sei)) {
            DWORD err = GetLastError();
            CMH_LOGF(L"InvokeCommand: ShellExecute failed, err=%u", err);
            return HRESULT_FROM_WIN32(err);
        }
        if (sei.hProcess) CloseHandle(sei.hProcess);
        return S_OK;
    }

    std::wstring cmdLine = L"\"" + exe + L"\" " + args;
    CMH_LOGF(L"InvokeCommand: CreateProcess cmdLine=%s", cmdLine.c_str());
    STARTUPINFOW si      = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        DWORD err = GetLastError();
        CMH_LOGF(L"InvokeCommand: CreateProcess failed, err=%u", err);
        return HRESULT_FROM_WIN32(err);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return S_OK;
}

STDMETHODIMP CContextMenuHandler::GetCommandString(
    UINT_PTR idCmd, UINT uType, UINT* /*pReserved*/, CHAR* pszName, UINT cchMax)
{
    if (idCmd >= m_config.entries.size()) return E_INVALIDARG;

    if (uType == GCS_VERBW) {
        return StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax,
                              m_config.entries[idCmd].id.c_str());
    }
    if (uType == GCS_VERBA) {
        WideCharToMultiByte(CP_ACP, 0, m_config.entries[idCmd].id.c_str(), -1,
                            pszName, (int)cchMax, nullptr, nullptr);
        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP CContextMenuHandler::HandleMenuMsg(UINT, WPARAM, LPARAM)
{
    return S_OK;
}

STDMETHODIMP CContextMenuHandler::HandleMenuMsg2(UINT, WPARAM, LPARAM, LRESULT*)
{
    return S_OK;
}
