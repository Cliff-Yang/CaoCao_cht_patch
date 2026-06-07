# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案簡介

這是一個針對「曹操傳MOD」(三國志曹操傳同人遊戲) 的簡轉繁 (Simplified→Traditional Chinese) 補丁。它以單一 DLL (`Koeicda.dll`) 的形式 **proxy 注入** 進遊戲：玩家先把遊戲原本的 `Koeicda.dll` 改名為 `Koeicda_Origin.dll`，再把本專案 build 出的 `Koeicda.dll` 與 `dictionary.txt` 放進遊戲目錄。完整安裝步驟請見 `readme.md` (遊戲需透過 Locale Emulator / AppLocale 以簡體中文 locale 啟動)。

## Build

Visual Studio 2022 solution (`CaoCao_cht_patch.sln`)，單一專案 `Koeicda` (輸出型態為 DynamicLibrary)。沒有測試套件，也沒有 lint 設定。

```powershell
# 可從 VS 內 build，或從命令列：
msbuild CaoCao_cht_patch.sln /p:Configuration=Release /p:Platform=Win32
```

**請務必 build `Win32` (x86) 平台。** 雖然 solution 定義了 x64 configs，但 hooking 程式碼僅支援 32-bit — `Address.hpp`/`HookManager.hpp` 在計算相對 `E9` jmp offset 時，假設指標為 4 bytes (`DWORD`)。目標遊戲是 32-bit process。x64 雖然能編譯，但會產生無法運作的 DLL。

Build 完成後，`dictionary.txt` 會被複製到 DLL 旁邊 (vcxproj 中的 `CopyFileToFolders` item)。兩個檔案必須一起部署 — `dictionary.txt` 在 runtime 時是依工作目錄相對路徑讀取的。

## 運作原理 (architecture)

本 DLL 透過劫持遊戲原本音訊 DLL 的 exports，並 hook 兩個系統 API，達成兩件事。所有初始化都在 `dllmain.cpp` 的 `DLL_PROCESS_ATTACH` 中接好。

**1. Export forwarding (DLL proxy)。** `dllmain.cpp` 用 `#pragma comment(linker, "/EXPORT:...=Koeicda_Origin.<fn>,@n")` 把全部約 20 個 `CDAudio*` exports 轉發給改名後的原始 DLL。這讓我們的 DLL 能冒充 `Koeicda.dll`，同時讓原始 DLL 繼續處理真正的 CD-audio 工作。這也是安裝步驟需要把原始檔改名為 `Koeicda_Origin.dll` 的原因。

**2. Inline API hooking**，透過 `HookManager` (`HookManager.hpp`)。每個 hook 會把目標系統函式的前 5 bytes 覆寫成一個指向我們替代函式的相對 `jmp` (`0xE9 + 4-byte offset`)。各處都採用相同模式：`unhook()` → 呼叫真正的函式 → 再次 `hook()`，讓 reentrant/真實呼叫得以通過。`Address.hpp` 只是把指標重新解讀成 4 bytes 以取得 offset。被 hook 的有兩個函式：

   - **`ScriptStringAnalyse` (usp10.dll)** — 遊戲用來繪製字串的 Uniscribe text-shaping 呼叫。`MyScriptStringAnalyse` 攔截輸入字串，先做中文轉換再往下傳。這是翻譯補丁的核心。已標記為 Unicode／空字串的情況 (`cString <= 0 && iCharset != -1`) 會原封不動通過。

   - **`mciSendCommandA` (winmm.dll)** — 把遊戲的 CD-audio 播放導向本地 MP3 檔。此 hook 攔截 `cdaudio` 的 `MCI_OPEN`，回傳一個 sentinel device id (`MAGIC_DEVICE_ID = 0xBEEF`)，並在 `MCI_PLAY` 時把要求的 CD track 編號對應到 `music\%02d.mp3`，以 MCI element 的方式開啟／循環播放。其他針對該 sentinel device 的 MCI 訊息則 proxy 給真正的 MP3 device。(於 commit「處理三國志孔明傳音樂 hook」加入。)

**3. 中文轉換** (`ChsToCht.hpp`)，分兩階段：
   - `UTF16LE_CHS_To_CHT` 透過 Win32 `LCMapStringW(LCMAP_TRADITIONAL_CHINESE)` 做主要的簡轉繁對應。
   - `UTF16LE_FixOneToMany` 接著修正「一簡對多繁」而 `LCMapStringW` 選錯字的字元。正確的覆寫對應來自 `dictionary.txt`。

## 編輯翻譯字典

`Koeicda/dictionary.txt` 是翻譯修正時最常會動到的檔案。格式為每行一組對應，`簡 : 繁` (簡體字、空格、冒號、空格、繁體字)，UTF-8 編碼。每個 token 只會取用 **第一個字元** (`ChsToCht.hpp` 讀 `[0]`)。loader (`Read_Dictionary_File`) 會剝除第一行開頭的 UTF-8 BOM。當 `LCMapStringW` 對某個單字轉錯時，在這裡加一行即可覆寫該單字的轉換。

## 慣例／注意事項

- 原始碼註解使用繁體中文；部分 `.hpp` 註解是以非 UTF-8 (Big5/GBK) 編碼存檔，用 UTF-8 讀取時會顯示為亂碼 (mojibake) — 這純屬顯示問題，除非刻意重新存檔，否則請勿更動。
- `pch.h` precompiled header 為必要；`pch.cpp` 是建立 PCH 的 translation unit。
- 這些 hook 都是 global static 的 `HookManager` instances；其 constructor 會在載入時擷取原始 bytes。請勿更動相對於 `DllMain` 假設的建構順序。
