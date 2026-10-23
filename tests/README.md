# 離線測試

測試使用實際產品來源，搭配 fake 儀器／transport、loopback HTTP、固定官方樣本、合成資料與暫存檔。完整 gate 必須執行主套件、[獨立 OCR](waveformocr/README.md) 與 [WFM Viewer](wfmviewer/README.md) 三個入口。

## 執行方式

Windows x64 主機準備 Python 3.12.10、Git 與 PowerShell 7，從專案根目錄執行：

```powershell
python -B tests/ci/bootstrap_toolchain.py --root build/isolated-toolchain
if ($LASTEXITCODE -ne 0) { throw 'Toolchain failed' }
python -B tests/ci/run_ci.py --toolchain build/isolated-toolchain/toolchain.json --build-root build/isolated-ci
if ($LASTEXITCODE -ne 0) { throw 'CI failed' }
```

這會新安裝 Qt／MinGW／Python 相依與建置工具，下載固定官方 VISA 標頭及 WFM 樣本，執行基礎設施故障回歸，再完整執行 CI 測試。版本、環境及報告詳見 [CI 入口](ci/README.md)。新工具、依賴與 clean CI 入口拒絕覆寫非空目錄；失敗後保留證據並使用新的目錄。

| Profile | 範圍 |
| --- | --- |
| ci-offline | 主 836（含 VISA）＋OCR 2＋WFM 1＝839 個 CTest；全部必須執行。 |
| local-full | 預設完整模式，與 CI 相同 839 個 CTest；需要官方 VISA 標頭及固定 WFM 樣本。 |

WFM 的單一 CTest 內含 101 個 QtTest 結果（99 個功能資料列＋init／cleanup），按固定清單逐列驗證並另列報告，不與 CTest 數量相加。manifest v3 起兩種 profile 不再排除 VISA；歷史 833／834 的結果仍只代表當時範圍。

既有工具環境可先執行 `python -B tests/ci/prepare_test_dependencies.py --root build/test-dependencies`，再使用 [run_offline_tests.py](tools/run_offline_tests.py) 的 `--help`：指定 `--qt-prefix`、`--compiler`、`--cmake`、`--ctest`、`--ninja`、`--build-root`，以及 `--visa-include build/test-dependencies/visa/include --wfm-fixtures build/test-dependencies/wfm`。預設 local-full；兩種 profile 使用不同 build-root。VISA case 由測試提供 fake vi*，不連實機；完整 app 的 library／runtime 與實機驗證另計。

## 依功能找測試

| 目錄 | 範圍 |
| --- | --- |
| architecture/ | 分層、依賴、能力契約與合法／違規 fixtures |
| instrumentsetup/、testconditions/ | Page1／Page2 模型、XML、ViewModel 與 Widgets |
| manualcontrol/、commandconsole/ | Page3／Page4、互鎖、Modbus、假通訊 |
| testsequence/dialogs/、xml/、execution/ | Page5 Dialog、保存載入、排程／Stop／失敗清理 |
| testsequence/strategies/、results/ | 策略、Group、Trial、報表；compatibility 保留必要等價性比較 |
| oscilloscope/、hardware/ | 設定、擷取、真 Driver 配 fake I/O |
| persistence/、reporting/ | 跨頁 XML、檔案與 XLSX |
| waveformocr/、integration/ | OCR 合成資料、mock HTTP 與跨功能整合 |
| wfmviewer/ | 官方／合成 WFM、解析、CSV、縮放／平移、雙軸游標及離屏 Widget |
| support/、cmake/、tools/、ci/ | fixture、CTest policy、完整 runner、報告與故障回歸 |

## 完整 PASS 的條件

[固定 manifest](ctest-manifest.json) 鎖定名稱、命令、參數、labels、timeout、QtTest 方法及 WFM 資料列。runner 每輪 configure／build／列舉／執行並比對結果；只有 exit 0 與本輪 summary PASS 才代表完整通過。少測、多測、重複、skip、disabled、timeout、破損報告、依賴不符或來源中途改變都失敗，不能從當次列舉重建 manifest 消除缺項。

每輪 `runs/<unique-id>/` 保存原始 JUnit、功能 JUnit、Qt XML、commands、版本及來源 hash。domain labels 可重疊，功能列不可加總作總數；`-L`／`-R` 診斷與 `--show-only` 不代表完整 PASS。GUI stdout 空白也不能代替 Qt XML。

新增測試須一併審查 CMake、manifest、對應故障回歸與 .gitignore 的公開清單。修改 gate 時執行 `test_ctest_gate.py`、`test_ci_profile.py`、`test_complete_suites.py`；依賴／bootstrap 變更另跑 `test_test_dependencies.py`、`test_toolchain_bootstrap.py`。共同 CI 入口包含這些回歸，與產品案例分開計數。

## 本機補驗與公開範圍

desktop、歷史診斷、coverage、SDK、私人波形、產物及維護文件不隨此測試來源發布。部分工作站另有 local 診斷，並不加入固定 CTest 清單。公開來源包含測試、固定依賴來源／hash 與取得腳本；vendor 標頭及官方波形下載至 ignored build，不提交或放入產品 ZIP。沒有實機或模型精確度宣稱。

`waveform_ocr_test` 內的 liveOllama／liveBatch 僅供明確指定 OCR_FIXTURE 的本機診斷，CI 不選取；不要無參數執行該程式。測試字型使用 Windows 系統字型。產生的截圖透過 ELECTRONICATE_TEST_EVIDENCE_DIR 或 temporary 目錄保存，不需要公開任何真實波形。
