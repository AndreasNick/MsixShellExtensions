#pragma once
#include <string>
#include <vector>

// Replaces {archive}, {files}, {folder} in a template string.
// archiveName: computed from selected file(s) + config.archiveExtension
// files:       full paths of the selected items
std::wstring ReplacePlaceholders(
    const std::wstring&              tmpl,
    const std::wstring&              archiveName,
    const std::vector<std::wstring>& files);
