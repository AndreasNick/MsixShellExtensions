#include "Placeholders.h"
#include <filesystem>

static std::wstring ReplaceAll(std::wstring s,
                               const std::wstring& from,
                               const std::wstring& to)
{
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

static std::wstring QuotedFiles(const std::vector<std::wstring>& files)
{
    std::wstring result;
    for (const auto& f : files) {
        if (!result.empty()) result += L" ";
        result += L"\"" + f + L"\"";
    }
    return result;
}

std::wstring ReplacePlaceholders(
    const std::wstring&              tmpl,
    const std::wstring&              archiveName,
    const std::vector<std::wstring>& files)
{
    namespace fs = std::filesystem;

    std::wstring folder;
    if (!files.empty())
        folder = fs::path(files[0]).parent_path().wstring();

    std::wstring result = tmpl;
    result = ReplaceAll(result, L"{archive}", archiveName);
    result = ReplaceAll(result, L"{files}",   QuotedFiles(files));
    result = ReplaceAll(result, L"{folder}",  folder);
    return result;
}
