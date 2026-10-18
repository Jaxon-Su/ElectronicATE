# Software tests CI

[Workflow](../../.github/workflows/offline-tests.yml) 顯示為 **Software tests**，在 push 到 main、以 main 為目標的 PR 或手動觸發時執行。它執行 835 個不連實機的 CTest 案例，其中 WFM 套件再核對 101 個 QtTest 結果；不自動合併或發布 Release。測試來源按 [功能](../README.md) 分類。

| 工作 | 用途 |
| --- | --- |
| `software-tests` | 安裝固定環境、執行測試與故障回歸，驗證並保存本輪證據。 |
| `ci-check` | 等待測試工作，要求成功、完整證據及相同 SHA，提供最終檢查結果。 |

workflow 檔名保留以延續執行紀錄；內部 `ci-offline` 模式與 `OFFLINE_*` 環境契約保持。`offline` 表示不依賴實體儀器，可在本機或雲端執行。歷史紀錄中的 `offline-tests`／`offline-ci` 分別為兩個工作的舊名稱。

## 固定環境與安裝

Windows x64、Python 3.12.10、Qt 6.12.0、MinGW 13.1.0、CMake／CTest 3.30.5、Ninja 套件 1.13.2；runner 使用 windows-2022，實際 image 版本保存於 artifact。

[bootstrap_toolchain.py](bootstrap_toolchain.py) 在空目錄建立 venv，安裝 [固定 Python 相依](requirements-win64.txt)，再安裝固定 aqt commit `076e1659807d0b362a3ed684d54c2e9c775eb9c7`（3.3.1.dev166），支援 Qt 6.11+ Windows 架構子目錄。Qt／MinGW 從官方來源及其鏡像取得，保留 HTTPS 與 SHA256 校驗；[run_aqt.py](run_aqt.py) 使用 Windows 系統憑證信任庫。

Qt 的 [實際套件 metadata](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_mingw/Updates.xml) 指定 MinGW 13.1；Python 使用有 [Windows x64 套件](https://github.com/actions/python-versions/releases/tag/3.12.10-14343898437) 的版本。版本變更須重新確認目標 OS／架構的套件及實際建置，不能只修改成功標記。

成功才產生 toolchain.json；失敗留下 bootstrap FAIL、命令與日誌。[run_ci.py](run_ci.py) 驗證工具在該根目錄內，清除舊 Qt／Python／CMake／compiler 環境，執行全新 build。需要的 host 前置是上述 Python、Git、PowerShell 7 與 Windows 系統字型；本機操作見 [共同入口](../README.md)。

## 執行與結果

1. 記錄 tested SHA、PR head／base SHA 與 runner image。
2. 安裝固定工具鏈。
3. 執行 bootstrap／profile／CTest gate／完整套件／依賴故障回歸，準備固定官方測試依賴，再 configure／build／執行主 832、OCR 2、WFM 1。
4. [verify_ci.py](verify_ci.py) 核對固定 manifest、原始 JUnit、本輪來源 hash 與 SHA，才輸出成功證據。
5. 無論成功或失敗，都保存可取得的 logs／JUnit／工具來源／版本；不打包 SDK、工具本體或私人資料。
6. [finish-gate.ps1](finish-gate.ps1) 要求上游 success、正確 profile 與 SHA；failure、cancelled、skipped、缺少證據均失敗。

PR／Actions 的 Checks 顯示結果；step log 說明錯誤，job summary 列功能組，artifact 保存原始證據。功能標籤交疊，不把分組數量加總。workflow 不自行設定 branch protection／required checks；Merge／Release 仍由人決定。

## 固定測試依賴與驗證界線

[prepare_test_dependencies.py](prepare_test_dependencies.py) 依 [固定來源與 SHA256](test-dependencies.lock.json) 從 NI 官方 HTTPS 下載 VISA Headers 26.5 套件，讀取其中固定 CAB，再以 Windows `expand.exe` 解出未修改的 visa.h／visatype.h／vpptype.h；只在當輪 build 準備編譯標頭，無系統安裝或驅動操作。逐層檢查套件、CAB 與標頭 hash，遵循 [NI 授權資訊](https://www.ni.com/en/about-ni/legal/license-agreements.html)。不公開 SDK，也不以 dummy header 代替。

WFM 下載 Tektronix 固定 commit 的六個官方樣本及 Apache-2.0 LICENSE，逐檔檢查 hash；說明見 [WFM tests](../wfmviewer/README.md)。下載與準備失敗保留 FAIL，不沿用半成品。Artifact 只保存依賴 metadata／結果／截圖，沒有 vendor 套件或標頭。

兩個 profile 都執行 main/review_fix_visa_partial；case 使用 fake vi*，不需真 runtime 或儀器。ci-check 重新核對三套 JUnit、全部 QtTest 報告、WFM 內部資料列、官方依賴 hash 與來源 SHA。缺失／跳過／取消不算成功。離線 PASS 不代表完整 app、實機、真 Ollama 或模型準確率；Windows 字型／runner image 差異仍以每次 hosted 結果判斷。
