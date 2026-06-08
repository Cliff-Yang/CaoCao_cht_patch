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
//   [特徵]
//     55 8B EC 83 EC 18           : push ebp; mov ebp,esp; sub esp,18h
//     8B 45 10 89 45 F8           : eax=[ebp+10h](full_text); [ebp-8]=eax
//     C7 45 F4 00 00 00 00        : mov [ebp-0Ch], 0 (初始化計數器)
//*****************************************************

#define FULL_SENTENCE_CALL_RVA 0x0002C8B0
static const unsigned char kFullSentenceCallSig[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x8B, 0x45, 0x10, 0x89, 0x45, 0xF8, 0xC7, 0x45, 0xF4, 0x00, 0x00, 0x00, 0x00
};
static const char kFullSentenceCallMask[] = "xxxxxxxxxxxxxxxxxxx";

typedef int(__cdecl* FullSentenceCall_t)(void* arg1, void* arg2, const char* full_text, int arg4);

extern "C" {
    int __cdecl MyFullSentenceCall(void* arg1, void* arg2, const char* full_text, int arg4);
}

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

int __cdecl MyFullSentenceCall(void* arg1, void* arg2, const char* full_text, int arg4)
{
    static char converted[8192];
    const char* target_text = full_text;

    if (full_text && strlen(full_text) > 0)
    {
#ifdef _DEBUG
        ConvertStats stats = { 0, 0 };
        bool matched = GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted), &stats);
        if (matched) target_text = converted;
        DebugLogConvert(full_text, matched ? converted : full_text, matched, stats);
#else
        if (GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted)))
        {
            target_text = converted;
        }
#endif
    }

    g_FullSentenceCallHook->unhook();
    int ret = ((FullSentenceCall_t)g_FullSentenceCallAddr)(arg1, arg2, target_text, arg4);
    g_FullSentenceCallHook->hook();

    return ret;
}

inline void Install_FullSentenceCall_Hook()
{
    g_FullSentenceCallAddr = FindAddress(kFullSentenceCallSig, kFullSentenceCallMask, FULL_SENTENCE_CALL_RVA);
    if (g_FullSentenceCallAddr) {
        g_FullSentenceCallHook = new HookManager(g_FullSentenceCallAddr, MyFullSentenceCall);
        g_FullSentenceCallHook->hook();
    }

#ifdef _DEBUG
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (g_FullSentenceCallAddr == nullptr)
        DebugLog("[FullSentenceCall] Feature Code scan failed, not hook\n");
    else
        DebugLog("[FullSentenceCall] addr=%p rva=0x%X %s\n",
            g_FullSentenceCallAddr, (unsigned)((BYTE*)g_FullSentenceCallAddr - base),
            ((BYTE*)g_FullSentenceCallAddr == base + FULL_SENTENCE_CALL_RVA) ? "(by fixed RVA)" : "(found by scan)");
#endif
}
