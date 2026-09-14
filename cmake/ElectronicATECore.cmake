# Shared production targets used by both the desktop app and offline tests.
# No target in this file may link Qt Widgets or the VISA implementation.
get_filename_component(ELECTRONICATE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)

add_library(ElectronicATEPersistence STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/service/app/xmlconfigstore.cpp")
target_include_directories(ElectronicATEPersistence PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/app")
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
target_link_libraries(ElectronicATEInstrumentCore PUBLIC Qt6::Core)

add_library(ElectronicATEStorage STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/storage/binaryfilestore.cpp")
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
target_link_libraries(ElectronicATEOscilloscopeLifecycle PUBLIC ElectronicATEData PRIVATE ElectronicATEInstrumentCore)

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
    "${ELECTRONICATE_SOURCE_ROOT}/service/operations/instrumentoperationrunner.cpp")
target_include_directories(ElectronicATEOperations PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/service/operations")
target_link_libraries(ElectronicATEOperations PUBLIC ElectronicATEData)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEOperations)
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
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/tableutils"
    PRIVATE "${ELECTRONICATE_SOURCE_ROOT}/ui/style")
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
    "${ELECTRONICATE_SOURCE_ROOT}/infrastructure/tableutils"
    PRIVATE "${ELECTRONICATE_SOURCE_ROOT}/service/validators")
target_link_libraries(ElectronicATEManualControl PUBLIC ElectronicATEModels ElectronicATEPersistence
    ElectronicATEOperations ElectronicATECapture ElectronicATETriggerBinding
    ElectronicATEOscilloscopeLifecycle ElectronicATEInstrumentCore
    PRIVATE ElectronicATEMessages Qt6::Concurrent)
list(APPEND ELECTRONICATE_CORE_TARGETS ElectronicATEManualControl)
foreach(core_target IN LISTS ELECTRONICATE_CORE_TARGETS)
    target_compile_features(${core_target} PUBLIC cxx_std_17)
endforeach()
