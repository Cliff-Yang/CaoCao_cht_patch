#pragma once
#include "pch.h"
#include <unordered_set>
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/EexScript.hpp"
#ifdef _DEBUG
#include "../util/DebugLog.hpp"
#endif

//*****************************************************
// CreateFileA / ReadFile (kernel32.dll)
//   在「讀檔當下」把劇本檔 (*.eex) 內嵌的 GBK 簡體文字就地簡轉繁, 取代原本
//   hook 遊戲內部函數 (FullSentenceCall / GameTextPrintf / PrintfImpl) 的作法。
//
//   流程:
//     MyCreateFileA  - 檔名以 ".eex" 結尾者, 記下其回傳 handle。
//     MyReadFile     - 真讀取後, 若 handle 是 eex 且 buffer 開頭為 "EEX\0",
//                      呼叫 Eex::ConvertBufferInPlace 就地轉繁。
//
//   不 hook CloseHandle: 正確性由「handle 屬 eex」+「buffer 開頭 magic」雙重把關,
//   handle 值縱被回收再用也幾乎不可能同時撞上 EEX magic; 省去對高頻 CloseHandle
//   做 unhook/rehook 的成本與 race surface。stale handle 僅佔極少記憶體。
//
//   執行緒模型沿用本專案既有 hook 慣例 (unhook → 真呼叫 → hook, 不加鎖); 此老引擎
//   資源載入為單執行緒。
//*****************************************************

extern "C" HANDLE WINAPI MyCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
extern "C" BOOL WINAPI MyReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

static HookManager CreateFileA_HookManager {
    getLibraryProcAddress(L"kernel32.dll", "CreateFileA"),
    MyCreateFileA
};
static HookManager ReadFile_HookManager {
    getLibraryProcAddress(L"kernel32.dll", "ReadFile"),
    MyReadFile
};

// 已開啟的 *.eex handle 集合
static std::unordered_set<HANDLE> g_EexHandles;

// 路徑是否以 ".eex" 結尾 (不分大小寫)
static bool IsEexPath(LPCSTR path)
{
    if (path == nullptr) return false;
    size_t n = strlen(path);
    if (n < 4) return false;
    const char* ext = path + (n - 4);
    return ext[0] == '.'
        && (ext[1] == 'e' || ext[1] == 'E')
        && (ext[2] == 'e' || ext[2] == 'E')
        && (ext[3] == 'x' || ext[3] == 'X');
}

extern "C" HANDLE WINAPI MyCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    bool isEex = IsEexPath(lpFileName);

    CreateFileA_HookManager.unhook();
    HANDLE h = CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    CreateFileA_HookManager.hook();

    if (isEex && h != INVALID_HANDLE_VALUE)
    {
        g_EexHandles.insert(h);
#ifdef _DEBUG
        DebugLog("[ReadFileHook] track eex handle=%p %s\n", h, lpFileName);
#endif
    }
    return h;
}

extern "C" BOOL WINAPI MyReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    ReadFile_HookManager.unhook();
    BOOL ret = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    ReadFile_HookManager.hook();

    if (ret && lpBuffer != nullptr && lpNumberOfBytesRead != nullptr
        && g_EexHandles.find(hFile) != g_EexHandles.end())
    {
        DWORD got = *lpNumberOfBytesRead;
        const BYTE* p = (const BYTE*)lpBuffer;
        if (got >= 4 && p[0] == 'E' && p[1] == 'E' && p[2] == 'X' && p[3] == 0x00)
        {
            Eex::ConvertBufferInPlace((BYTE*)lpBuffer, got);
        }
    }
    return ret;
}

inline void Install_ReadFile_Hook()
{
    CreateFileA_HookManager.hook();
    ReadFile_HookManager.hook();
}
