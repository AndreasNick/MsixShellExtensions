// {4A7B3C1D-E2F5-4689-ABCD-EF1234567891}
// Single CLSID for all ItemTypes (*, Directory). VS Code and Notepad++ use the
// same pattern — one CLSID, multiple desktop5:ItemType entries. Two CLSIDs
// crashed Explorer (KERNELBASE.dll 0xc0000005) before the DLL was even loaded.
// The JSON config may override this via the "clsid" field.
#pragma once
#include <initguid.h>
DEFINE_GUID(CLSID_ModernContextMenuHandler,
    0x4a7b3c1d, 0xe2f5, 0x4689, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x91);
