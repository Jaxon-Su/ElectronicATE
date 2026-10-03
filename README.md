# ElectronicATE

電子 ATE 自動化測試系統

ElectronicATE 是一套用於電源供應器、電子負載、示波器與繼電器控制的 Windows 桌面測試平台。專案以 Qt Widgets 實作 GUI，透過 MVVM 分層管理 UI、測試條件、儀器設定、XML 保存／載入與硬體控制流程。

**操作流程：設定儀器 → 建立條件 → 手動控制或自動測試 → 保存結果。**

## 文件導覽

| 文件 | 內容 |
| --- | --- |
| [操作與畫面](USER_GUIDE.md) | Page1–6 功能、任務與報表、負載同步、本機 Ollama 圖片辨識與 Excel 匯出、畫面截圖。 |
| [支援儀器與限制](SUPPORTED_INSTRUMENTS.md) | 各類儀器型號、尚未支援的模式及實機驗證限制。 |
| [整體架構](ARCHITECTURE.md) | 跨頁接線與互鎖、完整架構圖及分層職責。 |
| [Page3：手動控制流程](PAGE3_FLOW.md) | 手動控制、示波器連線、擷取與借用。 |
| [Page5：自動測試流程](PAGE5_FLOW.md) | Session 管理背景執行、整批預檢、量測、Stop、清理與報表。 |
| [Page5：量測策略](MEASUREMENT_STRATEGIES.md) | Static／Dynamic、暫態與 Short 流程、Group 逐負載結果、Auto Period 與 Brown Out。 |

使用者可先讀「操作與畫面」與「支援儀器與限制」；閱讀程式可先讀「Page3：手動控制流程」與「Page5：自動測試流程」，再對照「整體架構」。

## 版本與下載

版本與發布檔案請見 [GitHub Releases](https://github.com/Jaxon-Su/ElectronicATE/releases)。

## 使用前確認

本版供後續實機驗證。儀器型號與操作條件見 [支援儀器與限制](SUPPORTED_INSTRUMENTS.md#目前限制)。

## 授權

ElectronicATE 秉持分享與交流精神公開原始碼。本軟體依「現狀」提供，不提供任何明示或默示之擔保。使用者應自行驗證其適用性，並採取必要的設備保護與安全措施。在適用法律允許的最大範圍內，作者及貢獻者不對因使用或無法使用本軟體所造成的損失承擔責任，包括設備損壞、資料遺失及營運損失。詳細授權條款以 LICENSE 為準；Qt 與其他第三方元件仍依各自授權條款使用。

## 作者

Jaxon Su

GitHub: [@Jaxon-Su](https://github.com/Jaxon-Su)
