#pragma once
#include "pch.h"
#include "Address.hpp"

class HookManager
{
    enum { SHELLCODE_SIZE = 5 };
private:
    LPVOID targetFuncAddr;
    LPVOID hookFuncAddr;
    BYTE originalBytes[SHELLCODE_SIZE];
    BYTE shellCode[SHELLCODE_SIZE];
    DWORD oldProtect = 0;
public:
    // 參數是目標函數地址targetFuncAddress，我們自己的hook函數的地址HookFuncAddress
    explicit HookManager(PVOID targetFuncAddress, PVOID hookFuncAddress)
        :targetFuncAddr(targetFuncAddress), hookFuncAddr(hookFuncAddress)
    {
        // 計算相對偏移生成shellcode
        Address offset((DWORD)hookFuncAddress - ((DWORD)targetFuncAddress + SHELLCODE_SIZE));
        BYTE tempShellCode[SHELLCODE_SIZE] = {
            0xE9, offset[0], offset[1], offset[2], offset[3],
        };
        memcpy(shellCode, tempShellCode, SHELLCODE_SIZE);

        //保存原有的字節，需要先把目標函數的虛擬內存設置為可讀寫
        VirtualProtect(targetFuncAddr, SHELLCODE_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(originalBytes, targetFuncAddr, SHELLCODE_SIZE);
    }
    void hook()
    {
        //將shellcode寫入目標函數來hook
        memcpy(targetFuncAddr, shellCode, SHELLCODE_SIZE);
    }
    void unhook()
    {
        //恢復原先的字節來unhook
        memcpy(targetFuncAddr, originalBytes, SHELLCODE_SIZE);
    }
    ~HookManager()
    {
        //析構時將目標函數的虛擬內存的保護屬性恢復
        VirtualProtect(targetFuncAddr, SHELLCODE_SIZE, oldProtect, &oldProtect);
    }
};

