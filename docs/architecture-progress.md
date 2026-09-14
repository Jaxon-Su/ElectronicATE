# Architecture refactoring progress

## Working agreement

- Project: `C:\Qt_Project\ElectronicATE`; daily continuation at 23:00 Asia/Taipei.
- Keep per-page View/ViewModel where useful. Improve demonstrated responsibility and dependency problems, not directory names alone.
- Inspect applicable instructions and Git changes before editing; preserve user work and previous uncommitted refactors. Do not push or merge automatically.
- Work one bounded change at a time, verify it, then continue with the next while quota permits. The user explicitly authorized immediate sustained work, not waiting for 23:00. Record intentional behavior fixes. Do not operate physical instruments.

## 2026-09-13 — presentation connection composition

Evidence: AppService combined XML persistence with concrete Page1/2/3/5 ViewModel wiring. This made a shared service depend on presentation classes.

Moved all cross-page connection methods to `src/viewmodels/mainwindow/pageconnectioncoordinator.{h,cpp}`. MainWindowViewModel invokes the coordinator after constructing its pages. AppService now has no concrete page ViewModel dependencies; XML APIs and meta-type registration remain unchanged. Explicit page2config/QMap includes replace transitive includes.

Validation:

- Compared extracted method bodies against HEAD after normalizing only the class name and QObject::connect qualification: identical, including Page2 injection before Page5 signal wiring and receiver lifetime contexts.
- CMake configure and build succeeded with Qt 6.12.0 / MinGW 13.1 in `build/Desktop_Qt_6_12_0_MinGW_64_bit_Debug`; executable linked and runtime deployment completed.
- `git diff --check` passed (Git reports LF-to-CRLF normalization warnings).
- No GUI interaction or physical hardware tests performed. Deployment warns that dxcompiler.dll/dxil.dll are absent; D3D12 features were not validated.
- Changes remain uncommitted.

Build command (prepend `C:\Qt\Tools\mingw1310_64\bin` to PATH):

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build/Desktop_Qt_6_12_0_MinGW_64_bit_Debug
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/Desktop_Qt_6_12_0_MinGW_64_bit_Debug --parallel 4
```

## Candidates for subsequent inspection

1. Completed below: Page5ViewModel reads conditions through a provider interface, and Page5RightPanel now uses shared presentation formatting instead of Page2ViewModel.
2. Capture services have been separated from dialogs; see the capture entry below. Continue inspecting async resource ownership with offline fakes.
3. Completed below: XML persistence returns structured results and has offline tests. Page-level semantic validation/rollback remains a separate concern.

These are investigation candidates, not instructions to rewrite all three at once.

## 2026-09-13 — read-only test condition provider

User requested another immediate bounded refactor after the initial scheduled-task setup.

Evidence: Page5ViewModel depended on the complete Page2ViewModel solely for six const-reference condition getters. Introduced `src/data/itestconditionprovider.h`, implemented by Page2ViewModel. Page5ViewModel now accepts a borrowed const ITestConditionProvider through setConditionProvider; PageConnectionCoordinator supplies Page2 before establishing refresh connections.

All six getters still return live references, and a null provider still uses Page5Model fields. No new data copies, refresh signals, XML behavior, or worker behavior were introduced. Provider lifetime and UI-thread access requirements are documented; ownership remains with the existing composition.

Validation: Qt 6.12.0 / MinGW build and runtime deployment completed successfully. Compared the entire Page5ViewModel implementation against HEAD allowing only the include, provider identifier, and explanatory comment substitutions; identical otherwise. Reviewed override signatures and the unchanged coordinator connection order. git diff --check passed apart from line-ending normalization warnings. No GUI or physical hardware tests were performed; the existing optional D3D12 deployment warning remains. Both rounds remain uncommitted for review.

## 2026-09-13 — shared input-condition display formatting

Extracted Page2ViewModel::inputTitle into the header-only ConditionTextFormatter in `src/ui/style/conditiontextformatter.h`. Updated Page2ViewModel's title list, both Page2 view call sites, and Page5RightPanel. Removed the old static ViewModel method. Page5 views and ViewModel no longer reference Page2ViewModel.

Preserved the original function body exactly, including an empty title if any field is empty. Page5Conditions::inputTitle intentionally remains separate: its execution/signature mapping defaults an empty phase mode to 1phase and does not reject incomplete rows. Unifying these functions would change existing behavior.

Validation: extraction-body comparison passed; reviewed all migrated call sites and verified Page2's inputTitleChanged signal connection remains unchanged. Initial compile caught incomplete call-site migration, corrected before the final successful Qt 6.12.0 / MinGW build, link and deployment. git diff --check passed with line-ending warnings only. No GUI or hardware tests performed. Changes remain uncommitted alongside previous rounds.

## 2026-09-13 — XML operation results and failure handling

User authorized continued immediate work using available quota, without waiting for the nightly schedule.

AppService no longer invokes MessageService: save/load return a nodiscard XmlOperationResult. MainWindowViewModel translates and presents errors and updates lastSavePath only after success. Saving uses QSaveFile so serialization or commit failure does not truncate an existing configuration. Loading validates the complete in-memory XML snapshot and expected root before invoking page loaders; unknown top-level sections are skipped as units.

Added an independent QtCore-only CMake/CTest suite under tests/. xml_persistence tests Unicode/escaping round trips, null entries, missing files/directories, failed serialization preserving an existing file, malformed/trailing XML rejection before mutation, root validation, unknown-section isolation, and an empty root. Test and full application build passed. Preflight validates XML syntax, not all page semantics; semantic errors in individual loaders are not a cross-page rollback transaction.

Test commands (PATH also includes C:\Qt\6.12.0\mingw_64\bin):

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S tests -B build/offline-tests -G 'MinGW Makefiles' '-DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64'
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build/offline-tests --parallel 4
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build/offline-tests --output-on-failure
```

