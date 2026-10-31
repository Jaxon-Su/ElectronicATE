# TestSequence：量測策略

本頁說明各測試的事件順序、波形搜尋與結果語意。程式如何呼叫這些策略，見 [TestSequence：自動測試流程](TEST_SEQUENCE_FLOW.md)；型號與使用條件見 [支援儀器與限制](SUPPORTED_INSTRUMENTS.md)。

一般量測與 Brown Out 共用 `IMeasurementStrategy` 執行介面，再依測試方式分工。原有五類策略（Static／Dynamic 合併為一類）提供 Search direction：BOTH／Rise／Fall。BOTH 分別搜尋兩個方向；每筆有效事件保存 Max／Min／RMS／Mean，未選方向顯示 Not run。另有兩項 descent 單組測試，使用指定通道的負緣與固定 Level 清單，原有追蹤策略維持不變。

「單組測試」指測試一筆負載條件。示波器 AUTO／NORMAL 是觸發模式，RUNSTOP／Single 是擷取停止方式。**AUTO 參考**採 STOP → AUTO → RUNSTOP → RUN，確認本次新記錄後 STOP 再讀值，因此平直 DC 波形不必跨越 Trigger level。**NORMAL＋Single** 用於正式事件，明確設定單次擷取並讀回確認，仍須有效觸發。

AUTO 參考會回讀觸發模式、停止方式、擷取／觸發狀態與擷取計數，Log 區分參考與正式搜尋。每筆參考等待以 10 秒為基準，依時間範圍與平均次數調整，上限 60 秒並受剩餘 Phase 限制；Brown Out 另保留既有較嚴格期限。無新記錄即失敗，不沿用上一張畫面；過長時間範圍或 FastFrame 會明確報錯。

## 一般量測策略

一般六種測項（含 Group）的 **Vertical margin (10% top/bottom)** 預設關閉。開啟時使用未截波新記錄的 Max／Min，檢查實際 Scale／Offset／Position 下的畫面餘量；已在上下 10% 內就不調整。超出時一般波形置中；窄帶直線（跨度不超過 0.1 div）全正靠上、全負靠下，零／跨零窄帶置中。邊界目標留 10.5% 供讀回誤差，Scale 只在跨度或 Position 範圍不足時擴大，Offset 保持。真正 ADC 截波仍先 Scale ×2，不以截斷極值推算位置；調整後重新擷取。每列 attempt 結束時還原原 Scale／Position，Group 成員共用本次調整；還原失敗中止後續測試。此選項不改 Rise／Fall 或固定 Level。

六種一般測項及各 Group 的 **Trigger source** 在輸出操作、Auto Period 與策略啟動前先套用並讀回確認。新測項預設 CH1；舊設定未選來源時沿用示波器當前來源。選擇必須是可用且已啟用的通道，不改其他量測通道或 Rise／Fall 搜尋方式。

### 1. Static／Dynamic

Load OFF → Input OFF → 關機等待 → 在 OFF 狀態設定條件 → Input ON → 開機等待 → 靜態／動態 Load ON → 負載穩定等待 → 示波器 Trigger AUTO。Capture 開啟時準備階段保持 STOP，由後續 AUTO 參考／NORMAL＋Single 啟動各階段擷取；未開啟時允許準備階段連續 RUN。

接著執行 Auto Period（在各筆 Static／Dynamic 設定中勾選，預設關閉）：連續 3 筆新波形的 PERIOD 穩定、實際畫面包含 **3～8 個週期**即接受，初始已符合時保留時間刻度。範圍外才以 **5 個週期**作調整目標；儀器採用不同刻度但已落在接受範圍時，仍須重新擷取 3 筆穩定波形驗證。最多 24 輪週期量測、6 次水平調整及 15 分鐘；Log 列出目標／實際刻度、Horizontal Mode／Manual Inputs、取樣率、記錄長度與嘗試計數。未啟用則沿用目前時基；Write Oscilloscope 僅設定初始時基，不控制此開關。

