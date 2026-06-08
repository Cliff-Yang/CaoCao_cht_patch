#pragma once
#include "pch.h"

//*****************************************************
// 特徵碼掃描功能 (AOB Scan)
//*****************************************************

static bool SigMatch(const BYTE* p, const unsigned char* sig, const char* mask)
{
    for (; *mask; ++mask, ++p, ++sig)
        if (*mask == 'x' && *p != *sig) return false;
    return true;
}

static PVOID FindAddress(const unsigned char* sig, const char* mask, DWORD hardcodedRva)
{
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (base == nullptr) return nullptr;

    auto dos = (PIMAGE_DOS_HEADER)base;
    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    size_t sigLen = strlen(mask);

    // 1) 先驗證寫死的 RVA
    if (hardcodedRva + sigLen <= imageSize)
    {
        BYTE* guess = base + hardcodedRva;
        if (SigMatch(guess, sig, mask))
            return guess;
    }

    // 2) 掃描所有可執行區段
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s)
    {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* start = base + sec[s].VirtualAddress;
        DWORD size = sec[s].Misc.VirtualSize;
        if (size < sigLen) continue;
        for (BYTE* p = start; p <= start + size - sigLen; ++p)
            if (SigMatch(p, sig, mask))
                return p;
    }
    return nullptr;
}

//*****************************************************
// 多特徵碼版: 同一函數在不同遊戲 (同引擎、不同編譯 codegen) 會有不同
// prologue, 例如 FullSentenceCall 在天龍八部與云荒逍遥传位於相同 RVA
// 但前導 bytes 不同。依序試多組 sig, 任一命中即回傳。
//*****************************************************
struct SigPattern { const unsigned char* sig; const char* mask; };

// 回傳命中位址; 若 outIndex 非 null, 寫入命中的是第幾組 sig (用於依變體切換呼叫慣例)。
static PVOID FindAddressMulti(const SigPattern* pats, size_t count, DWORD hardcodedRva, int* outIndex = nullptr)
{
    if (outIndex) *outIndex = -1;
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (base == nullptr) return nullptr;

    auto dos = (PIMAGE_DOS_HEADER)base;
    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;

    // 1) 先驗證寫死的 RVA (逐一比對每組 sig)
    BYTE* guess = base + hardcodedRva;
    for (size_t k = 0; k < count; ++k)
    {
        size_t sigLen = strlen(pats[k].mask);
        if (hardcodedRva + sigLen <= imageSize && SigMatch(guess, pats[k].sig, pats[k].mask))
        {
            if (outIndex) *outIndex = (int)k;
            return guess;
        }
    }

    // 2) 掃描所有可執行區段; 每個位置逐一比對每組 sig, 各自做邊界檢查
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s)
    {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* start = base + sec[s].VirtualAddress;
        BYTE* end = start + sec[s].Misc.VirtualSize;
        for (BYTE* p = start; p < end; ++p)
            for (size_t k = 0; k < count; ++k)
            {
                size_t sigLen = strlen(pats[k].mask);
                if (p + sigLen <= end && SigMatch(p, pats[k].sig, pats[k].mask))
                {
                    if (outIndex) *outIndex = (int)k;
                    return p;
                }
            }
    }
    return nullptr;
}