## 2026-09-13 — execution state independent of widgets

Moved TaskStatus from Page5RunPanel into src/data/taskstatus.h, updating Worker, ViewModel, run panel and signal consumers. Removed Page5RunPanel includes from Worker and ViewModel. Replaced the unused QComboBox include in Page1Config with its actual QStringList dependency.

Validation: a QtCore-only test sends all four task states from a worker thread to the main thread through queued signals. The test and full application build passed. Existing ignored-QFuture warnings and optional D3D12 deployment warning remain.

## 2026-09-13 — bounded XML parser loops and stateless persistence

Page1Model's four XML loops and CommunicationConfig's read loop now stop at parser end/error. Page1 validates its entry element and only publishes the temporary config after successful parsing. Offline tests exercise truncated XML and unexpected nested content in scalar fields, confirm no update signal or model mutation, and verify a real Page1 model round trip with legacy GPIB address and channel sync settings. This prevents error loops from hanging indefinitely.

Renamed the remaining persistence service to XmlConfigStore, a stateless class; removed the AppService singleton/QObject and moved TableKind registration to MainWindowViewModel composition. Tests were migrated to the new API. Three offline suites and the full application build passed after these changes.

## 2026-09-13 — capture presentation and resource ownership

All four capture commands now request a file through CaptureContext's injected selector. Page3 supplies the actual QFileDialog callback with a QPointer lifetime guard; an absent/destroyed View cancels the request. Completion messages use MessageService rather than constructing QMessageBox. Dialog labels and filters remain unchanged.

Replaced four local async flag guards with CaptureLease. The shared lease spans channel queries, file selection, and background capture, releasing on cancellation, exceptions, or completion. Busy operations return before channel queries, dialogs or preference changes. A missing flag is handled without dereferencing null. Offline fake-scope tests exercise no scope, absent selector, cancellation, all four busy paths with no channel queries, shared lease ownership and PNG async completion. No Qt Widgets, VISA, or real devices are used by this test target.

## 2026-09-13 — oscilloscope service boundary

OscilloscopeManager no longer accepts AbstractTriggerController. Page3ViewModel detaches its controller before clearing the manager, preserving the order of operations. Extracted hardware construction into oscilloscopemanager_factory.cpp so lifecycle operations can be linked/tested independently of factories. clear() reuses disconnectAll() rather than duplicating cleanup. Removed misleading unused oldMap scaffolding in Page3.

