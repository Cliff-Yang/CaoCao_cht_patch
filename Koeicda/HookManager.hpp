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
    // �ѼƬO�ؼШ�Ʀa�}targetFuncAddress�A�ڭ̦ۤv��hook��ƪ��a�}HookFuncAddress
    explicit HookManager(PVOID targetFuncAddress, PVOID hookFuncAddress)
        :targetFuncAddr(targetFuncAddress), hookFuncAddr(hookFuncAddress)
    {
        // �p��۹ﰾ���ͦ�shellcode
        Address offset((DWORD)hookFuncAddress - ((DWORD)targetFuncAddress + SHELLCODE_SIZE));
        BYTE tempShellCode[SHELLCODE_SIZE] = {
            0xE9, offset[0], offset[1], offset[2], offset[3],
        };
        memcpy(shellCode, tempShellCode, SHELLCODE_SIZE);

        //�O�s�즳���r�`�A�ݭn����ؼШ�ƪ��������s�]�m���iŪ�g
        VirtualProtect(targetFuncAddr, SHELLCODE_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(originalBytes, targetFuncAddr, SHELLCODE_SIZE);
    }
    void hook()
    {
        //�Nshellcode�g�J�ؼШ�ƨ�hook
        memcpy(targetFuncAddr, shellCode, SHELLCODE_SIZE);
    }
    void unhook()
    {
        //��_������r�`��unhook
        memcpy(targetFuncAddr, originalBytes, SHELLCODE_SIZE);
    }
    ~HookManager()
    {
        //�R�c�ɱN�ؼШ�ƪ��������s���O�@�ݩʫ�_
        VirtualProtect(targetFuncAddr, SHELLCODE_SIZE, oldProtect, &oldProtect);
    }
};

