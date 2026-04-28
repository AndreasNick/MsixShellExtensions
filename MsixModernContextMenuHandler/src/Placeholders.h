#pragma once
#include <string>
#include <vector>

std::wstring ReplacePlaceholders(
    const std::wstring& tmpl,
    const std::wstring& archiveName,
    const std::vector<std::wstring>& files);