Offline lifecycle tests cover selection, missing model names, clearing connected/null/already-disconnected instruments and empty assignment. Added a six-section test suite including an entire five-Model configuration round trip (input/DC/load/dynamic/relay data, selections, command history and DUT settings) through the real XML store.

## Checkpoint before quota reset

- Full Qt 6.12.0 / MinGW application build, link and runtime deployment succeeded after all changes above.
- All six CTest suites passed: xml_persistence, taskstatus_queued_delivery, page1_xml_error_handling, capture_without_widgets, oscilloscope_lifecycle, complete_model_configuration.
- Existing ignored-QFuture compiler warnings and optional dxcompiler/dxil deployment warning remain. No GUI/manual instrument validation performed.
- No commits, pushes, or merges performed. Preserve the accumulated working tree on the next run.
- At the final quota check, five-hour usage was 95% and weekly usage 22%; finishing documentation and review before taking another change.

## Next investigation candidates

- Page3 trigger controllers still combine widget binding and device commands; inspect a bounded presentation/device boundary improvement.
- CMake's global include/link paths can be scoped to targets before introducing independently built layers.
- Page-level semantic parsing remains nontransactional across pages; syntax preflight and Page1 error handling are covered, but a full configuration rollback requires a separate design and tests.
- Capture command future ownership still generates compiler warnings. Assess structured operation lifetime before changing dispatch policy.

## 2026-09-13 — continuation after quota reset: enforce build boundaries

User reset quota and requested continued work; the fresh tool reading showed five-hour usage 2%, weekly 0%.

- Replaced global include/link directories with application-private target settings; VISA SDK paths are configurable cache paths with the original defaults.
- Created ElectronicATEData (QtCore interface target) and ElectronicATEModels (QtCore/QtXml static library), shared by the app and tests. Models no longer receive UI, VISA, driver or QtSerialPort include paths.
- Moved pure CommunicationConfig to src/data, replacing its unused QSerialPort include with QStringList. Extracted DutRowData into src/data/dutrowdata.h, removing full Page5Model includes from Page5ViewModel and CenterPanel.
- Added shared production library definitions in cmake/ElectronicATECore.cmake for persistence, messages, abstract instrument implementation, capture and oscilloscope lifecycle. Both app and tests link those same targets rather than compiling separate copies. Core sources are excluded from the executable source list.

Validation: all six offline suites passed against shared production libraries, and the full application built and deployed successfully. Verified model compile flags exclude Widgets/Gui/SerialPort, VISA, Views/ViewModels and hardware directory includes.

## 2026-09-13 — trigger family catalog

Centralized trigger controller/widget family mapping in TriggerModelCatalog. Controller factory, widget factory and Page3 ViewModel share it; Page3 no longer includes the factory or a concrete DPO7000 controller header. Corrected the mixed-case MSOSeries456 alias being rejected by isModelSupported while creation accepted it. Hardware factory support lists and channel counts remain unchanged, as they are a different capability boundary.

The new seventh offline suite covers all 20 supported names, normalized case/whitespace, legacy MSO4000 family mapping and unknown-name behavior. All seven suites passed. Full application build and deployment passed for this checkpoint.

## 2026-09-13 — trigger binding lifetime

Added a QObject-only ITriggerController port and a separately built TriggerBinding coordinator. Page3ViewModel now binds instruments and reads selected channels through this port. Binding owns the reconnect subscription, disconnects replaced controllers, and uses QPointer to tolerate controller destruction. AbstractTriggerController implements the port; device command implementations remain unchanged.

The eighth offline suite checks replacement, mismatched families, repeated attachment, null/destroyed controllers, instrument forwarding and reconnect subscription lifetime. All eight suites and the full application build passed.

## 2026-09-13 — isolated XML page validation

IXmlSerializable now requires validateXml to consume a section without changing live state. Each Page ViewModel delegates validation to an isolated instance of its existing Model parser. XmlConfigStore validates every known section before invoking any live loadXml method, in addition to the existing document syntax/root checks. Application and test adapters implement the same mandatory contract.

A regression test loads a valid Page1 followed by syntactically valid but unparseable Page2 scalar content. The load fails and both existing page models retain their original data. All eight offline suites passed. This is parser preflight, not a rollback transaction: it preserves existing permissive field parsing and does not promise rollback for failures during live callbacks or resource exhaustion.

