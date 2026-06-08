#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/AobScan.hpp"
#include "../util/ChsToCht.hpp"
#ifdef _DEBUG
#include "../util/DebugLog.hpp"
#endif

//*****************************************************
// FullSentenceCall (DialogRender, 遊戲內部函數, 特徵碼掃描定位)
//   對話框渲染進入點, 此處傳入的是尚未拆解的完整簡體 GBK 句子。
//   是唯一的整句簡轉繁執行點。
//   同一支函數在多款同引擎遊戲都位於 RVA 0x0002C8B0, 但因編譯 codegen
//   不同, prologue bytes 不同, 故以多組特徵碼依序比對 (FindAddressMulti)。
//
//   [變體 A] 天龍八部 / 曹操傳 (直接 mov 立即值清計數器)
//     55 8B EC 83 EC 18           : push ebp; mov ebp,esp; sub esp,18h
//     8B 45 10 89 45 F8           : eax=[ebp+10h](full_text); [ebp-8]=eax
//     C7 45 F4 00 00 00 00        : mov [ebp-0Ch], 0 (初始化計數器)
//
//   [變體 B] 云荒逍遥传 (xor eax,eax 清計數器, 之後才載入 full_text)
//     55 8B EC 83 EC 18           : push ebp; mov ebp,esp; sub esp,18h
//     33 C0                       : xor eax,eax
//     89 45 F4 89 45 F0 88 45 FE  : [ebp-0Ch]=[ebp-10h]=0; [ebp-2]=al
//     8B 45 10 89 45 F8           : eax=[ebp+10h](full_text); [ebp-8]=eax
//*****************************************************

#define FULL_SENTENCE_CALL_RVA 0x0002C8B0

// 變體 A: 天龍八部 / 曹操傳
static const unsigned char kFullSentenceCallSig_A[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x8B, 0x45, 0x10, 0x89, 0x45, 0xF8, 0xC7, 0x45, 0xF4, 0x00, 0x00, 0x00, 0x00
};
static const char kFullSentenceCallMask_A[] = "xxxxxxxxxxxxxxxxxxx";

// 變體 B: 云荒逍遥传
static const unsigned char kFullSentenceCallSig_B[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x33, 0xC0, 0x89, 0x45, 0xF4, 0x89, 0x45, 0xF0, 0x88, 0x45, 0xFE, 0x8B, 0x45, 0x10, 0x89, 0x45, 0xF8
};
static const char kFullSentenceCallMask_B[] = "xxxxxxxxxxxxxxxxxxxxxxx";

static const SigPattern kFullSentenceCallSigs[] = {
    { kFullSentenceCallSig_A, kFullSentenceCallMask_A },
    { kFullSentenceCallSig_B, kFullSentenceCallMask_B },
};

// 重要: 兩款雖是同一支邏輯函數, 卻被編成不同呼叫慣例 ——
//   變體 A (天龍/曹操) 結尾 8B E5 5D C3      => __cdecl  (caller 清參數)
//   變體 B (云荒)      結尾 8B E5 5D C2 10 00 => __stdcall (callee ret 16 清 4 參數)
// 因此 hook 函數與呼叫原函數的 typedef 都必須跟著變; 否則 stack 被雙重清理,
// 觸發 RTC #0 "ESP was not properly saved across a function call" 並使遊戲堆疊損毀。
typedef int(__cdecl*   FullSentenceCall_cdecl_t)  (void* arg1, void* arg2, const char* full_text, int arg4);
typedef int(__stdcall* FullSentenceCall_stdcall_t)(void* arg1, void* arg2, const char* full_text, int arg4);

static PVOID g_FullSentenceCallAddr = nullptr;
static HookManager* g_FullSentenceCallHook = nullptr;

#ifdef _DEBUG
// DEBUG 組態才編入: 把轉換前/後文字與命中狀況寫到遊戲目錄下的 log_gbk.txt
//   (字串為 GBK, 請以 GBK/ANSI 編碼開啟檢視)
static void DebugLogConvert(const char* before, const char* after, bool matched, const ConvertStats& stats)
{
    DebugLog("[FullSentenceCall] [before] %s\n", before != nullptr ? before : "");
    DebugLog("[FullSentenceCall] [after ] %s\n", after  != nullptr ? after  : "");
    DebugLog("[FullSentenceCall] [match ] %s (phrase=%d, dict=%d)\n",
        matched ? "YES" : "no", stats.phraseHits, stats.dictHits);
}
#endif

// 共用轉換: 回傳要實際傳給原函數的字串 (轉換成功回 converted, 否則原文)。
static const char* FullSentenceCall_Resolve(const char* full_text)
{
    static char converted[8192];
    if (full_text && strlen(full_text) > 0)
    {
#ifdef _DEBUG
        ConvertStats stats = { 0, 0 };
        bool matched = GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted), &stats);
        DebugLogConvert(full_text, matched ? converted : full_text, matched, stats);
        if (matched) return converted;
#else
        if (GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted)))
            return converted;
#endif
    }
    return full_text;
}

// 變體 A: 天龍/曹操 (__cdecl)
int __cdecl MyFullSentenceCall_cdecl(void* arg1, void* arg2, const char* full_text, int arg4)
{
    const char* target_text = FullSentenceCall_Resolve(full_text);
    g_FullSentenceCallHook->unhook();
    int ret = ((FullSentenceCall_cdecl_t)g_FullSentenceCallAddr)(arg1, arg2, target_text, arg4);
    g_FullSentenceCallHook->hook();
    return ret;
}

// 變體 B: 云荒 (__stdcall)
int __stdcall MyFullSentenceCall_stdcall(void* arg1, void* arg2, const char* full_text, int arg4)
{
    const char* target_text = FullSentenceCall_Resolve(full_text);
    g_FullSentenceCallHook->unhook();
    int ret = ((FullSentenceCall_stdcall_t)g_FullSentenceCallAddr)(arg1, arg2, target_text, arg4);
    g_FullSentenceCallHook->hook();
    return ret;
}

inline void Install_FullSentenceCall_Hook()
{
    int variant = -1;
    g_FullSentenceCallAddr = FindAddressMulti(kFullSentenceCallSigs, _countof(kFullSentenceCallSigs), FULL_SENTENCE_CALL_RVA, &variant);
    if (g_FullSentenceCallAddr) {
        // 依命中的變體選擇對應呼叫慣例的 hook 函數 (0=cdecl, 1=stdcall)
        PVOID hookFn = (variant == 1) ? (PVOID)&MyFullSentenceCall_stdcall : (PVOID)&MyFullSentenceCall_cdecl;
        g_FullSentenceCallHook = new HookManager(g_FullSentenceCallAddr, hookFn);
        g_FullSentenceCallHook->hook();
    }

#ifdef _DEBUG
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (g_FullSentenceCallAddr == nullptr)
        DebugLog("[FullSentenceCall] Feature Code scan failed, not hook\n");
    else
        DebugLog("[FullSentenceCall] addr=%p rva=0x%X variant=%d(%s) %s\n",
            g_FullSentenceCallAddr, (unsigned)((BYTE*)g_FullSentenceCallAddr - base),
            variant, variant == 1 ? "stdcall" : "cdecl",
            ((BYTE*)g_FullSentenceCallAddr == base + FULL_SENTENCE_CALL_RVA) ? "(by fixed RVA)" : "(found by scan)");
#endif
}
