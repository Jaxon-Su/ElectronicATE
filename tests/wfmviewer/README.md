# WFM Viewer software tests

[測試總入口](../README.md) · [CI 與依賴](../ci/README.md)

本套件以正式 reader／CSV／statistics／ViewModel／Widget 來源，測試類比 WFM、各種取樣編碼與大小端、錯誤／取消、CSV、峰值垂直範圍／可選上下界連線、模式按鈕、縮放／平移、時間／振幅游標直接拖曳（交點同時操作雙軸）及精簡版面。使用 Qt offscreen；完整 MainWindow、原生對話框與實機驗收另計。

共同 runner 的 `wfmviewer.all` 是一個 CTest，內部固定 101 個 QtTest 結果（99 個功能資料列、initTestCase、cleanupTestCase）。整個測試物件都執行，並以 [manifest](../ctest-manifest.json) 檢查方法與每個資料列，新增／移除方法或資料列必須一併審查預期集合；不能用跳過或自動刷新清單消除缺測。

依賴使用 [Tektronix tm_data_types](https://github.com/tektronix/tm_data_types/tree/6d135b97d8eb2b1289c4b4fd7b03b2814e16f9e0/tests/waveforms) 的固定樣本；來源版本、每檔 SHA256 及 [Apache-2.0 LICENSE](https://github.com/tektronix/tm_data_types/blob/6d135b97d8eb2b1289c4b4fd7b03b2814e16f9e0/LICENSE.md) 的下載位置集中在 [dependency lock](../ci/test-dependencies.lock.json)。三個類比樣本驗證既定數值，數位／IQ／FastFrame 樣本驗證明確拒絕；Peak Detect Min/Max 的雙端序、負垂直刻度、任意 header 時間刻度、pair 繪圖／CSV 與 metadata 單 LF 均用合成資料驗證。合成格式／損壞案例由測試建立，不包含私人波形。

先透過共同 CI 入口準備依賴並執行全部套件。單獨診斷可使用此 CMake 入口，設定 `ELECTRONICATE_WFM_FIXTURES` 為準備好的 `wfm` 目錄；configure 先驗證所有樣本雜湊。截圖使用 `ELECTRONICATE_TEST_EVIDENCE_DIR`，未指定則存測試工作目錄的 `test-evidence/`，不依賴工作站或日期路徑。

原始 WFM 不修改；CSV 測試寫暫存目錄。樣本檔與 SDK 由執行環境下載至 ignored build，均不提交到本 repository 或產品 ZIP。