Next candidates: structured asynchronous capture ownership, additional separation of trigger widget/device commands, and explicit domain validation of configuration values. Earlier build-boundary and trigger-binding investigation items are now implemented.
Latest verification: full Qt 6.12.0 MinGW application build/link/deployment succeeded after isolated XML validation; 8/8 CTest suites passed and git diff --check passed. No hardware execution or commits performed.

## 2026-09-13 — continued at user's request to use remaining quota

CSV and WFM commands now delegate to executeWaveformCapture, preserving UI text, filename extensions, channel fallback, scope temporary paths and transfer arguments. Added a ninth offline suite covering both formats, explicit/fallback channels, successful and failed transfers, exceptions, application-thread notifications, and enabled-channel partial results for AllCsv.

All capture workflows use runCaptureTask, which owns the submitted callable in the global QThreadPool and reports standard/non-standard exceptions on the application thread. Shared scope and CaptureLease captures retain their original lifetime. ElectronicATECapture no longer links QtConcurrent; tests require only QtCore/QtXml. This does not add cancellation or change Page3's existing two-second shutdown wait. Nine suites and the full application build passed at this checkpoint.

Extracted BinaryFileStore into an independent QtCore storage target. PNG screenshot and the DPO7000/DPO4000/MSO456 waveform host writers share it. QSaveFile checks the full write and commit before replacing the destination, and failures return a typed result. PNG no longer reports success after incomplete writes. Device transfer/SCPI methods remain unchanged.

The tenth suite covers raw binary data, Unicode path, shorter replacement, empty data, missing parent, protected directory contents, and on Windows failed replacement of a read-only existing file. Capture regression checks also cover empty image preserving the existing file and failed destination producing a warning without a success notification or leaked lease.

Removed the unused synchronous Page3ViewModel::createAllInstruments entry point and obsolete direct factory/file-system headers. Oscilloscope creation remains routed through the existing manager-based background path. Added tests/README.md with reproducible commands and coverage/limitations.

## Next bounded work, based on source review

- Page3ViewModel::applyPendingConfig changes its update state back to Idle before checking m_isConnecting. A configuration arriving during an existing connection can consequently return without scheduling a rebuild of the latest snapshot. Extract a testable pending-update coordinator first, cover rapid updates during an outstanding build, then integrate it with debounce and completion delivery. Do not silently change hardware timing without those tests.
- Page3 shutdown waits at most two seconds for capture, then disconnects instruments. Shared ownership prevents object deletion but does not serialize disconnect against a still-running transfer. Define an explicit operation/drain policy before modifying shutdown. The new runner deliberately does not claim to solve that policy.
- Synchronous channel queries/file selection still happen before background dispatch. The runner handles background exceptions; synchronous query failure handling remains a separate boundary.
- Model parser preflight retains legacy permissive numeric/JSON interpretation. Domain validation and complete rollback are separate changes.

No real instrument traffic, GUI launch, commit, push or merge was performed. Nightly work should preserve the accumulated uncommitted changes and continue from this checkpoint.

## Latest verified checkpoint — 2026-09-13, quota 95%

- Full Qt 6.12.0 MinGW application build, link and runtime deployment passed after the final Page3 cleanup.
- All 10 offline CTest suites passed after the storage integration; the added Windows read-only-file regression also passed after its addition.
- git diff --check passed. Capture compile flags include QtCore only (no QtConcurrent/Widgets/VISA); Model flags include QtCore/QtXml only.
- Remaining compiler warnings concern existing Page3 QtConcurrent futures. The optional dxcompiler/dxil deployment warning remains.
- Five-hour quota reading: 95% used; weekly: 15% used. Finishing the checkpoint rather than starting a refactor that cannot be validated before the limit.

## Continuation after quota window reset — pending configuration and connection policy

Fresh quota reading was 0% used. Replaced Page3's split Pending/Processing/Idle state plus m_isConnecting with PendingConfigUpdate. The queue owns snapshots, coalesces newer settings while a build is running and retains blocked requests. Page3 completes the queue on both successful and exceptional future completion and schedules retained work. QFutureWatcher owns the UI completion delivery; workers no longer inspect a QPointer to the ViewModel. This resolves the earlier lost-update investigation item. Capture-blocked settings now wait until they can be applied instead of repeatedly publishing them before a rebuild can start.

