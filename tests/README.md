# Offline regression tests

Run from the repository root in PowerShell. These commands use the Qt/MinGW installation verified on this workstation; adjust the paths for another machine.

```powershell
$env:PATH = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.12.0\mingw_64\bin;' + $env:PATH
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S tests -B build/offline-tests -G 'MinGW Makefiles' '-DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64'
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/offline-tests --parallel 4
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/offline-tests --output-on-failure
```

Tests link the same core libraries as the desktop app. QtCore, QtXml and QtConcurrent support the core suites; only the offscreen Page2 Widget suite links QtWidgets. Fake instruments replace VISA and real devices. File tests use temporary directories. Capture tests supply file selectors and do not write application preferences.

| Suite | Regression covered |
| --- | --- |
| xml_persistence | XML snapshot/root validation, unknown sections and failed saves |
| taskstatus_queued_delivery | Worker status crosses a queued signal without UI types |
| page1_xml_error_handling | Malformed input terminates and preserves live config |
| capture_without_widgets | No scope, cancel, busy, image completion and save failures |
| oscilloscope_lifecycle | Selection, disconnect and clear ownership |
| complete_model_configuration | Five real Models round trip and page parser preflight |
| trigger_model_catalog | Controller/widget model family consistency |
| trigger_binding_lifetime | Replacement, deleted controllers and reconnect subscriptions |
| waveform_capture_workflow | CSV/WFM arguments, channel fallback, exception handling and partial multi-channel results |
| binary_file_storage | Binary replacement, write failures and existing file preservation |
| pending_configuration_updates | Coalescing while busy, snapshots and retry transitions |
| oscilloscope_connection_policy | Configuration filtering, resource precedence and per-device failures |
| capture_preparation_errors | Channel query, file selector and preference exceptions release leases and permit retry |
| capture_session_shutdown | Draining a real capture command, transferred scopes and acquisition/close races |
| instrument_operation_completion | Worker results/errors delivered on owner thread, suppressed after owner deletion |
| page4_console_results | Injected transport, write failures, nested commands, disconnect/replacement and deleted query owner |

Desktop compilation remains a separate check because these suites intentionally do not compile Views, device drivers or the application composition:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/Desktop_Qt_6_12_0_MinGW_64_bit_Debug --parallel 4
```

Passing these checks does not validate instrument protocol compatibility or GUI behavior. Capture lifetime tests include session closure during a blocked fake transfer. Pending-update tests cover state transitions; GUI scheduling and real driver/thread-affinity behavior still need integration validation.

The configuration_policies suite checks Page1 channel mapping/output bounds and Page2 load capability data using QtCore only, without concrete drivers or Widgets.

Additional suites: page2_model_updates verifies output resizing and atomic XML replacement; page2_viewmodel_without_widgets exercises production condition notifications, power and capability filtering; page1_viewmodel_without_widgets verifies model injection and configuration updates; page3_model_snapshot verifies failed-load isolation and legacy partial-document behavior. Page1/Page2 ViewModels are linked from production core targets without Widgets or VISA implementations.

instrument_operation_queue checks Page3's shared FIFO primitive: operation order, continuation after exceptions, reentrant submissions and discarding pending work on owner destruction. It does not test physical instruments or cross-page arbitration.

page3_viewmodel_without_drivers links ElectronicATEManualControl with injected execution functions to test queued operations, captured condition snapshots, Load retry, callback deletion, and adopted/abandoned scope connection lifetimes. page2_widget_snapshot uses offscreen QtWidgets to verify real table-to-snapshot synchronization and output resizing; it does not show the UI or use hardware.

`control_page_lock` 使用 offscreen QtWidgets 驗證 Page3／Page4／Page5 互斥停用、Page5 設定頁鎖定及解鎖彙整。Page3／Page4 測試另涵蓋輸出持續 ON、關閉失敗、延後連線、擷取取消及指令返回前不解鎖。

必要修正驗證：`output_off_readback` 檢查 OFF、延遲 ON→OFF、未知值及傳輸失敗；`page4_console_results` 檢查背景執行緒、阻塞 I/O 時 GUI 回應及取消；`xml_persistence` 檢查後段套用失敗回復、多頁通知時的一致狀態與巢狀載入拒絕。實機驗收另見 `docs/hardware-acceptance.md`。