新波形完成後，PERIOD 結果可能仍在更新。程式輪詢等待，預算為 **10 秒＋當前畫面時間，最高 60 秒**，並受剩餘總時限限制；有效數值穩定後立即繼續。Log 分開列出擷取與 PERIOD 等待的預算、耗時及結果。期限內仍無週期時有界擴大時間範圍；量化後刻度只要比原值增加超過 1%，即可重新擷取驗證，並不直接判 Pass。沒有前進、查詢錯誤或逾時才到達的有效值都不當成成功；Stop 與清理流程保持。

量測完成後 Scope STOP → Load OFF → Input OFF，清理成功才回報成功。Group 每筆也相同；三段等待的設定見 [Static／Dynamic 流程](TEST_SEQUENCE_FLOW.md#3-staticdynamic-量測)。

自動搜尋共用 `Step＝Trigger CH 實際 Scale÷25`，Level 對齊以零點為基準的整數 Step。參考 Max 向上、Min 向下取嚴格超過極值的下一格；例如 Scale＝4、Max＝21.555，Step＝0.16，起始 Level＝21.6。四種暫態保留擴步與二分搜尋，候選只取整格；相鄰 HIT／MISS 格點即收斂，排除界線外沒有可測格點時回報無有效事件。Step 保留精度（9.999÷25＝0.39996），不量化量測四值；兩種 descent、Brown Out Fixed 與手動指定 Level 不套用此規則。Brown Out Track 使用相同格點。

搜尋時以 AUTO 參考 擷取新的穩態參考 → 切換 NORMAL＋Single → 分別進行 Rise／Fall 搜尋；每輪重新擷取一次並調整 Level，無觸發時 STOP，結束該方向搜尋。

### 2. Turn on

關機 → 放電 → 解除放電 → NORMAL＋Single → 等待 3000 ms／確認 READY → 開機 → 等待觸發並量測。

每次試驗重做完整開機流程，以不同 Level 搜尋事件邊界。每次成功 Single（包含提前觸發後 rearm）都等待 3000 ms；等待可 Stop 並計入原 Phase time limit，不保證等待後必定 READY。

若 Single 在開機前已觸發，先確認自然擷取完成，保持 Input OFF，Rise／Fall 依上述格點規則移至界線外下一格，再重新 Single；確認 READY 才開機。這段避讓不改 Scale，也不重做已完成的放電。提前記錄只供診斷，不作有效事件或量測四值；後續搜尋不返回已知的提前觸發界線。每次 rearm 仍計入原次數及 Phase 時限，通訊錯誤、其他狀態或逾時仍中止。

### 3. Turn off

開始先確認 Scope STOP → NORMAL → Input OFF／Load ON → 開機穩定 → AUTO 參考 穩態參考 → 調整 Level 排除穩態觸發 → NORMAL＋Single 保持待命 → 關機等觸發。

每次 attempt／Group 成員都先統一示波器起始狀態；STOP 或 NORMAL 設定失敗就中止並清理。Turn off 沒有實際 Relay／放電操作。

Turn off 的診斷 Log 列出實際方向、Trigger／Phase 時限、關機等待、Settle、Trial／擷取上限及 Capture／餘量設定，並印 Trigger CH 與啟用通道。Input ON、初始 Settle、AUTO 參考、事件等待及清理區分 begin／complete；每個診斷附 Rise／Fall 的 Phase 已用／剩餘時間。AUTO 參考另標記讀值與 Capture 前後，失敗時保留當前步驟；最後只有 begin 表示操作尚未回報完成。計時沿用原階段預算，同步通訊仍受原 transport timeout，不新增等待或重試。

依結果更新 Level，重新開機穩定後重試。

### 4. Turn on then short

開機穩定 → AUTO 參考 穩態參考 → 調整 Level 排除穩態觸發 → NORMAL＋Single → 等待 3000 ms → Baseline／READY 確認 → 短路等觸發。

事件擷取完成（示波器 STOP）→ 確認 Input OFF → 關機等待 → 保持短路並放電 → 解除放電 → 解除短路 → 讀取量測／保存 Capture。下一輪重新開機穩定、NORMAL＋Single 待命後再施加短路。

### 5. Short then turn on

確認 Input OFF → 關機等待／放電 → 解除放電 → 短路 → NORMAL＋Single → 等待 3000 ms／確認 READY → 開機 → 等待觸發。

事件擷取完成（示波器 STOP）後，使用相同的短路退出流程，再讀取量測／保存 Capture。每次試驗重做完整流程，再測下一個 Level。

兩種短路測試（含 Group）在無觸發、Stop、例外及最後結束時也執行退出流程。**任一 Input OFF 失敗，不釋放短路 Relay、不開始傳檔，也不自動重試或執行後續條件。** 放電 ON／OFF 失敗同樣不繼續解除短路；Scope STOP、Load OFF 仍會嘗試。錯誤狀態與當次設定交給 ManualControl。

關機等待為 300 ms，放電時間使用該任務的 `Discharge time`；這些等待不是 DUT 殘壓量測，仍須按治具與 DUT 驗證。

兩種 Short 的單組、Group 流程圖與時間軸，見 [Short 測試流程與時間](#short-測試流程與時間)。

### 6. Turn off (descent)／Turn on then short (descent)

Dialog 的 Trigger CH 選擇負緣觸發波形，Edge 固定 Fall。量測沿用示波器已啟用的通道，不另選量測 Channel；例如 CH4 接 20 V 降至 0 V 的觸發訊號，CH1～3 保持原有量測設定。所選觸發 Channel 必須是該機型的實體通道。

Turn off (descent) 在 Trigger CH 旁提供 **Enable**（預設勾選）：勾選會開啟該通道顯示並納入量測，取消會關閉顯示，仍使用該 CH 作負緣觸發。每次執行在 STOP 後套用並確認顯示狀態，其他 CH 不變；結束時保留這項顯示選擇。若全部 CH 關閉，仍執行觸發，但沒有通道四值。Turn on then short (descent) 維持觸發 CH 須已啟用的原設定。

關閉的 Trigger CH 不提供 WFM／CSV Capture；單通道匯出請另選已啟用 CH，使用預設 Trigger 選取時會明確回報通道不可用。AllWFM／AllCSV 只保存已啟用通道，沒有啟用通道時回報不可用；PNG 可照常保存完成畫面。

Trigger levels 清單以 Add level 新增、Delete 刪除，直接編輯每列數值，沒有上下移動按鈕；依列順序執行，保留重複值、0 與負值，輸入最多四位小數。Trials per level 預設 1。每個 Level 的每次 Trial 都重新上電、等待 Settle、啟動 NORMAL＋Single 並確認 READY，才執行關機或短路；成功或依下述失敗政策完成清理後才進入下一次，不搜尋或自動改寫 Level。

兩種 descent 在上電穩定、Short OFF 後先用 AUTO 完成記錄檢查啟用通道；截波通道的 V/div ×2，重新擷取至不截波才開始正式事件。關機／短路事件仍截波時，保存診斷後擴大量程，重新上電重做同一組 Level／Trial。預檢與重試共用調整上限及 Phase time limit，測試期間只擴大、結束後還原自動改過的刻度；Turn off 的 Trigger CH 顯示 OFF 仍維持原來源，全部 CH 關閉時略過量程預檢。Turn on then short (descent) 的事件記錄先完成短路退出，再診斷、擴量程與重試；上電中的 AUTO 預檢尚未施加短路。

兩種 descent 每次成功啟動正式事件 Single 後，先等待 **3000 ms** 給示波器處理，再查 READY；確認 READY 才 Input OFF 或 Short ON。這段等待可由 Stop 中斷，並計入 Phase time limit。Turn off (descent) 關機前若確認提前觸發且記錄自然完成，該 Trial 標 Fail，確認 Input OFF／Scope STOP 與診斷保存成功後，跳過同 Level 剩餘 Trials，繼續下一列 Level，整體仍為 Fail。SAVE 但仍在擷取、通訊錯誤、逾時或清理失敗仍中止。等待期間不施加短路；Single 失敗不進入等待，AUTO 預檢不增加此固定等待。Turn on then short (descent) 的短路前自然完成記錄或事件等待 MISS，該 Trial 標 Fail；完整短路退出、Scope STOP 與必要診斷保存成功後，繼續全部剩餘 Trial／Level，整體仍 Fail。MISS 沒有完成記錄，不保存舊波形或四值。擷取未完成逾時、Stop、通訊、Scale／Capture 或清理錯誤仍中止。

```mermaid
flowchart TD
    A["Input OFF／必要的短路退出<br/>Scope STOP、指定 Channel／Falling／NORMAL、Load ON"] --> B["依序取 Level／Trial<br/>確認 Input OFF，等待 300 ms<br/>Short 類另完成放電與解除短路"]
    B --> D{"測試項目"}
    D -->|"Turn off (descent)"| P["Input ON → Settle → AUTO 啟用通道預檢<br/>截波則 V/div ×2 並重新擷取"]
    P --> Q["設定固定 Level／NORMAL → Single<br/>等待 3000 ms → 檢查 READY 與擷取狀態"]
    Q -->|"READY 且仍在擷取"| E["Input OFF → 等待負緣觸發"]
    Q -->|"確認提前觸發、自然完成"| N["該 Trial Fail，Input OFF／Scope STOP<br/>依設定保存診斷，不當事件"]
    N -->|"清理／保存成功"| O["略過同 Level 剩餘 Trials<br/>取下一列 Level，整體保持 Fail"]
    O --> I
    Q -->|"其他錯誤／逾時"| K
    N -->|"清理／保存失敗"| K
    D -->|"Turn on then short (descent)"| F["Input ON → Settle → AUTO 啟用通道預檢<br/>截波則 ×2 並重新擷取 → 固定 Level → Single<br/>等待 3000 ms → READY／擷取狀態"]
    F -->|"READY 且仍在擷取"| T["Short ON → 等待負緣觸發<br/>完整短路退出"]
    F -->|"短路前確認自然完成"| U["該 Trial Fail／NA<br/>短路退出、Scope STOP；提前記錄僅保存診斷"]
    F -->|"其他錯誤／逾時"| K
    T -->|"MISS，沒有完成記錄"| U
    U -->|"清理與必要保存成功"| I
    U -->|"清理／保存失敗或到期"| K
    E --> R{"已完成事件且截波？"}
    R -->|"是，仍在調整／時間上限內"| S["保存診斷 → 擴大截波通道量程<br/>保留同一 Level／Trial"]
    S --> B
    R -->|"否"| G
    R -->|"超出調整／時間上限"| K
    G{"取得有效完成記錄？"}
    T -->|"事件完成"| R
    T -->|"其他錯誤／擷取未完成逾時"| K
    G -->|"是"| H["讀取已啟用通道四值<br/>保存勾選的 Capture、保留 Trial"]
    H --> I{"還有 Level／Trial？"}
    I -->|"是"| B
    I -->|"否"| J["清理輸出與 Scope STOP<br/>還原自動改過的刻度"]
    G -->|"否"| K["本次失敗、保留先前 Trial<br/>完成記錄視情況保存診斷"]
    K --> J
```

Advanced 的 Trigger timeout 控制每次等待；Phase time limit 是整組 Level 總時限，涵蓋預檢、重試、各 Level／Trial 的等待、量測與 Capture。兩種 descent 的預檢與事件共用最多 8 回合量程調整，固定 Level 與總預算不重設。Turn off 提前觸發依上述政策跳到下一 Level，MISS 仍中止；short descent 提前自然完成或 MISS 則依序繼續下一 Trial，曾有 Fail 就維持整體 Fail。超出上限仍截波、通訊、擷取未完成逾時、Phase 到期或清理失敗均停止清單。Stop 與清理失敗依既有規則阻擋後續任務與重試。Short 仍採 Input OFF → 等待／放電 → 解除短路，再讀值或傳檔；OFF／放電失敗時保留短路並交接 UNKNOWN。

結果方向為 Fall；Step 對應清單列號，Trial 對應該列次數，Trigger Level 保存當次設定。Turn off (descent) 的 Detail 預設 Trials，short descent 保留預設 Summary；兩者可切 Trials 列出每筆有效事件的完整四值，Summary 查看最低 Min 那筆，Rise 顯示 Not run。提前觸發／MISS 的 Fail，以及 Turn off 略過的 Skipped 列有原因與 Level／Trial，四值為 NA，不占有效 Record 序號、不參與代表值；後續有效列可為 Pass，Note 明示整體測項仍 Fail。Excel Trials 依 Step／Trial 保留全部，Results 保留代表值及失敗／略過列。Turn off 全通道隱藏時仍列觸發記錄與 Level，四值為 NA。

### 原有追蹤策略的量測方式

- 每筆有效 Trial 保存同一次事件的 Max／Min／RMS／Mean。Rise 選 Max 最高那筆、Fall 選 Min 最低那筆的完整四值；不跨 Trial 拼接數值。
- Static／Dynamic 先量穩態，再調整觸發門檻；四種暫態則重複開關機或短路，逐步縮小搜尋範圍。
- 波形超出垂直量程時，自動放大刻度後重測；任一量測欄位查詢通訊失敗或搜尋超過限制，均回報失敗。合法 NA 仍依原量測規則處理。

Turn on／Short then turn on（均含 Group）在 Input ON 前觀察到 TRIGGER，或 SAVE 且確認自然完成時，可保存診斷並向外移動 Level 重試；TRIGGER 尚未完成時只在剩餘 Phase 內等待自然完成，不以 STOP 製造完成。一般 Turn on then short（含 Group）在 Short 前觀察到同類記錄，交給下述 Baseline 避讓流程。AUTO、Invalid、通訊錯誤、SAVE 但仍 Running 或 READY 逾時不重試。其餘暫態維持 READY 失敗時，若有自然完成記錄便保存診斷後回報失敗。Capture 須已啟用，Short 都先完成 Input OFF／放電／解除短路；提前記錄不計入 Trial／結果。

### 沒有觸發時

| 策略 | 處理方式 |
| --- | --- |
| Static／Dynamic | 保留已量到的穩態與搜尋結果。 |
| Turn on／Short then turn on | 保留另一側的有效事件；兩側都沒有事件則回報失敗（NA）。 |
| Turn off／Turn on then short | 搜尋正常結束後，將穩態值與事件值合併，保留較大 Max、較小 Min；沒有事件時使用穩態值。 |
| Turn off (descent)／Turn on then short (descent) | 本次清單失敗並停止後續 Level，保留先前有效 Trial；不使用穩態參考補值。 |

### TestSequence 量測值的來源

每個 Attempt／Load／Direction／CH 分開保存逐次事件與方向代表結果。Pass 表示策略與清理按目前規則完成，並非 DUT 規格上下限比對，也不保證每欄都有數值。

| 方向 | 代表 Trial | 四值來源 |
| --- | --- | --- |
| Rise | Max 最高的有效 Trial | 該筆 Max／Min／RMS／Mean 整組複製 |
| Fall | Min 最低的有效 Trial | 該筆 Max／Min／RMS／Mean 整組複製 |

平手保留較早的 Trial；NA 不從其他記錄補值。未選方向為 Not run。原 **Turn off（含 Group）** 在選取方向正常搜尋完成但沒有有效事件時，從同方向的初始 AUTO 與後續有效 Baseline，逐通道選 Rise 最高 Max／Fall 最低 Min 那筆的完整四值；同值保留較早記錄。**Turn on then short（含 Group）** 維持使用初始 AUTO 四值。有有效事件時，兩者都優先從事件選值，最後一次未觸發不覆蓋先前結果，也不以參考補事件 NA。其他策略沒有有效事件時仍顯示 NA。AUTO 參考、Baseline、截幅、未觸發與未完成查詢的事件不冒充有效 Trial。

Turn off／Turn on then short 的穩態回退在 Detail Summary／Trials 與 Excel Results／Trials 均標示 `Steady reference (no transient trigger)`；Record／Step／Trial／Trigger Level 留空，不增加事件 Trial 次數。Turn off 的 Log 會列出勝出的 Baseline 參考四值；Capture 筆數包含診斷與參考，不等於事件數。參考讀取失敗、Stop 或逾時不改判成功；合法 NA 保留原值，不跨記錄補齊。

Static／Dynamic 每筆有效搜尋擷取各留一筆；暫態按每個 Step 的 Trial 保存；Brown Out Fixed 為 Fall，Track 沿設定方向。單組記錄實際負載條件，Group 依 TestConditions 行號與名稱逐負載保存，重試以 Attempt 分開。失敗／取消保留已取得資料，未執行條件為 Skipped。

Tasks 顯示摘要；下方 Detail 提供 Summary／Trials 及目前所選 Task 的篩選器，Log 顯示執行訊息。Excel 使用 Results／Trials 兩張表，保存 Record、Step、Trial、Trigger Level 與四值；相同新格式檔案採追加，舊版單表不覆寫。Record 是有效事件序號，Step 是該方向的搜尋步驟（descent 為 Level 清單列號），Trial 是該步驟的試驗次數；它們不是 Capture 檔案編號。


## Short 測試流程與時間

兩種 Short 共用：**事件完成、示波器 STOP → Input OFF → 等待／放電 → 解除短路 → 讀值／保存波形 → 下一次事件。** Capture 啟用時才保存檔案。

### 1. 單組起始與搜尋

```text
整批預檢通過
  → 未選用但已配置的 Input OFF
  → 所選 Input OFF → 等待 0.3 秒
  → 保持 Short ON，放電 D 秒 → Discharge OFF → Short OFF
  → Input OFF 下設定本筆 Load → Load ON
  → 執行所選方向：Rise → Fall（單邊設定只跑該方向）
  → 各方向依 Level、Trials 重複對應事件流程
  → 確認退出完成 → Scope STOP → Load OFF → 回報結果
```

`Discharge time` 預設 D＝10 秒；每個 Level 的 `Trials per level` 預設 3 次，依觸發結果調整 Level。同一負載的事件之間 Load 保持 ON，整筆結束才 OFF。波形截幅時先保存診斷，再調整 Scale 重測。

### 2. Short then turn on：先短路，再開機

```mermaid
flowchart TD
    A["Input OFF、Load ON<br/>起始準備或上次退出已完成"] --> B["Short ON"]
    B --> C["設定 Level／方向<br/>NORMAL＋Single → 等待 3000 ms → 檢查 READY"]
    C -->|"READY"| D["Input ON<br/>等待事件觸發"]
    C -->|"上電前觸發且自然完成"| J["共用退出：Input OFF／放電／解除短路<br/>按設定保存診斷，不讀事件數值"]
    J --> K["Level 向外移至下一整數 Step 格點<br/>確認剩餘 Phase／次數／Stop"]
    K --> A
    C -->|"其他錯誤／逾時"| L["中止並執行安全清理"]
    D --> E["擷取完成：示波器 STOP"]
    E --> F["共用退出<br/>Input OFF → 等待／放電 → 解除短路"]
    F --> G["讀取 Max／Min<br/>保存波形"]
    G --> H{"還有 Trial／Level／方向？"}
    H -->|"是"| A
    H -->|"否"| I["Scope STOP → Load OFF<br/>回報結果"]
```

**短路 → NORMAL＋Single → 等待 3000 ms／確認 READY → 開機 → 擷取完成 → 關機／放電／解除短路 → 保存。** 此模式不等待 Settle time；Single 後處理等待期間已維持 Short ON，Stop／Phase 到期仍完成安全退出。

上電前的提前記錄也先走安全退出，成功後才保存診斷並重新 Short ON／Single。這與 Turn on 保持已放電狀態的重試不同；任一退出步驟失敗就中止，不繼續避讓或上電。一般事件的 Rise／Fall 搜尋與代表值規則不變。

等待觸發預設上限 10 秒；未觸發也執行共用退出，該次為 MISS、不保存新波形。所選方向至少取得一筆有效事件，才能完成量測。

### 3. Turn on then short：先開機，再短路

```mermaid
flowchart TD
    A["Input OFF、Short OFF、Load ON"] --> B["Input ON → 等待 Settle time<br/>預設 5 秒"]
    B --> C["本方向的 AUTO 參考 穩態參考<br/>讀取／保存"]
    C --> D["設定 Level／方向 → NORMAL＋Single → 等待 3000 ms<br/>排除 baseline → READY／擷取狀態確認"]
    D -->|"短路前已自然完成"| E["關機／放電／安全釋放 → 診斷 baseline<br/>Level 向外避讓，重建搜尋區間"]
    E --> F["Input ON → 等待 Settle time"]
    F --> D
    D -->|"仍 READY"| G["Short ON → 等待事件<br/>擷取完成：示波器 STOP"]
    G --> H["共用退出<br/>Input OFF → 等待／放電 → 解除短路"]
    H --> I["讀取 Max／Min<br/>保存波形"]
    I --> J{"搜尋下一步"}
    J -->|"同方向繼續"| F
    J -->|"下一方向"| A
    J -->|"完成"| K["Scope STOP → Load OFF<br/>回報結果"]
```

**開機 → 穩定 → AUTO 參考 穩態參考 → NORMAL＋Single → 等待 3000 ms → 排除 baseline → READY → 短路 → 擷取完成 → 關機／放電／解除短路 → 保存。** 每個方向只在開始時取得參考；後續 Trial 重新開機、等待穩定後繼續。AUTO 參考不增加這段處理等待。

施加短路前，先在 Input ON、Short OFF 下排除 baseline，預設等待 10 秒；觀察結束後還會檢查 READY 與擷取狀態。若此時已自然完成，這輪不施加短路：先安全退出，再診斷 Baseline；以該記錄極值及目前 Level 為界，Rise 往上、Fall 往下取界線外下一整數 Step 格點，重新上電／穩定／Single。新 Baseline 會重建搜尋區間，但保留已完成真事件；不向內重訪排除界線。baseline 不計入事件極值。事件未觸發也退出；正常搜尋完成才使用既有穩態回退。

若該 Baseline 同時需要擴大量程，排除界線仍保留：觸發通道未截波時使用改刻度前的極值；觸發通道已截波時只保留已知 Level／先前界線。下一輪依調整後讀回的 Trigger CH Scale÷25 格點向外避讓，次數及 Phase 時限不重新起算。

最後一次狀態查詢與 Relay 實際動作並非原子操作，兩者之間的觸發仍需硬體同步或時間戳才能完整辨識；本流程不宣稱能排除所有物理時序競態。Stop、通訊／讀值／保存錯誤、原 Phase／次數上限及安全退出失敗仍會中止，不無限重試。

程式中的 `waitForBaseline()` 對應施加短路前的背景觸發檢查，`waitForEvent()` 對應短路後的事件等待；兩者都不取代上電後的 Settle time。背景檢查若通訊失敗或擷取未完成，會中止並清理，不繼續施加短路。

### 4. 共用退出與時間

```mermaid
flowchart TD
    A["擷取完成／未觸發／Stop／例外"] --> B{"所選 Input 全部 OFF 成功？"}
    B -->|"是"| C["保持 Short ON<br/>等待 0.3 秒"]
    C --> D["Discharge ON → 等待 D 秒<br/>Discharge OFF → Short OFF"]
    D -->|"成功"| E["退出完成<br/>已完成記錄才讀值／保存"]
    B -->|"否"| F["保留短路保護<br/>停止後續 Relay 動作與測試"]
    D -->|"任一步失敗"| F
    F --> G["仍嘗試 Scope STOP、Load OFF<br/>交由 ManualControl 恢復"]
```

**Input OFF 成功 → 0.3 秒 → 保持短路並放電 D 秒 → 解除放電 → 解除短路 → 讀值／保存。** 完成的波形保持 STOP，退出成功後才傳檔。

D＝10 秒時，一次退出等待為 **10.3 秒**，另加設備操作與傳檔時間。Stop 或異常同樣先退出；Input OFF／放電失敗時保留短路保護，不執行後續 Group 或重試。事件已退出且沒有再次上電／短路，最後收尾不重複放電。

### 5. Group：逐負載重做單組

```mermaid
flowchart TD
    A["依 TestConditions 順序選取下一筆 Load"] --> B["起始準備<br/>Input OFF → 0.3 秒 → 放電 D 秒 → 解除 Relay"]
    B --> C["Input OFF 下設定 Load → Load ON"]
    C --> D["執行對應 Short 單組<br/>每次事件退出後才保存"]
    D --> E["完成搜尋 → Scope STOP → Load OFF<br/>保存本負載 Max／Min 與狀態"]
    E --> F{"本筆成功且還有 Load？"}
    F -->|"是"| A
    F -->|"全部完成"| G["Group 完成"]
    F -->|"失敗／取消"| H["停止本次 Group<br/>後續負載標示 Skipped"]
```

以 **100% → 50% → 10%** 為例，每個負載都完整執行「起始準備 → 單組搜尋 → 退出與 Load OFF」。前一筆成功才換下一筆；下一筆重新等待 0.3＋D 秒，成員之間沒有額外 Group 間隔。

TestSequence 與 Excel 逐負載、逐通道記錄 **Max／Min** 及狀態，不混合不同負載；失敗／取消保留已取得資料。Capture 序號在同次 Group 內接續。

## Brown Out（Dell/HP Test）

選擇 AC／DC Input、Load 及降壓來源 → 等待穩定 → 逐步降低指定來源的 Vin，另一種輸入維持供電 → 由負載讀回 Vout 確認關機，同時擷取示波器波形。

- **Fixed Fall (Vout)**：NORMAL＋Single，以指定輸出電壓通道與 Fall Level 擷取一次事件。
- **Track Level**：未開 Capture 時為 NORMAL＋連續 RUN，開啟時為逐筆 NORMAL＋Single；在一次降壓中隨有效波形調整 Level，逐筆保存完整四值，依方向選出同一筆代表結果。
- Step down 預設 0.5 V；Advanced 的 Ignore 預設勾選，使用內建等待與關機確認時間。

Brown Out 沿用示波器目前時基與水平位置，不提供 Auto Period。取得 AUTO 參考並成功啟動事件 Single／RUN 後，等待 **3000 ms** 再確認 READY，才開始降壓；初次等待期間持續監測 Vout，異常或 Stop 會退出並清理。Track 開啟 Capture 後，在查詢／讀取記錄前暫停新的降壓步進，等正在執行的電壓指令結束，才讀取／保存。保存完成、更新 Level、重新啟動 NORMAL＋Single、等待 3000 ms 並確認 READY 後才恢復降壓；Vout 監測與異常清理持續執行。

```mermaid
flowchart TD
    A["示波器擷取中<br/>允許降壓步進"] --> B["暫停新的降壓步進<br/>等待進行中的電壓指令結束"]
    B --> C{"查詢／讀取記錄"}
    C -->|"尚未完成"| A
    C -->|"已完成"| D["保存波形<br/>Vout 監測持續"]
    D --> E{"記錄有效且仍需追蹤？"}
    D -->|"保存失敗"| G
    E -->|"是"| F["更新 Level → NORMAL＋Single → 等待 3000 ms<br/>確認 READY 才恢復降壓"]
    F --> A
    F -->|"READY 失敗／超時"| G
    E -->|"觀察結束"| G["輸出 OFF／Scope 還原<br/>依結果回報"]
    C -->|"查詢失敗但已確認完整記錄"| H["嘗試保存診斷<br/>保留原始錯誤"]
    H --> G
    E -->|"記錄無效"| G
```

暫停時間不計入降壓步進間隔與進度期限，恢復後不補發一串降壓指令；3000 ms 處理等待之後，READY 查詢及傳輸仍各自有原期限。Track 未開 Capture 的後續 Level 更新維持連續 RUN，不加入重複等待。Vout 關機確認／尾段觀察使用實際時間。讀取、保存、READY 或監測失敗，以及使用者 Stop，都停止後續降壓並清理。量測結果僅涵蓋成功讀回的有效波形。

[返回文件導覽](README.md#文件導覽)