The eleventh suite covers blocked requests, rapid updates during a build, snapshot independence and retry after failure. Eleven suites and full desktop build passed for that checkpoint.

Extracted buildConnectedOscilloscopes from hardware factory composition. The service accepts an injected creator, filters enabled oscilloscope entries and isolates failures per device. Actual factory composition still retains communication until after the scope destructor. Resource selection uses InstrumentConfig::getResourceString(), preserving legacy address fallback while accepting valid structured communication settings and honoring their precedence. Driver commands and duplicate-model map behavior are unchanged.

The twelfth suite checks filtering, absent creator, structured-only resources, structured/legacy precedence, disconnected/throwing/null factories and continuation after failures. Twelve offline suites passed.

Verified after connection-policy extraction: full Qt 6.12.0 MinGW application build/link/deployment passed, 12/12 offline CTest suites passed, and git diff --check passed. No real instruments or GUI were started. Shutdown/capture coordination remains open; pending-update state transitions are now tested, while end-to-end GUI scheduling still needs integration coverage.

## Capture preparation exception boundary

ICaptureCommand::execute is now the final public entry point; individual formats implement executeImpl. The shared entry point catches standard and non-standard exceptions from synchronous connection checks, channel queries, file selectors and preference callbacks, reporting through MessageService. CaptureLease unwinds before reporting; background exceptions remain handled by runCaptureTask. Successful and canceled flows retain their existing behavior.

Added a thirteenth suite for channel-query errors, selector exceptions, preference callback errors and clean retry/cancellation across all four capture commands. All 13 suites passed. This resolves the earlier synchronous-query exception-boundary item; capture/shutdown drain policy remains unchanged and still needs a dedicated design.

Latest verification: full application build/link/deployment succeeded after the capture preparation boundary, all 13 offline suites passed, and git diff --check passed. Changes remain uncommitted; no GUI or real hardware was exercised.

## Capture session shutdown ownership

Replaced the exposed atomic busy flag with CaptureSession. CaptureLease acquires the session before any connection/channel query; closing prevents new leases and transfers instrument cleanup until the last lease owner exits. Page3 moves its scope map out with OscilloscopeManager::takeAll, detaches controller bindings and closes the session. Its destructor no longer waits two seconds and then disconnects an active transfer. Idle sessions still clean up immediately; active sessions clean up on the final releasing thread. This does not cancel an in-progress instrument operation or guarantee a deadline if a driver never returns.

The four existing capture test suites were migrated to the session API. The new fourteenth suite checks transferred scope ownership, active drain after owner destruction, idle/idempotent close and close/acquire races. All 14 suites passed at this checkpoint.

The session shutdown suite now executes an actual PngCaptureCommand against a semaphore-controlled fake scope, drops page-equivalent owners while transfer is blocked, and verifies disconnect occurs only after transfer ends. Fourteen suites and the full application build passed at the session checkpoint.

## Owner-bound instrument operation completion

Added InstrumentOperationResult as a data contract and ElectronicATEOperations as a QtCore-only target. runInstrumentOperation owns a QPromise/QFutureWatcher completion channel; work receives owned snapshots only and the callback belongs to the requesting QObject. Page3 Input/Relay/Load/Dynamic Load all use this shared runner. Removed cross-thread QPointer checks and ViewModel signal emissions from workers; standard/non-standard exceptions become failed results delivered on the owner thread. Load busy-state release now also occurs after non-standard exceptions. Input errors with nonempty result messages are surfaced instead of silently discarded. Hardware execution order and arguments remain unchanged.

The fifteenth suite checks normal/failed results, exception conversion, callback thread affinity and deletion of an owner while a worker is blocked. All fifteen suites passed. The Page3 completion adapter additionally checks lifetime after synchronous message/signal delivery to tolerate reentrant owner deletion. Removed the now-unused Page3 configuration mutex.

Oscilloscope cleanup now isolates both status-query and disconnect exceptions per scope, so one failing device does not skip the rest of the map. The lifecycle regression covers both failure sites followed by a healthy scope. This remains best-effort cleanup and does not add a timeout to driver calls.

