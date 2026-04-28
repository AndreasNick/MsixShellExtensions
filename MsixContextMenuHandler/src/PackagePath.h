#pragma once
#include <string>
#include <windows.h>

// Returns the directory that contains MsixContextMenuHandler.json.
// Inside MSIX:  uses GetCurrentPackagePath() via runtime lookup.
// Outside MSIX: falls back to the directory containing this DLL.
std::wstring GetConfigDirectory(HMODULE hModule);
