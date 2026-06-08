#pragma once
#include "pch.h"
#include <mciapi.h>
#include <digitalv.h>
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#pragma comment(lib, "Winmm.lib")

//*****************************************************
// mciSendCommandA (winmm.dll)
//   攔截遊戲的 CD-audio 播放, 導向本地 music\%02d.mp3。
//   MCI_OPEN cdaudio 回傳 sentinel device id, 其餘訊息 proxy 給真正的 MP3 device。
//*****************************************************

extern "C" {
    MCIERROR WINAPI MyMciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);
}

static HookManager MciSendCommandA_HookManager {
    getLibraryProcAddress(L"winmm.dll", "mciSendCommandA"),
    MyMciSendCommandA
};

#define MAGIC_DEVICE_ID 0xBEEF

MCIERROR WINAPI MyMciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    static MCIDEVICEID s_device_id;

    switch (uMsg) {
    case MCI_OPEN:
    {
        LPMCI_OPEN_PARMSA parms = (LPMCI_OPEN_PARMSA)dwParam;
        bool cond1 = fdwCommand == MCI_OPEN_TYPE && strcmp(parms->lpstrDeviceType, "cdaudio") == 0;
        bool cond2 = (fdwCommand & MCI_OPEN_TYPE_ID) && LOWORD(parms->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO;
        if (cond1 || cond2) {
            parms->wDeviceID = MAGIC_DEVICE_ID;
            return 0;
        }
        break;
    }

    case MCI_SET:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }

    case MCI_STATUS:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }

    case MCI_PLAY:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            LPMCI_PLAY_PARMS play_param = (LPMCI_PLAY_PARMS)dwParam;
            int track_number = play_param->dwFrom & 0xFF;
            char path[MAX_PATH];
            sprintf_s(path, "music\\%02d.mp3", track_number);

            MCI_OPEN_PARMSA open_param = { 0 };
            open_param.lpstrElementName = path;

            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(NULL, MCI_OPEN, MCI_OPEN_ELEMENT, (DWORD_PTR)&open_param);

            s_device_id = open_param.wDeviceID;
            play_param->dwFrom = 0;

            mciSendCommandA(s_device_id, MCI_PLAY, MCI_NOTIFY | MCI_DGV_PLAY_REPEAT, (DWORD_PTR)play_param);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }
    case MCI_STOP:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            mciSendCommandA(s_device_id, MCI_CLOSE, 0, NULL);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
    }

    case MCI_CLOSE:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            return 0;
        }
    }
    default:
        break;
    }

    MciSendCommandA_HookManager.unhook();
    auto ret = mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    MciSendCommandA_HookManager.hook();
    return ret;
}

inline void Install_MciSendCommandA_Hook()
{
    MciSendCommandA_HookManager.hook();
}