The overlapping application builds caused one temporary truncated-object read. Both processes ended; a subsequent independent --clean-first application build completed successfully. Do not start another build in the same output directory until its previous process has exited.

## Page4 console boundary and command result correctness

Page4ViewModel now has an injected communication creator and is built as ElectronicATEConsole without concrete transport factories. The default application constructor lives in page4viewmodel_factory.cpp; existing callers retain their API. Offline tests can instantiate the production ViewModel with a fake ICommunication.

executeCommand returns an explicit success/response/error result. Failed or partial non-query writes now record failure instead of unconditional OK; query framing and successful response/history behavior are preserved. An owner-aware command guard rejects reentrant sends during the existing processEvents polling loop. Connection revisions invalidate an in-flight query after disconnect/replacement, including allocator address reuse. Lifetime checks stop query completion after owner deletion.

The sixteenth suite covers default command newline framing, query responses, negative/partial writes, failed connection, disconnect and replacement while polling, reentrant send rejection, retry after failure and owner deletion. All 16 suites passed. Page4 still uses synchronous polling and the legacy timeout accounting; a dedicated asynchronous transport owner is a future improvement, not implemented by this change.

Removed the unused Page4 timeout timer/slot, unused last-command field and duplicate uncalled address parser. Timeout behavior still belongs to the existing polling path; address creation belongs to the injected factory. All 16 offline suites passed after this cleanup.

## Next investigation candidates after this run

- Move Page4 query execution to a dedicated transport owner/thread so processEvents polling can be removed without violating SerialPort/QObject affinity. Preserve the new connection-revision and failure-history regressions.
- Add explicit handling for exceptions during Page4 transport creation/open/read, and replace elapsed-loop-count timeout accounting with a deadline that accounts for blocking driver reads. Do not claim current timeout is a hard wall-clock deadline.
- Configuration preflight still uses legacy permissive parsing. Domain validation and complete transactional rollback remain separate work.
- Validate real-device shutdown and Qt-affine communication implementations; session tests prove lease/disconnect ordering with fakes, not protocol compatibility or an upper shutdown time bound.

Preserve all accumulated uncommitted changes. No real hardware, GUI execution, commits, pushes or merges were performed.

Latest verified checkpoint: five-hour quota 98% used, weekly 31% used. Full application build/link/deployment succeeded after final Page4 cleanup; all 16 offline suites passed; git diff --check passed. Ending at a fully validated checkpoint before the quota limit. Optional dxcompiler/dxil deployment warning remains.

## Continuation after quota recovery — console exchange service

The new quota window started at 0% used. Page4 timeout accounting now measures actual elapsed time, including driver reads. Negative read results fail immediately. The regression uses a delayed fake read to prove a 100 ms query does not perform ten 60 ms reads; a blocking driver call itself still cannot be interrupted by this deadline.

Extracted exchangeConsoleCommand into ElectronicATEConsoleTransport, a QtCore-only service without a ViewModel or concrete transport factory. It owns command framing, write/read validation, response collection and conversion of standard/non-standard transport exceptions to typed results. Page4 maps those results to translated UI messages and history. The injected continue-waiting callback retains the existing owner/revision invalidation behavior without making the service depend on UI event processing.

Existing console tests additionally exercise delayed reads, immediate read errors, write/read exceptions and retry after exceptions. This extraction does not make Page4 asynchronous or change its legacy acceptance of nonempty partial responses without a newline. Connection creation/open/close exception handling remains a separate next step.

Latest verification: all 16 offline suites passed; full application build/link/deployment succeeded after console exchange extraction; git diff --check passed. Console transport compile flags contain QtCore and abstract communication includes, without Widgets or VISA. No GUI or real hardware was executed.

## Quota reservation for branch, push and GitHub release

User requested further refactoring while reserving quota for branching, pushing code and a GitHub release. Start-of-turn quota was 38% used. Scope was limited to Page4 close cleanup: move transport ownership out before invoking close, catch close exceptions, and always release the transport even during ViewModel destruction. Regression verifies explicit disconnect and destruction with a throwing fake transport. All 16 offline suites passed.

