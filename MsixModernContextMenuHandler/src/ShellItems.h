#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>

// Extracts file system paths from an IShellItemArray.
// Items that have no SFGAO_FILESYSTEM attribute are silently skipped.
std::vector<std::wstring> GetFilesFromShellItemArray(IShellItemArray* psia);

// Determines the suggested archive name for the current selection.
// 1 file  → stem + archiveExt  (e.g. "report.rar")
// N files → parent folder name + archiveExt  (e.g. "Documents.rar")
std::wstring ComputeArchiveName(const std::vector<std::wstring>& files,
                                const std::wstring& archiveExt);
