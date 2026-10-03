# Shared production targets used by both the desktop app and offline tests.
# No target in this file may link Qt Widgets or the VISA implementation.
get_filename_component(ELECTRONICATE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)

add_library(ElectronicATETaskPreparation INTERFACE)
target_include_directories(ElectronicATETaskPreparation INTERFACE "${ELECTRONICATE_SOURCE_ROOT}/service/taskpreparation")
target_link_libraries(ElectronicATETaskPreparation INTERFACE ElectronicATEData)

add_library(ElectronicATEPersistence STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/xml/xmlconfigstore.cpp")
target_include_directories(ElectronicATEPersistence PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/xml")
target_link_libraries(ElectronicATEPersistence PUBLIC Qt6::Core)

add_library(ElectronicATEMessages STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/message/messageservice.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/message/messageservice.h")
set_target_properties(ElectronicATEMessages PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEMessages PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/message")
target_link_libraries(ElectronicATEMessages PUBLIC Qt6::Core)

add_library(ElectronicATEInstrumentCore STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/instrumentwithcommbase.cpp")
target_include_directories(ElectronicATEInstrumentCore PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication")
target_link_libraries(ElectronicATEInstrumentCore PUBLIC ElectronicATEData Qt6::Core)

add_library(ElectronicATEStorage STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/storage/binaryfilestore.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/storage/ziparchive.cpp")
target_include_directories(ElectronicATEStorage PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/storage")
target_link_libraries(ElectronicATEStorage PUBLIC Qt6::Core)

add_library(ElectronicATECapture STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/capturesession.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/icapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/capturetaskrunner.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/waveformcaptureworkflow.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/pngcapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/csvcapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/allcsvcapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/allwfmcapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/wfmcapturecommand.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/capture/savedirpreference.cpp")
target_include_directories(ElectronicATECapture PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/capture")
target_link_libraries(ElectronicATECapture PUBLIC Qt6::Core
    PRIVATE ElectronicATEInstrumentCore ElectronicATEMessages ElectronicATEStorage)

add_library(ElectronicATEOscilloscopeLifecycle STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/oscilloscopebuilder.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/oscilloscopemanager.cpp")
target_include_directories(ElectronicATEOscilloscopeLifecycle PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope")
target_link_libraries(ElectronicATEOscilloscopeLifecycle PUBLIC ElectronicATEData PRIVATE ElectronicATEInstrumentCore Qt6::Concurrent)

add_library(ElectronicATETriggerBinding STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3/triggerbinding.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3/triggerbinding.h"
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/itriggercontroller.h")
set_target_properties(ElectronicATETriggerBinding PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATETriggerBinding PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3"
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope")
target_link_libraries(ElectronicATETriggerBinding PUBLIC ElectronicATEData)

set(ELECTRONICATE_CORE_TARGETS ElectronicATEModels ElectronicATEPersistence ElectronicATETriggerBinding ElectronicATEStorage
    ElectronicATEMessages ElectronicATEInstrumentCore ElectronicATECapture ElectronicATEOscilloscopeLifecycle)
add_library(ElectronicATEOperations STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/execution/instrumentoperationrunner.cpp")
target_include_directories(ElectronicATEOperations PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/operations"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/execution")
target_link_libraries(ElectronicATEOperations PUBLIC ElectronicATEData)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEOperations)
add_library(ElectronicATEManualScope STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/manualscopecontrol.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/manualscopecontrol.h")
set_target_properties(ElectronicATEManualScope PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEManualScope PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope")
target_link_libraries(ElectronicATEManualScope PUBLIC ElectronicATEInstrumentCore ElectronicATEData ElectronicATEOperations Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEManualScope)
add_library(ElectronicATEConsoleTransport STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/console/consoleexchange.cpp")
target_include_directories(ElectronicATEConsoleTransport PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/console")
target_link_libraries(ElectronicATEConsoleTransport PUBLIC Qt6::Core PRIVATE ElectronicATEInstrumentCore)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEConsoleTransport)
add_library(ElectronicATEConsole STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/console/consolesession.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/console/consolesession.h"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page4/page4viewmodel.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page4/page4viewmodel.h")
set_target_properties(ElectronicATEConsole PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEConsole PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page4")
target_link_libraries(ElectronicATEConsole PUBLIC ElectronicATEModels ElectronicATEPersistence
    PRIVATE ElectronicATEInstrumentCore ElectronicATEConsoleTransport)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEConsole)
add_library(ElectronicATEConditions STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page2/page2viewmodel.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page2/page2viewmodel.h")
set_target_properties(ElectronicATEConditions PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEConditions PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page2"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/presentation"
)
target_link_libraries(ElectronicATEConditions PUBLIC ElectronicATEModels ElectronicATEPersistence)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEConditions)
add_library(ElectronicATEInstrumentSettings STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page1/page1viewmodel.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page1/page1viewmodel.h")
set_target_properties(ElectronicATEInstrumentSettings PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEInstrumentSettings PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page1")
target_link_libraries(ElectronicATEInstrumentSettings PUBLIC ElectronicATEModels ElectronicATEPersistence)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEInstrumentSettings)
add_library(ElectronicATEScopeSession STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/scopesessioncoordinator.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope/scopesessioncoordinator.h")
set_target_properties(ElectronicATEScopeSession PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEScopeSession PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscope")
target_link_libraries(ElectronicATEScopeSession PUBLIC ElectronicATEOscilloscopeLifecycle ElectronicATECapture PRIVATE ElectronicATEInstrumentCore Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEScopeSession)
add_library(ElectronicATEManualControl STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3/page3viewmodel.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3/page3viewmodel.h"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/debounce/debounce.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/debounce/debounce.h"
    "${ELECTRONICATE_SOURCE_ROOT}/service/validators/instrumentconfigvalidator.cpp")
set_target_properties(ElectronicATEManualControl PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEManualControl PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page3"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/debounce"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/presentation"
    PRIVATE "${ELECTRONICATE_SOURCE_ROOT}/service/validators")
target_link_libraries(ElectronicATEManualControl PUBLIC ElectronicATEModels ElectronicATEPersistence
    ElectronicATEScopeSession ElectronicATEOperations ElectronicATECapture ElectronicATETriggerBinding
    ElectronicATEOscilloscopeLifecycle ElectronicATEInstrumentCore
    PRIVATE ElectronicATEMessages Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEManualControl)
foreach(core_target IN LISTS ELECTRONICATE_CORE_TARGETS)
    target_compile_features(${core_target} PUBLIC cxx_std_17)
endforeach()

add_library(ElectronicATEReport STATIC "${ELECTRONICATE_SOURCE_ROOT}/service/report/page5excelreport.cpp")
target_include_directories(ElectronicATEReport PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/report")
target_link_libraries(ElectronicATEReport PUBLIC ElectronicATEData Qt6::Core PRIVATE ElectronicATEStorage Qt6::Xml)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEReport)

add_library(ElectronicATETestRun STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/testrun/testrunservice.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/testrun/testrunactions.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/testrun/testrunmeasurement.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/testrun/testrunservice.h")
set_target_properties(ElectronicATETestRun PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATETestRun PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/testrun"
    PRIVATE "${ELECTRONICATE_SOURCE_ROOT}/service/oscilloscopestrategy")
target_link_libraries(ElectronicATETestRun PUBLIC ElectronicATETaskPreparation ElectronicATEData ElectronicATEInstrumentCore
    PRIVATE ElectronicATECapture ElectronicATEStorage)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATETestRun)

add_library(ElectronicATEReportWriter STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/execution/reportwriter.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/execution/reportwriter.h")
set_target_properties(ElectronicATEReportWriter PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEReportWriter PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/execution")
target_link_libraries(ElectronicATEReportWriter PUBLIC ElectronicATEReport PRIVATE Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEReportWriter)

add_library(ElectronicATEAutomatedTasks STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/worker/testrunsession.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/worker/testrunsession.h"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page5/page5viewmodel.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page5/page5viewmodel.h"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/worker/page5testworker.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/worker/page5testworker.h")
set_target_properties(ElectronicATEAutomatedTasks PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEAutomatedTasks PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/presentation"
    "${ELECTRONICATE_SOURCE_ROOT}/viewmodels/page5"
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/worker")
target_link_libraries(ElectronicATEAutomatedTasks PUBLIC ElectronicATEModels ElectronicATEPersistence
    ElectronicATETestRun PRIVATE ElectronicATEReportWriter ElectronicATEMessages)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEAutomatedTasks)

# Keep the headless production boundaries enforced as new dependencies are introduced.
foreach(core_target IN LISTS ELECTRONICATE_CORE_TARGETS)
    get_target_property(core_dependencies ${core_target} LINK_LIBRARIES)
    foreach(dependency IN LISTS core_dependencies)
        if(dependency MATCHES "Widgets|visa")
            message(FATAL_ERROR "${core_target} must not depend on UI or VISA: ${dependency}")
        endif()
    endforeach()
endforeach()

add_library(ElectronicATEConnectionTest STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/connection/connectiontestservice.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/connection/connectiontestservice.h")
set_target_properties(ElectronicATEConnectionTest PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEConnectionTest PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/connection")
target_link_libraries(ElectronicATEConnectionTest PUBLIC ElectronicATEInstrumentCore PRIVATE Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEConnectionTest)