Repository remote is https://github.com/Jaxon-Su/ElectronicATE.git and current branch is main. Accumulated changes remain uncommitted; this turn did not create a branch, push or publish a release. Reserve remaining quota for that workflow rather than continuing open-ended refactoring.

## Page4 connection initialization failures

Transport creation, open and failure-detail exceptions now become an Error connection state. Failed transports are closed and released before failure signals, allowing a retry without retaining stale ownership. Replacing an existing transport no longer requires an isOpen query. Added standard and non-standard factory/open exception regressions and successful retry checks. All 16 offline suites passed. Five-hour quota checkpoint: 58% used, 42% remaining reserved for branch/push/release work. No hardware or GUI execution.

## Page1 and Page2 configuration policies

User superseded quota reservation: continue local refactoring without reserving GitHub publication quota; no push or release requested for this run.

Page1 now shares OutputIndexPolicy for output options and bounds across initial display, model changes and output-count changes. ChannelNumberPolicy owns numeric catalog parsing, sorting/deduplication and positional channel assignment, preserving the existing -1 sentinel. These policies are QtCore-only and exercised without Widgets.

LoadCapabilityCatalog now owns the existing Chroma model-family lists and manual/dynamic range capabilities. Page2 queries it directly instead of DCLoadFactory; the factory reuses the same catalog for driver selection and delegates its compatibility query methods. Existing model matching and option order are preserved.

Added configuration_policies suite covering sparse channels, missing slots, output bounds, supported families and unknown modes/models. All 17 offline suites passed. Page2 model field encapsulation, Page1 uniqueness rules still embedded in Widgets, and Page3 operation coordination remain follow-up work. No GUI or real hardware execution, commits, pushes or releases.

## Page1–Page3 continuation without GitHub publication

Page2's seven condition collections are now private with const read access and explicit replacement methods. Output metadata and relay-row resizing moved from the ViewModel into the Model, preserving empty-string load defaults, relay off defaults and dynamic timing values. Numeric/domain validation remains permissive; this is not a complete validation layer.

Page2 XML loading now validates its element and parses into temporary data before committing. Failed loads retain all existing collections and emit no configLoaded signal. The ViewModel skips its refresh on parse failure. ElectronicATEConditions compiles the actual Page2 ViewModel with Models/Persistence and no Widgets or concrete hardware factory dependencies. Regression checks power calculation, resize notifications, capability filtering and failed-load silence.

Page1 ViewModel construction no longer opens Instrument.xml. MainWindowViewModel owns the explicit catalog path/load in the same initialization order and logs failures; Page1 hydrates from the supplied Model. ElectronicATEInstrumentSettings shares the production ViewModel with offline tests, covering supplied configuration, UI updates and malformed XML.

Page3 Model groups persisted fields into a State snapshot. XML parsing runs on a candidate with a copy of current state and commits only on success, preserving legacy omitted-field behavior. Page3 ViewModel does not restore UI after parse errors. Regression compares serialized state after malformed input and checks valid partial updates.

All 21 offline suites passed after these changes. No GUI/hardware execution, commits, pushes or releases. Remaining priorities: Page3 per-instrument operation coordination, Page1 uniqueness logic outside Widgets, and stronger domain validation across configuration snapshots.
Final checkpoint: full desktop application build/link/runtime deployment succeeded; all 21 suites passed, followed by a passing targeted Page2 wrong-root regression. Core compile definitions for Page1/Page2 contain QtCore/QtXml only. git diff --check passed. Optional dxcompiler/dxil deployment warning remains. Work is local and uncommitted.

## Page3 serialized instrument operations

InstrumentOperationQueue is an owner-thread FIFO using the existing worker/completion runner. Page3 Input, Relay and accepted Load/Dynamic operations now share this queue, preserving captured configuration and arrival order. Load/Dynamic retains its existing busy rejection policy. Pending entries are discarded on queue destruction; active work finishes with captured inputs and no stale completion. Reentrant submissions append behind accepted work. This serializes all these Page3 categories conservatively, not by physical resource address. Scope capture/reconnect and operations from other pages are not covered; blocking drivers are not cancelled, and queued operations can be delayed by earlier work.

Added instrument_operation_queue regression for FIFO ordering, worker exception recovery, reentrant submission and destruction with active/pending work. All 22 offline suites passed. No hardware execution or GitHub publication.

