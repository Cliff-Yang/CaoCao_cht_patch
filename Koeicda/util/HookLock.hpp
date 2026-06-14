#pragma once
#include "pch.h"

//*****************************************************
// 全域 hook 串行化鎖 (thread-safety)
//
//   背景: 本 DLL 同時 proxy 遊戲原本的 CD-audio DLL。換 BGM 時 MCI 會在它自己的
//   worker thread 上播放 music\NN.mp3, 該執行緒會發出 ReadFile/CreateFileA —— 於是
//   「被 hook 的 ReadFile/CreateFileA」會被主執行緒 (載入 .eex 劇本) 與音訊 worker
//   thread 同時呼叫。兩者原本無鎖地:
//     (1) 改寫同一段 5-byte E9 jmp patch (unhook/hook), 以及
//     (2) 存取共享的 g_EexHandles (std::unordered_set; insert 觸發 rehash 重配
//         bucket 陣列時, 被另一執行緒 find 走訪同一塊記憶體) -> heap free-list 損毀
//         -> 稍後在 ntdll heap allocator 內 AV (即進 S_38 換 BGM 時穩定閃退的成因)。
//
//   解法: 用一個全域 CRITICAL_SECTION 把這些 hook 函數本體串行化。CRITICAL_SECTION
//   本身可重入 (同執行緒可重複 Enter), 故轉換期間若 re-entrant 回到某個 hook (例如
//   Win32 轉換 API 內部讀檔再次進入 MyReadFile) 不會自我死鎖。
//
//   必須在任何 hook 安裝前呼叫 InitHookLock() (見 dllmain.cpp DLL_PROCESS_ATTACH 最前)。
//*****************************************************

extern CRITICAL_SECTION g_HookLock;

inline void InitHookLock()
{
    InitializeCriticalSection(&g_HookLock);
}

// RAII: 建構時 Enter, 解構時 Leave。宣告在 hook 函數本體最前面即可串行化整段。
struct HookGuard
{
    HookGuard()  { EnterCriticalSection(&g_HookLock); }
    ~HookGuard() { LeaveCriticalSection(&g_HookLock); }
};
