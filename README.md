# ElectronicATE

電子 ATE 自動化測試系統

ElectronicATE 是一套用於電源供應器、電子負載、示波器與繼電器控制的 Windows 桌面測試平台。專案以 Qt Widgets 實作 GUI，透過 MVVM 分層管理 UI、測試條件、儀器設定、XML 保存/載入與硬體控制流程。

---

## 目前功能

### Page1：儀器與通訊設定

- 從 `XML/Instrument.xml` 讀取儀器模板與可選型號。
- 支援多組 Load、Relay、Oscilloscope、InputSource 設定。
- 提供 DC Source1、DC Source2、DC Source3 三台獨立儀器設定。
- 支援 Load Outputs / Relay Outputs / DC Input 數量設定；預設皆為 1，所有模組預設勾選。
- 支援通道型儀器的 subModel、output index、sync role 設定。
- Load sync role 支援 `MASTER` / `SLAVE` / `NONE`。
- 63640 使用 `63600-5` 模板，通道為 `1,3,5,7,9`。
- 通訊設定目前下拉可選 GPIB、TCP/IP、Serial、Modbus RTU。

### Page2：測試條件表格

- Input 條件表：phase mode、Vin、frequency、phase。
- DC Source 群組表：自訂條件名稱、Vin、I Limit，依 Page1 DC Input 數量顯示 Index1～3；隱藏欄位保留設定。
- Relay 表：繼電器輸出條件。
- Load 表：mode、range、name、Vo、Von 與各條件電流/電壓資料。
- Dynamic Load 表：range、Vo、Von、T1/T2 與動態電流條件。
- 支援新增/刪除列、表格 copy/paste/delete、鍵盤導覽與資料同步。
- 依 Page1 output 數量動態更新表頭、欄位與功率欄。
- Load / Dynamic Load 會依儀器 subModel 提供可用 range/mode 選項。

### Page3：手動控制與擷取

- Input Source：Power On / Off / Change。
- Relay 下方提供單一 DC Input 群組控制，依 Page2 條件執行 On／Off／Change；只控制 Page1 數量範圍內且勾選的 DC Source。
- Load：Load On / Off / Change。
- Dynamic Load：Dynamic Load On / Off / Change。
- Relay：Relay On / Off / Change。
- Load 與 Dynamic Load 在 UI 上互鎖，避免同時啟用。
- 示波器 trigger widget 依機型動態建立。
- 支援示波器 PNG、CSV、All CSV、WFM、All WFM 擷取；All WFM 依啟用通道各輸出一個檔案。
- 支援示波器 reconnect request 與 trigger controller 綁定。

### Page4：通訊指令工具

- 提供手動連線、斷線、送指令與通訊記錄。
- 支援 timeout 設定。
- 內建常用 SCPI 按鈕：`*IDN?`、`*RST`、`*CLS`、`*OPC?`。
- 保存 address / command history，方便調試儀器通訊。

### Page5：全自動測試

- 目前僅規劃 UI 介面。

## Load / Dynamic Load Sync 控制

Page1 的 `MASTER` / `SLAVE` / `NONE` 是軟體設定，不會在 Page1 變更當下立即寫入硬體。實際寫入發生在 Page3 執行 Load / Dynamic Load 動作時。

sync enabled 時採用保守順序，避免 sync 線已接上時出現 master-first 的瞬間狀態：

1. 建立所有啟用的 DCLoad channel。
2. 將 Page1 `syncType` 複製到每個 DCLoad 的 `configuredSyncType`。
3. configured master 先送 `SYNC:RUN OFF`。
4. 全部 configured sync members 先送 `SYNC:TYPE NONE`。
5. configured slave 送 `SYNC:TYPE SLAVE`。
6. configured master 最後送 `SYNC:TYPE MASTER`。
7. 建立 sync plan。
8. 寫入 Load / Dynamic Load 參數前，master 再送 `SYNC:RUN OFF`，所有 sync members 暫時切 `NONE`。
9. 逐 channel 寫入 static load 或 dynamic load/T1/T2 設定。
10. restore sync role：`NONE` → `SLAVE` → `MASTER`。
11. master 送 `SYNC:RUN ON`。
12. synchronized members 由 master 送 `LOAD ON`；independent members 各自送 `LOAD ON/OFF`。

注意事項：

- 63640 / 63600-5 的 `1,3,5,7,9` 每個 channel 都視為獨立 sync member。
- Page3 執行前會依 Page1 設定套用 sync role，並用實機狀態建立 sync plan。
- 每個 sync 指令可能因硬體尚未反應完成而 timeout。若 sync sequence 中途失敗，應優先 recovery：`SYNC:RUN OFF`，再將可連線成員切回 `SYNC:TYPE NONE`，再重建角色。

---

## 支援儀器

### AC Source

- Delta A3000：`DE-A3000AB`
- Chroma：`61505`、`61509`、`6530`