## Page1 commit ordering and Page2 complete condition updates

Page1 UI changes now commit output counts and instruments before notifications; nested UI updates suppress stale outer publication. Output-index claim/availability policies are shared with Widgets and regression tested. Page3's operation queue now closes at destructor entry, discarding pending/new work and suppressing active completion while cleanup proceeds.

TestConditionSnapshot is a copyable metatype containing all seven Page2 data groups. Page2 publishes complete snapshots to Page3; the coordinator replaces six separate data subscriptions. Page2's UI reads all tables before one setConditions call, preserving compatibility notifications after the Model commit. Its dynamic-header-only path starts from existing conditions to retain unrelated tables. Reentrant snapshot updates suppress stale granular notifications.

## Page3 production ViewModel test boundary

Page3Operations injects copied execution callbacks and scope creation. The default constructor lives in page3viewmodel_factory.cpp and wires existing hardware services. Action enums moved to data/instrumentactions.h. ElectronicATEManualControl compiles the production ViewModel, validators and debounce without Widgets or concrete drivers. Tests exercise FIFO capture semantics, condition delivery, Load failure/retry and deletion during config/busy notifications.

PendingScopes owns newly built scope connections until a live completion adopts them; if the ViewModel disappears during connection, the unadopted result disconnects its scopes. Active driver calls are still not cancellable and affinity requirements still need real-device validation.

Final additions: Page3 internally holds TestConditionSnapshot rather than six parallel fields. Tests cover adopted and abandoned scope results, retry after connection exceptions and deletion from operation completion. Page2 XML notification paths now stop if the ViewModel is deleted or superseded during callbacks.

The offscreen Page2 Widget regression compiles the actual View/style/delegate sources and verifies one snapshot per UI sync, table round trips, dynamic timing and output resize without erasing other tables. It uses QApplication with QT_QPA_PLATFORM=offscreen, never shows a window, and does not construct any hardware service. All 24 suites passed. Earlier full-app compile found a missed dynamic-header sync call; it was corrected to preserve the existing snapshot and subsequently passed the Widget test and full build. Tests now require QtWidgets only for this dedicated UI suite; core targets remain independent.

Remaining work: cross-page/physical-resource operation arbitration, stronger domain validation, complete multi-page transactional apply and Page4 asynchronous transport ownership. Keep all changes local; no commit, push or release was performed.
Verified final checkpoint: all 24 suites passed after the last production change; full desktop build/link/runtime deployment succeeded. ManualControl compile includes QtCore/QtXml/QtConcurrent and abstract instruments, without Widgets or VISA SDK headers. git diff --check passed. Five-hour quota was 95% used at final validation. Current branch remains main and all accumulated work is local/uncommitted. Optional dxcompiler/dxil deployment warning remains.

## Page1 XML notification reentrancy

Page1 catalog/XML load notifications now use the same revision/lifetime boundary as UI commits. A nested load supersedes the outer notification sequence, and deletion from dataChanged or output-count callbacks stops further access. Removed the unguarded notifyViewModelReady helper. Regression verifies nested XML loads publish only the newer configuration and deletion during XML notification is safe. All 24 suites passed. Work remains local with no GitHub publication.

## Resource selection alignment before cross-page arbitration

Inventory found InstrumentCreator still read the legacy address field for AC Source, Load and Relay, unlike scope creation. These paths now resolve getResourceString once per instrument and use it consistently for validity checks, transport creation, connection reuse keys, DC Load sync addresses, relay diagnostics and slave-ID parsing. Relay execution uses the same resolved-address filter. Regression covers legacy fallback, structured override, structured-only configuration and Modbus slave preservation. All 24 suites passed. This aligns the resource-selection prerequisite only: cross-page locking and alias canonicalization are not implemented yet. No hardware execution or GitHub publication.

## GitHub release preparation

User authorized a GitHub release. Created refactor/architecture-alpha3 from main at 02f3e97; planned tag v0.2.0-alpha.3 follows the repository prerelease convention. All 24 suites passed. A fresh Windows x64 Release build and deployable archive are being prepared. No additional broad refactoring is included in this publication step; remaining architectural limitations are documented in the release notes.
