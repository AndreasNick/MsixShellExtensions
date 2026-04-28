#pragma once
#include <windows.h>
#include <string>
#include <sstream>

extern bool g_debugLog;

inline void MCMHLogW(const std::wstring& msg)
{
    if (!g_debugLog) return;
    std::wstring full = L"[MsixMCMH] " + msg + L"\n";
    OutputDebugStringW(full.c_str());
}

inline void MCMHLog(const char* msg)
{
    if (!g_debugLog) return;
    std::string full = std::string("[MsixMCMH] ") + msg + "\n";
    OutputDebugStringA(full.c_str());
}

#define MCMH_LOG(msg)  MCMHLog(msg)
#define MCMH_LOGW(msg) MCMHLogW(msg)
#define MCMH_LOGF(fmt, ...) \
    do { if (g_debugLog) { \
        char _buf[512]; \
        sprintf_s(_buf, fmt, __VA_ARGS__); \
        MCMHLog(_buf); \
    } } while(0)