### DC Source

Chroma 62000H 系列支援以下 13 個型號，提供 SCPI 設定、輸出控制與量測：

| 型號 | 最大電壓 (V) | 最大電流 (A) | 額定功率 (W) |
| --- | ---: | ---: | ---: |
| 62050H-40 | 40 | 125 | 5000 |
| 62050H-450 | 450 | 11.5 | 5000 |
| 62050H-600 | 600 | 8.5 | 5000 |
| 62075H-30 | 30 | 250 | 7500 |
| 62100H-40 | 40 | 250 | 10000 |
| 62100H-450 | 450 | 23 | 10000 |
| 62100H-600 | 600 | 17 | 10000 |
| 62100H-1000 | 1000 | 10 | 10000 |
| 62100H-30 | 30 | 375 | 11250 |
| 62150H-40 | 40 | 375 | 15000 |
| 62150H-450 | 450 | 34 | 15000 |
| 62150H-600 | 600 | 25 | 15000 |
| 62150H-1000 | 1000 | 15 | 15000 |

硬體控制已完成離線測試，尚未完成實機驗證。

### DC Load

- Chroma 6310：`63101`、`63102`、`63103`、`63105`、`63106`、`63108`、`63112`
- Chroma 6310A：`63101A`、`63102A`、`63103A`、`63105A`、`63106A`、`63107A`、`63108A`、`63110A`、`63112A`、`63113A`、`63115A`、`63123A`
- Chroma 63600：`63610-80-20`、`63630-80-60`、`63640-80-80`、`63640-150-60`、`63630-600-15`
- Chroma 63200A：150 V、600 V、1200 V 系列多型號
- Chroma 63800：`63802`、`63803`、`63804`

### Oscilloscope

- Tektronix DPO7000：待驗證
- Tektronix DPO4000：待驗證
- Tektronix MSO 4/5/6 Series：MSO44(B)、MSO46(B)、MSO54(B)、MSO56(B)、MSO58(B)、MSO58LP、MSO64(B)、MSO66B、MSO68B、LPD64

MSO44B LAN capture 目前以 VXI-11 為主要測試路徑，例如：

```text
TCPIP0::192.168.0.6::INSTR
```

PNG 與 native CSV 透過示波器檔案系統保存後讀回。

### Relay

- Modbus RTU 4CH Relay：`Modbus_RTU_4CH`

---

## 架構

採用 MVVM 分層，介面、資料與儀器控制各自管理。

| 層級 | 職責 | 目錄 |
| --- | --- | --- |
| View | 顯示頁面、接收使用者操作 | `src/views` |
| ViewModel | 處理操作、同步頁面與資料 | `src/viewmodels` |
| Model / Data | 保存儀器設定與測試條件 | `src/models`、`src/data` |
| Service | 執行控制、波形擷取與 XML 存取 | `src/service` |
| Hardware | 儀器驅動與通訊協議 | `src/hardware` |

使用流程：**Page1 設定儀器 → Page2 建立條件 → Page3 手動控制與擷取**。
Page4 提供獨立通訊工具；Page5 為全自動測試的 UI 規劃。

---

## 圖片展示

### Page1：儀器設定

![Page1](screenshots/page1.png)

### Page2：條件設定

![Page2](screenshots/page2.png)

### Page3：手動控制

![Page3](screenshots/page3.png)

### Page4：通訊指令工具

![Page4](screenshots/page4.png)

### Page5：全自動測試（目前僅規劃 UI）

![Page5](screenshots/page5.png)

---

## 開發注意

- 專案提供 tests/ 離線測試；修改後請執行相關 CTest 測試與完整 CMake build。
- 硬體通訊流程多為非同步或背景執行，需留意 lifetime、timeout、thread 與 queued signal。
- Load / Dynamic Load sync 修改必須保守處理實體 sync 線已接上的狀態。
- 不要復原舊 `src/shared/` 架構；目前已拆分為 `hardware/`、`service/`、`infrastructure/`、`ui/` 等層。
- MSO44B LAN capture 目前優先使用 VXI-11，不建議重新開啟 Raw Socket 路徑，除非重新驗證。

---

## TODO

- 增加 Page5 全自動控制。
- 增加更多儀器型號與協議支援。
- 持續拆分過長 UI 與 executor 邏輯。

---

## 授權

ElectronicATE 秉持分享與交流精神公開原始碼。本軟體依「現狀」提供，不提供任何明示或默示之擔保。使用者應自行驗證其適用性，並採取必要的設備保護與安全措施。在適用法律允許的最大範圍內，作者及貢獻者不對因使用或無法使用本軟體所造成的損失承擔責任，包括設備損壞、資料遺失及營運損失。詳細授權條款以 LICENSE 為準；Qt 與其他第三方元件仍依各自授權條款使用。

---

## 作者

Jaxon Su  
GitHub: [@Jaxon-Su](https://github.com/Jaxon-Su)
