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

**2. Inline API hooking**，透過 `HookManager` (`HookManager.hpp`)。每個 hook 會把目標系統函式的前 5 bytes 覆寫成一個指向我們替代函式的相對 `jmp` (`0xE9 + 4-byte offset`)。各處都採用相同模式：`unhook()` → 呼叫真正的函式 → 再次 `hook()`，讓 reentrant/真實呼叫得以通過。`Address.hpp` 只是把指標重新解讀成 4 bytes 以取得 offset。被 hook 的主要系統函式：

   - **`ScriptStringAnalyse` (usp10.dll)** — 遊戲用來繪製字串的 Uniscribe text-shaping 呼叫。`MyScriptStringAnalyse` 攔截輸入字串，先做中文轉換再往下傳。這是翻譯補丁的核心。已標記為 Unicode／空字串的情況 (`cString <= 0 && iCharset != -1`) 會原封不動通過。

   - **`mciSendCommandA` (winmm.dll)** — 把遊戲的 CD-audio 播放導向本地 MP3 檔。此 hook 攔截 `cdaudio` 的 `MCI_OPEN`，回傳一個 sentinel device id (`MAGIC_DEVICE_ID = 0xBEEF`)，並在 `MCI_PLAY` 時把要求的 CD track 編號對應到 `music\%02d.mp3`，以 MCI element 的方式開啟／循環播放。其他針對該 sentinel device 的 MCI 訊息則 proxy 給真正的 MP3 device。(於 commit「處理三國志孔明傳音樂 hook」加入。)

   - **`CreateFontA` / `CreateFontIndirectA` (gdi32.dll)** (`api/CreateFontA.hpp`) — 把遊戲繪製用字體一律換成「楷体」(Windows 內建 `simkai.ttf`，標楷體風格) 並開灰階抗鋸齒，改善原本宋體＋無抗鋸齒的觀感。`MyCreateFontA`/`MyCreateFontIndirectA` 改呼叫**寬版** `CreateFontW`/`CreateFontIndirectW`，face name 用寬字面 `L"楷体"` (避開 ANSI/GBK 編碼問題)；**charset 沿用遊戲原值**——轉換後的字串仍以 GBK 繪製，而 `楷体` 支援 GB2312/GBK 且涵蓋繁體字形，故無需更動 charset；`lfQuality` 覆寫為 `ANTIALIASED_QUALITY` (刻意不用 CLEARTYPE，以免點陣/透明背景出現次像素彩邊)。無條件替換所有字體。(此處因呼叫的是 W 版、與被 hook 的 A 版不同函式，故無 reentrant 風險。)

**3. 中文轉換** (`ChsToCht.hpp`)，分「繪製端」與「整句端」兩層處理，把「一簡對多繁」的歧義字交給有上下文的整句端決定：
   - **繪製端** (`ScriptStringAnalyse`)：`UTF16LE_CHS_To_CHT` 透過 Win32 `LCMapStringW(LCMAP_TRADITIONAL_CHINESE)` 做主要的逐字簡轉繁；接著 `UTF16LE_KeepAmbiguousAsIs` 把**出現在 `dictionary.txt` 的一對多歧義字還原回原簡體**，不在此處逐字亂轉，留給整句端用上下文判斷。
   - **整句端** (`FullSentenceCall` → `GBK_ResolveAmbiguousSentence`)：對話框渲染進入點，傳入的是**完整簡體 GBK 句子**。先用詞庫 (`phrases.txt`) 對原始簡體做最長匹配修正歧義詞，未被詞庫覆蓋的歧義字再套單字預設 (`dictionary.txt`)。這是唯一的整句執行點。
   - 注意：因為繪製端會把 `dictionary.txt` 裡的字延後到整句端才轉，而整句端只在對話框跑，所以**判準是「`LCMapStringW` 有沒有自己轉對」**：放進 `dictionary.txt` 的應是 `LCMapStringW` 轉不出來的字 (一對多歧義字的預設、或它不認得的確定性字)——這些選單本來就已顯示簡體，加進來零退步、只改善對話。**切勿放 `LCMapStringW` 已能正確轉換的字**，那會被延後副作用害選單等非對話文字倒退回簡體。
   - (`UTF16LE_FixOneToMany` 為早期逐字修正版，現行管線已不使用。)

## 內部函數 hook 的定位 (特徵碼掃描)

`PrintfImpl` / `GameTextPrintf` / `FullSentenceCall` 不是系統 API，而是遊戲內部函數，靠 `AobScan.hpp` 定位：`FindAddress(sig, mask, hardcodedRva)` 先驗證寫死的 RVA、不中再掃所有可執行區段。同一引擎不同遊戲，這些函數可能位於相同 RVA，但因編譯版本不同而 prologue/結尾不同。

## 移植到同引擎的其他遊戲 (多 sig / 呼叫慣例 / 脫殼)

本補丁的引擎也被天龍八部 (`Ekd5.exe`)、云荒逍遥传等同人作品使用。移植時踩過三層坑，詳見 `notes/2026-06-08-云荒逍遥传移植-脱壳与呼叫慣例.md`：

