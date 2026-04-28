#include "Config.h"
#include <windows.h>

static std::wstring ReadFileToWString(const std::wstring& path)
{
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return {};

    DWORD size = GetFileSize(hFile, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) { CloseHandle(hFile); return {}; }

    std::string buf(size, '\0');
    DWORD read = 0;
    ReadFile(hFile, buf.data(), size, &read, nullptr);
    CloseHandle(hFile);

    // Strip UTF-8 BOM
    if (buf.size() >= 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
    {
        buf = buf.substr(3);
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, buf.c_str(), -1, wstr.data(), wlen);
    while (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

struct JsonParser
{
    const std::wstring& s;
    size_t              pos;

    explicit JsonParser(const std::wstring& str) : s(str), pos(0) {}

    void skipWs()
    {
        while (pos < s.size() &&
               (s[pos] == L' ' || s[pos] == L'\t' || s[pos] == L'\r' || s[pos] == L'\n'))
            ++pos;
    }

    bool peek(wchar_t c) { skipWs(); return pos < s.size() && s[pos] == c; }

    bool consume(wchar_t c)
    {
        skipWs();
        if (pos < s.size() && s[pos] == c) { ++pos; return true; }
        return false;
    }

    std::wstring parseString()
    {
        skipWs();
        if (pos >= s.size() || s[pos] != L'"') return {};
        ++pos;
        std::wstring result;
        while (pos < s.size() && s[pos] != L'"') {
            if (s[pos] == L'\\' && pos + 1 < s.size()) {
                ++pos;
                switch (s[pos]) {
                    case L'"':  result += L'"';  break;
                    case L'\\': result += L'\\'; break;
                    case L'/':  result += L'/';  break;
                    case L'n':  result += L'\n'; break;
                    case L'r':  result += L'\r'; break;
                    case L't':  result += L'\t'; break;
                    default:    result += s[pos]; break;
                }
            } else {
                result += s[pos];
            }
            ++pos;
        }
        if (pos < s.size()) ++pos;
        return result;
    }

    bool parseBool()
    {
        skipWs();
        if (pos + 4 <= s.size() && s.substr(pos, 4) == L"true")  { pos += 4; return true; }
        if (pos + 5 <= s.size() && s.substr(pos, 5) == L"false") { pos += 5; return false; }
        return false;
    }

    int parseInt()
    {
        skipWs();
        int sign = 1;
        if (pos < s.size() && s[pos] == L'-') { sign = -1; ++pos; }
        int result = 0;
        while (pos < s.size() && s[pos] >= L'0' && s[pos] <= L'9') {
            result = result * 10 + (int)(s[pos] - L'0');
            ++pos;
        }
        return sign * result;
    }

    void skipValue()
    {
        skipWs();
        if (pos >= s.size()) return;
        if (s[pos] == L'"') { parseString(); return; }
        if (s[pos] == L'{') { skipObject();  return; }
        if (s[pos] == L'[') { skipArray();   return; }
        while (pos < s.size() && s[pos] != L',' && s[pos] != L'}' && s[pos] != L']')
            ++pos;
    }

    void skipObject()
    {
        consume(L'{');
        while (!peek(L'}') && pos < s.size()) {
            parseString();
            consume(L':');
            skipValue();
            consume(L',');
        }
        consume(L'}');
    }

    void skipArray()
    {
        consume(L'[');
        while (!peek(L']') && pos < s.size()) {
            skipValue();
            consume(L',');
        }
        consume(L']');
    }

    std::vector<std::wstring> parseStringArray()
    {
        std::vector<std::wstring> result;
        if (!consume(L'[')) return result;
        while (!peek(L']') && pos < s.size()) {
            result.push_back(parseString());
            consume(L',');
        }
        consume(L']');
        return result;
    }

    ConfigEntry parseEntry()
    {
        ConfigEntry e;
        if (!consume(L'{')) return e;
        while (!peek(L'}') && pos < s.size()) {
            std::wstring key = parseString();
            consume(L':');
            if      (key == L"id")              e.id              = parseString();
            else if (key == L"label")           e.label           = parseString();
            else if (key == L"labelWithFile")   e.labelWithFile   = parseString();
            else if (key == L"args")            e.args            = parseString();
            else if (key == L"filesOnly")       e.filesOnly       = parseBool();
            else if (key == L"useShellExecute") e.useShellExecute = parseBool();
            else if (key == L"extensions")      e.extensions      = parseStringArray();
            else                                skipValue();
            consume(L',');
        }
        consume(L'}');
        return e;
    }

    std::vector<ConfigEntry> parseEntryArray()
    {
        std::vector<ConfigEntry> result;
        if (!consume(L'[')) return result;
        while (!peek(L']') && pos < s.size()) {
            result.push_back(parseEntry());
            consume(L',');
        }
        consume(L']');
        return result;
    }
};

Config ReadConfig(const std::wstring& path)
{
    Config cfg;
    std::wstring json = ReadFileToWString(path);
    if (json.empty()) return cfg;

    JsonParser p(json);
    if (!p.consume(L'{')) return cfg;

    while (!p.peek(L'}') && p.pos < json.size()) {
        std::wstring key = p.parseString();
        p.consume(L':');

        if      (key == L"clsid")            cfg.clsid            = p.parseString();
        else if (key == L"menuTitle")        cfg.menuTitle        = p.parseString();
        else if (key == L"executable")       cfg.executable       = p.parseString();
        else if (key == L"psfLauncher")      cfg.psfLauncher      = p.parseString();
        else if (key == L"icon")             cfg.icon             = p.parseString();
        else if (key == L"iconIndex")        cfg.iconIndex        = p.parseInt();
        else if (key == L"iconSize")         cfg.iconSize         = p.parseInt();
        else if (key == L"archiveExtension") cfg.archiveExtension = p.parseString();
        else if (key == L"entries")          cfg.entries          = p.parseEntryArray();
        else if (key == L"fileTypes")        cfg.fileTypes        = p.parseStringArray();
        else if (key == L"allFiles")         cfg.allFiles         = p.parseBool();
        else if (key == L"folders")          cfg.folders          = p.parseBool();
        else if (key == L"background")       cfg.background       = p.parseBool();
        else if (key == L"recycleBin")       cfg.recycleBin       = p.parseBool();
        else if (key == L"debug")            cfg.debug            = p.parseBool();
        else                                 p.skipValue();

        p.consume(L',');
    }

    cfg.valid = !cfg.executable.empty() && !cfg.entries.empty();
    return cfg;
}
