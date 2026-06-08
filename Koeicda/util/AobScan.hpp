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
