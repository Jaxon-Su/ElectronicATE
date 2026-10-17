# 操作與畫面

本頁集中說明各頁功能、任務與報表、負載同步及畫面。各測試的事件順序與搜尋方式見 [TestSequence：量測策略](MEASUREMENT_STRATEGIES.md)，使用前請先確認 [支援儀器與限制](SUPPORTED_INSTRUMENTS.md)。

## 目前功能

**操作流程：設定儀器 → 建立條件 → 手動控制或自動測試 → 保存結果。**

| 頁面 | 主要功能 |
| --- | --- |
| InstrumentSetup：儀器設定 | 設定儀器型號、通道與 GPIB／TCP/IP／Serial／Modbus RTU 通訊；Scan Resources 列出可用資源並複製地址。 |
| TestConditions：測試條件 | 編輯 AC／DC Input、Load、Dynamic Load 與 Relay 條件，支援新增、刪除、複製與貼上。 |
| ManualControl：手動控制 | 控制輸入、負載與繼電器；調整示波器觸發、讀取量測值、保存 PNG／CSV／WFM。 |
| CommandConsole：指令工具 | 手動送出 SCPI 或 Modbus RTU 讀寫；查看通訊記錄、TX／RX Hex 與解析結果；提供 SCPI 常用指令及 63600 設定。 |
| TestSequence：自動測試 | 編排測試任務，支援單組／多組條件、取消、重試及 Excel 報表。 |
| WaveformOcr：Waveform OCR | 本機 Ollama 批次辨識圖片中的 Max／Min／RMS／Mean，可對照原圖修改數值，依來源資料夾匯出 Excel。 |

InstrumentSetup／TestConditions／ManualControl／CommandConsole／TestSequence 設定可透過 XML 保存與載入，僅支援目前完整格式，不轉換舊版設定。WaveformOcr 的辨識結果與核對狀態屬於當次分析，請另匯出 Excel 保存。正常關閉時若有未匯出變更，會提供匯出／放棄／取消；取消或匯出失敗保留資料。Scan Resources 僅列舉 VISA／COM 資源，不代表儀器已回應，也不發送控制指令。ManualControl 的其他儀器可在示波器未連線時獨立操作。

WaveformOcr 完成或停止辨識後，可雙擊 Max／Min／RMS／Mean、按 F2，或按「修改數值」編輯，Enter 儲存、Esc 取消。可輸入含單位的數值（如 `5.1 V`、`-185.3 mA`）；只改數字時會沿用該欄位原有單位，或該列唯一的有效單位。沒有可用單位時請一併輸入單位。清空欄位表示缺值，格式錯誤以黃色底色提示。通道、統計範圍及「已核對」皆為選填，不影響匯出；需要時可勾選「顯示通道／統計範圍」修改。Excel 匯出採用修改後的數值，並保留原始辨識文字、辨識原值及手動修改標記。

### 自動測試項目

- **Single／Group Test（單組／群組測試）**：Static、Dynamic、Turn on、Turn off、Short then turn on、Turn on then short；Group 依序測試多個負載條件，逐負載列出結果。每筆有效 Trial 均保存 Max／Min／RMS／Mean，並分 Rise／Fall 選出完整四值代表結果。
- **Dell/HP Test**：Brown Out 降壓測試，流程見 [TestSequence：量測策略](MEASUREMENT_STRATEGIES.md)。
- **輔助任務**：Relay、Write Oscilloscope、Delay。
- **輸入選擇**：所有量測測試可選 AC、DC 或兩者並用；開／關事件依序操作所選輸入，並非硬體同步切換。
- **結果與報表**：結果顯示於 Tasks；新任務預設不勾選 Report，可用 Report All／Report None 一次全選或取消。勾選的任務追加至同一份 Excel，更換檔名或路徑才建立新檔。經 Excel 修改格式的報表請另用新檔名。

Tasks 顯示摘要；選取一項 Task 後，在下方 **Detail** 切換 Summary／Trials，並篩選 Attempt、Load、Direction、CH。Single 顯示實際負載條件，Group 依 TestConditions 行號及名稱分列。Rise 選 Max 最高那筆，Fall 選 Min 最低那筆，四值整組保留；Excel 的 **Results／Trials** 分別保存代表值與逐次資料。未選方向為 Not run，沒有有效值為 NA，重試各自保存。舊版單表報表請另用新檔名。

### Write Oscilloscope 水平設定

Mode 選擇 **Auto／Manual**。Manual 的 Inputs 可選 **Time Scale + Sample Rate** 或 **Sample Rate + RL**，數字與單位分開，支援鍵盤輸入；第三值依 10 格自動計算。可先改任一輸入，切換 Inputs 時沿用目前計算值。Position 預設 40%。

| Inputs | 範例 | 計算值 |
| --- | --- | --- |
| Time Scale + Sample Rate | 16 us/div、1.25 GS/s | RL ≈ 200k points |
| Time Scale + Sample Rate | 100 us/div、625 MS/s | RL ≈ 625k points |
| Sample Rate + RL | 625 MS/s、625k points | Time Scale ≈ 100 us/div |

**Range check** 依實際設備選擇 MSO44B 標準 31.25M、已安裝的 62.5M 選配，或 Other。MSO44B profile 檢查即時取樣率 1.5625 S/s～6.25 GS/s、RL 最少 1k 及所選容量；Other 保留數值檢查，實際能力由儀器判定。此選項是人工選擇的檢查範圍，並非自動偵測。快捷選單不是官方完整刻度表。

Apply 只保存設定；任務執行時設定兩個獨立值，再讀回實際 Time Scale、Sample Rate、RL 與格數。**Log** 列出 requested／actual；儀器調整時明確標示 adjusted，預覽不視為實際讀回。

## Load / Dynamic Load Sync 控制

InstrumentSetup 的 `MASTER`／`SLAVE`／`NONE` 保存同步角色；編輯設定時不會直接操作硬體。ManualControl 手動控制與 TestSequence 自動測試共用 InstrumentExecutor，在執行 Load／Dynamic Load 動作時套用設定。

啟用同步時，程式先建立與檢查同步群組，再依動作暫停同步、寫入負載參數、恢復角色及切換輸出。同步成員由 Master 控制，獨立成員分別控制；63640／63600-5 的 `1,3,5,7,9` 通道各自參與群組判定。實際同步能力與時序仍須依儀器接線及型號驗證。

## 圖片展示

### InstrumentSetup：儀器設定

![InstrumentSetup](screenshots/instrumentsetup.png)

### TestConditions：測試條件表格

![TestConditions](screenshots/testconditions.png)

### ManualControl：手動控制

![ManualControl](screenshots/manualcontrol.png)

### CommandConsole：通訊指令工具

![CommandConsole](screenshots/commandconsole.png)

### TestSequence：自動測試

![TestSequence](screenshots/testsequence.png)

### WaveformOcr：Waveform OCR

![WaveformOcr](screenshots/waveformocr.png)

[返回文件導覽](README.md#文件導覽)
