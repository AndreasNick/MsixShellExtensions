#pragma once
#include <windows.h>
#include <strsafe.h>

// g_debugLog is set to true when JSON contains "debug": true.
// In Debug builds all logging is unconditional; in Release it is gated by this flag.
// Capture output with Sysinternals DebugView (filter: MsixCMH).
extern bool g_debugLog;

inline void _CmhLog(const wchar_t* msg)
{
    OutputDebugStringW(L"[MsixCMH] ");
    OutputDebugStringW(msg);
    OutputDebugStringW(L"\n");
}

#ifdef _DEBUG
  #define CMH_LOG(msg) _CmhLog(msg)
  #define CMH_LOGF(fmt, ...) \
      do { \
          wchar_t _cmhbuf[512]; \
          StringCchPrintfW(_cmhbuf, 512, fmt, __VA_ARGS__); \
          _CmhLog(_cmhbuf); \
      } while (0)
#else
  #define CMH_LOG(msg) \
      do { if (g_debugLog) _CmhLog(msg); } while (0)
  #define CMH_LOGF(fmt, ...) \
      do { \
          if (g_debugLog) { \
              wchar_t _cmhbuf[512]; \
              StringCchPrintfW(_cmhbuf, 512, fmt, __VA_ARGS__); \
              _CmhLog(_cmhbuf); \
          } \
      } while (0)
#endif
