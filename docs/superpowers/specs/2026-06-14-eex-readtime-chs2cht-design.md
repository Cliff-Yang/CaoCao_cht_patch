# 讀檔時簡轉繁：以 EEX 解析取代遊戲內函數 hook

日期：2026-06-14
分支：develop

## 背景與目標

目前對話文字的簡轉繁靠 hook 三個遊戲內部函數完成：

- `FullSentenceCall` (RVA 0x2C8B0) — 整句進入點，做 phrases+dictionary 轉換
- `GameTextPrintf` / `PrintfImpl` — 逐字繪製

這套作法依賴特徵碼定位遊戲內函數、依呼叫慣例切換 hook，移植到同引擎其他遊戲時脆弱。

本案改為**在讀檔當下**就把劇本檔 (`*.eex`) 內嵌的 GBK 簡體文字轉成繁體，buffer 回到遊戲時已是繁體，不再 hook 任何遊戲內函數。

### 為何可行

`R_*.eex` / `S_*.eex` 是 `EEX\0` magic 開頭的 bytecode 劇本檔，**對白與 UI 文字以純 GBK 簡體字串內嵌在指令流中**（無加密、無壓縮）。實測載入第 38 關時 `R_38.eex` 是**一次整檔讀取**（`ReadFile want=5619 got=5619`）。

## 設計決策（已與使用者確認）

1. **轉換邏輯**：沿用現有 GBK 管線 `GBK_FullConvert`（phrases.txt + dictionary.txt + LCMapStringW），輸出仍為 GBK。遊戲以楷体 (GBK) 字型繪字，GBK 涵蓋繁體字形，故不可輸出 Big5。
2. **寫回方式**：原地等長覆寫。GBK 簡→GBK 繁 每字皆 2 bytes，等長覆寫文字區即可，EEX 的 section 長度 / scene offset / 指令76 跳轉全部不變，**不需重新序列化**。
3. **Hook 點**：hook `ReadFile`。以 `CreateFileA` 記錄 `.eex` 的 handle，於 `ReadFile` 填好 buffer 後就地轉換（buffer 開頭須為 `EEX\0` magic 才處理）。
4. **其他 hook**：只移除 `FullSentenceCall`/`GameTextPrintf`/`PrintfImpl` 三個；其餘（ScriptStringAnalyse / DrawTextExA / SetWindowTextA / LoadMenuA / DialogText / CreateFontA / MciSendCommandA）全部保留——它們處理的是 Windows 選單 / 視窗標題 / 字型 / 音樂，文字不在 eex 內。

## 變更清單

| 動作 | 檔案 |
|---|---|
| 新增 | `util/EexScript.hpp` — EEX walker + 就地轉換 `EexConvertBufferInPlace(BYTE*, DWORD)` |
| 新增 | `api/ReadFileHook.hpp` — hook CreateFileA / ReadFile / CloseHandle |
| 移除 | `api/FullSentenceCall.hpp`、`api/GameTextPrintf.hpp`、`api/PrintfImpl.hpp` |
| 改動 | `dllmain.cpp`、`Koeicda.vcxproj`、`Koeicda.vcxproj.filters` |
| 保留(不再被 include) | `util/AobScan.hpp`（移植其他遊戲仍會用到） |

## EexScript.hpp（自 CaoCaoEditor 的 EEX.cs 精簡移植）

### 保留
- `CommandType` enum 全部數值——決定每個指令佔幾 bytes，缺一個就 desync。
- Command walker：依指令型別前進，含
  - sub-section 遞迴（`子事件設定` 旗標 / `事件結束` 觸發條件，與 C# 一致）
  - 變動長度的 `變量測試` (0x05) 參數掃描
- 文字指令分類與其 skip_header / skip_tail：
  - skip_header=2：內部信息/信息/場所名/事件名稱設定/旁白/勝利條件/顯示勝利條件/對話
  - skip_header=8：章名/信息傳送
  - skip_header=10：對話2
  - skip_header=2, tail=12：地圖文字顯示
  - skip_header=2, tail=4：選擇框
  - skip_header=6, tail=4：單挑武將出場/單挑對話

### 刪除（冗餘）
- Scene / Section / Command / CommandMessage 物件模型（改為 cursor 直接走 buffer）
- `Dict_Commands_76` 與指令76 跳轉位址解析
- `SerializeToStream` / `SaveAs`（不重序列化）
- `PositionOfTextCommand`（文字位置匯出，僅編輯器需要）
- Big5 編碼路徑（改用 GBK_FullConvert）

### 文字區轉換
逢文字指令，定位文字位元組區間 `[textStart, nul)`（skip_header 之後到 `0x00` 終止符之前），對該 GBK 子字串呼叫 `GBK_FullConvert`；若轉出長度與原文相等則 memcpy 蓋回原位，否則保留原文。

### 安全機制
- 每次讀取都對 `bytesRead` 做邊界檢查，絕不越界。
- 遇未知指令型別或結構越界 → 安全中止該層走訪（已套用的等長修改均合法）。
- 轉換後長度 ≠ 原文長度 → 保留原文不蓋。

## Hook 細節（api/ReadFileHook.hpp）

- `MyCreateFileA`：`lpFileName` 以 `.eex`（不分大小寫）結尾 → 真呼叫後把有效回傳 handle 記入 `std::unordered_set<HANDLE>`。
- `MyReadFile`：真呼叫後，若 handle 在集合內且 `lpBuffer` 開頭為 `45 45 58 00` → `EexConvertBufferInPlace`。
- `MyCloseHandle`：從集合移除（避免 handle 值回收後誤判）。
- 三者皆 kernel32 export，沿用 `HookManager` 的 unhook → 真呼叫 → hook 模式。
- DEBUG 組態下記錄轉換前/後（沿用 DebugLog）。

## dllmain.cpp 改動

- 移除三個 `#include "api/..."` 與三個 `Install_*_Hook()` 呼叫。
- 新增 `#include "api/ReadFileHook.hpp"` 與 `Install_ReadFile_Hook()`。
- 維持 `Read_Dictionary_File` / `Read_Phrase_File` 先行載入（轉換要用）。
- static `HookManager` 建構順序註記照舊（CLAUDE.md）。

## 驗證

- Build `Win32` Release 與 Debug 皆需通過。
- DEBUG 跑遊戲讀第 38 關，`log_gbk.txt` 應出現 ReadFile 轉換前/後紀錄，且遊戲對白顯示為繁體。
- 確認選單 / 視窗標題 / 戰前橫幅（非 eex）仍由保留的 hook 正常轉繁。