1. **加殼遊戲要先 dump 記憶體，不能靜態掃。** 例如云荒 exe 189MB、`SizeOfImage` 僅 ~7.7MB、imports 被砍到只剩 `KERNEL32`/`USER32`、section 名稱亂碼且 `VirtualSize ≫ RawSize` = 已加殼。由於 `FindAddress` 本就是 runtime 掃 `GetModuleHandle(nullptr)` 的**記憶體**（殼已解密的 code），所以用 `tools/mem_dump.py` 把執行中遊戲的主模組 dump 下來 (`OpenProcess`+`ReadProcessMemory`，外部讀取不易觸發反調試)，再對 dump 跑 AOB scan / 反組譯即可。

2. **同 RVA、不同 codegen → 多特徵碼。** 同一函數在不同遊戲常落在相同 RVA 但 prologue bytes 不同 (例 `FullSentenceCall` 在天龍與云荒都在 RVA `0x2C8B0`，但前導指令不同)。改用 `AobScan.hpp` 的 `FindAddressMulti(pats, count, rva, &outIndex)`：依序試多組 sig、任一命中即回傳，並回報命中第幾組。`FullSentenceCall.hpp` 即放 A(天龍/曹操)、B(云荒) 兩組變體 sig。

3. **同函數、不同呼叫慣例 → 依變體切換 hook。** 最隱蔽的坑：同一邏輯函數可能被編成不同呼叫慣例。`FullSentenceCall` 在天龍/曹操是 `__cdecl` (結尾 `8B E5 5D C3`)、在云荒是 `__stdcall` (結尾 `8B E5 5D C2 10 00`，`ret 16`)。若用錯慣例呼叫，stack 會被雙重清理 → ESP 失衡 → Debug build 跳 `Run-Time Check Failure #0 (ESP not properly saved)`、Release 則堆疊損毀。故 `FullSentenceCall.hpp` 備有 `MyFullSentenceCall_cdecl` 與 `MyFullSentenceCall_stdcall` 兩個 hook 函數 (轉換邏輯共用 `FullSentenceCall_Resolve`)，`Install_FullSentenceCall_Hook` 依 `FindAddressMulti` 回報的 `outIndex` 裝對應的那個。**判定呼叫慣例的方法：看函數 epilogue 是 `ret` (C3, cdecl) 還是 `ret N` (C2, stdcall)。** 新增變體時，hook 函數與其 typedef 的慣例都要對齊原函數。

## 編輯翻譯字典

翻譯修正有兩個檔案，對應兩個層級的轉換：

- **`Koeicda/dictionary.txt` — 單字預設對應 (fallback)。** 格式為每行 `簡 : 繁` (簡體字、空格、冒號、空格、繁體字)，UTF-8 編碼。每個 token 只取 **第一個字元** (`ChsToCht.hpp` 讀 `[0]`)。loader (`Read_Dictionary_File`) 會剝除第一行開頭的 UTF-8 BOM。這裡放 `LCMapStringW` 轉不出來的字：一是「一簡對多繁」字在沒有上下文時的**預設**選擇 (例如 `里 : 裡`)，二是 `LCMapStringW` 不認得的確定性 1:1 字。判準與「切勿放 `LCMapStringW` 已轉對的字」的原因見上節繪製端的延後轉換副作用。
- **`Koeicda/phrases.txt` — 詞庫例外 (優先)。** 格式為每行 `簡詞 : 繁詞` (簡繁字數必須相同，否則 `Read_Phrase_File` 跳過該行)。`GBK_ResolveAmbiguousSentence` 在整句階段先用此詞庫對**原始簡體**做貪婪最長匹配，命中的詞會覆蓋單字預設 (例如 `公里 : 公里` 讓 `里` 不被轉成 `裡`)。詞庫以**首字**分桶索引 (`CHS_TO_CHT_PhraseIndex`，`Read_Phrase_File` 載入後把每桶按詞長遞減排序)，比對時每個字位置只掃首字相同的那一桶、第一個命中即最長匹配，因此詞庫可擴張到數千條而不致拖慢繪製。簡繁等長的限制也讓命中後可直接前進詞長。

**新增例外時的鐵則：往 `phrases.txt` 加「簡體詞 : 繁體詞」，key 一律是原始簡體輸入。** 絕對不要用「先全部轉錯、再修正錯誤輸出」的反向做法 (例如拿已轉錯的 `公裡` 當 key 去修回 `公里`)，那會讓修正規則跟 `LCMapStringW` 的行為綁死、例外無止盡增長且無法區分歧義。詞庫比對發生在套用單字預設**之前**、且針對**原始簡體**，這才是穩定的設計。`dictionary.txt` 只放單字預設，不要拿來放反向修正。

## 慣例／注意事項

- 所有程式碼檔案必須使用"具有BOM的UTF-8"格式來儲存，不然會編譯錯誤。
- `pch.h` precompiled header 為必要；`pch.cpp` 是建立 PCH 的 translation unit。
- 這些 hook 都是 global static 的 `HookManager` instances；其 constructor 會在載入時擷取原始 bytes。請勿更動相對於 `DllMain` 假設的建構順序。
