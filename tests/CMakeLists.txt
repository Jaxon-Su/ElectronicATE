cmake_minimum_required(VERSION 3.16)
project(ElectronicATEOfflineTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Xml Concurrent Widgets SerialPort Network Test)
enable_testing()
set(OFFLINE_SUITE main)
include(cmake/offline-policy.cmake)
add_executable(waveform_ocr_test waveformocr/waveform_ocr_test.cpp ../src/views/waveformocr/waveformocrpage.cpp)
target_link_libraries(waveform_ocr_test PRIVATE ElectronicATEWaveformOcr ElectronicATEStorage Qt6::Widgets Qt6::Test Qt6::Xml)
foreach(scenario units channels textAmbiguity tables scan excel editAndReview client clientErrors cancel batch view)
    offline_add_qttest(waveform_ocr_${scenario} waveform_ocr_test ${scenario})
    set_tests_properties(waveform_ocr_${scenario} PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
add_executable(comm_scan_test instrumentsetup/comm_scan_test.cpp
    ../src/infrastructure/discovery/commscanprocess.cpp
    ../src/viewmodels/instrumentsetup/commscanviewmodel.cpp
    ../src/views/instrumentsetup/dialogs/commscandialog.cpp)
target_link_libraries(comm_scan_test PRIVATE Qt6::Core Qt6::Widgets)
add_test(NAME communication_scan COMMAND comm_scan_test)
set_tests_properties(communication_scan PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_subdirectory(../src/data "${CMAKE_CURRENT_BINARY_DIR}/data")
add_executable(measurement_data_test oscilloscope/measurement_data_test.cpp)
target_link_libraries(measurement_data_test PRIVATE ElectronicATEData)
add_test(NAME measurement_data_contract COMMAND measurement_data_test)

add_executable(transient_results_test testsequence/results/transient_results_test.cpp)
target_link_libraries(transient_results_test PRIVATE ElectronicATETestRun ElectronicATEReport ElectronicATEStorage Qt6::Xml)
add_test(NAME transient_results COMMAND transient_results_test)
add_executable(group_results_test testsequence/results/group_results_test.cpp ../src/views/testsequence/testsequencerunpanel.cpp)
target_include_directories(group_results_test PRIVATE ../src/views/testsequence ../src/viewmodels/presentation)
target_link_libraries(group_results_test PRIVATE ElectronicATEReport ElectronicATEStorage Qt6::Xml Qt6::Widgets)
add_test(NAME group_results COMMAND group_results_test)
set_tests_properties(group_results PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_subdirectory(../src/models "${CMAKE_CURRENT_BINARY_DIR}/models")
include(../cmake/ElectronicATECore.cmake)
include(../cmake/ElectronicATEDrivers.cmake)

add_executable(mso_capture_failure_test oscilloscope/mso_capture_failure_test.cpp)
target_link_libraries(mso_capture_failure_test PRIVATE ElectronicATEDrivers)
foreach(scenario success prepare_write gating_write save_write save_rejected opc read_throw)
    add_test(NAME mso_capture_${scenario} COMMAND mso_capture_failure_test ${scenario})
    set_tests_properties(mso_capture_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

add_executable(mso_waveform_sequence_test oscilloscope/mso_waveform_sequence_test.cpp)
target_link_libraries(mso_waveform_sequence_test PRIVATE ElectronicATEDrivers ElectronicATECapture ElectronicATEStorage)
foreach(scenario parser repeat alternate long_directory testSequence test_sequence_failure ldir_rejected ldir_read_failure
    clear_failure bad_size missing malformed short_file file_read_failure restore_failure delete_failure opc_read_failure
    file_terminator invalid_file_suffix repeat_lf alternate_lf test_sequence_lf)
    add_test(NAME mso_waveform_${scenario} COMMAND mso_waveform_sequence_test ${scenario})
    set_tests_properties(mso_waveform_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

add_executable(xml_persistence_test persistence/xml_persistence_test.cpp)
target_link_libraries(xml_persistence_test PRIVATE ElectronicATEPersistence)
add_test(NAME xml_persistence COMMAND xml_persistence_test)

add_executable(taskstatus_test testsequence/execution/taskstatus_test.cpp)
target_link_libraries(taskstatus_test PRIVATE ElectronicATEData)
add_test(NAME taskstatus_queued_delivery COMMAND taskstatus_test)
set_tests_properties(taskstatus_queued_delivery PROPERTIES TIMEOUT 10)

add_executable(instrument_setup_xml_test instrumentsetup/instrument_setup_xml_test.cpp)
target_link_libraries(instrument_setup_xml_test PRIVATE ElectronicATEModels)
add_test(NAME instrument_setup_xml_error_handling COMMAND instrument_setup_xml_test)
set_tests_properties(instrument_setup_xml_error_handling PROPERTIES TIMEOUT 10)

add_executable(capture_test oscilloscope/capture_test.cpp)
target_link_libraries(capture_test PRIVATE ElectronicATECapture ElectronicATEInstrumentCore ElectronicATEMessages)
add_test(NAME capture_without_widgets COMMAND capture_test)
set_tests_properties(capture_without_widgets PROPERTIES TIMEOUT 10)

add_executable(oscilloscope_manager_test oscilloscope/oscilloscope_manager_test.cpp)
target_link_libraries(oscilloscope_manager_test PRIVATE ElectronicATEOscilloscopeLifecycle ElectronicATEInstrumentCore)
add_test(NAME oscilloscope_lifecycle COMMAND oscilloscope_manager_test)

add_executable(model_config_test instrumentsetup/model_config_test.cpp)
target_link_libraries(model_config_test PRIVATE ElectronicATEModels ElectronicATEPersistence)
add_test(NAME complete_model_configuration COMMAND model_config_test)
set_tests_properties(complete_model_configuration PROPERTIES TIMEOUT 10)

add_executable(trigger_catalog_test oscilloscope/trigger_catalog_test.cpp)
target_link_libraries(trigger_catalog_test PRIVATE ElectronicATEData)
add_test(NAME trigger_model_catalog COMMAND trigger_catalog_test)

add_executable(trigger_binding_test oscilloscope/trigger_binding_test.cpp)
target_link_libraries(trigger_binding_test PRIVATE ElectronicATETriggerBinding ElectronicATEInstrumentCore)
add_test(NAME trigger_binding_lifetime COMMAND trigger_binding_test)

add_executable(waveform_capture_test oscilloscope/waveform_capture_test.cpp)
target_link_libraries(waveform_capture_test PRIVATE ElectronicATECapture ElectronicATEInstrumentCore ElectronicATEMessages)
add_test(NAME waveform_capture_workflow COMMAND waveform_capture_test)
set_tests_properties(waveform_capture_workflow PROPERTIES TIMEOUT 15)

add_executable(binary_file_test reporting/binary_file_test.cpp)
target_link_libraries(binary_file_test PRIVATE ElectronicATEStorage)
add_test(NAME binary_file_storage COMMAND binary_file_test)

add_executable(pending_config_test oscilloscope/pending_config_test.cpp)
target_link_libraries(pending_config_test PRIVATE ElectronicATEOscilloscopeLifecycle)
add_test(NAME pending_configuration_updates COMMAND pending_config_test)

add_executable(oscilloscope_builder_test oscilloscope/oscilloscope_builder_test.cpp)
target_link_libraries(oscilloscope_builder_test PRIVATE ElectronicATEOscilloscopeLifecycle ElectronicATEInstrumentCore)
add_test(NAME oscilloscope_connection_policy COMMAND oscilloscope_builder_test)

add_executable(capture_preparation_test oscilloscope/capture_preparation_test.cpp)
target_link_libraries(capture_preparation_test PRIVATE ElectronicATECapture ElectronicATEInstrumentCore ElectronicATEMessages)
add_test(NAME capture_preparation_errors COMMAND capture_preparation_test)

add_executable(capture_session_test oscilloscope/capture_session_test.cpp)
target_link_libraries(capture_session_test PRIVATE ElectronicATECapture ElectronicATEOscilloscopeLifecycle ElectronicATEInstrumentCore)
add_test(NAME capture_session_shutdown COMMAND capture_session_test)
set_tests_properties(capture_session_shutdown PROPERTIES TIMEOUT 10)

add_executable(instrument_operation_test hardware/instrument_operation_test.cpp)
target_link_libraries(instrument_operation_test PRIVATE ElectronicATEOperations)
add_test(NAME instrument_operation_completion COMMAND instrument_operation_test)
set_tests_properties(instrument_operation_completion PROPERTIES TIMEOUT 10)

add_executable(command_console_test commandconsole/command_console_test.cpp)
target_link_libraries(command_console_test PRIVATE ElectronicATEConsole ElectronicATEInstrumentCore)
add_test(NAME command_console_results COMMAND command_console_test)
set_tests_properties(command_console_results PROPERTIES TIMEOUT 10)

add_executable(modbus_console_test commandconsole/modbus_console_test.cpp ../src/views/commandconsole/commandconsolepage.cpp ../src/views/commandconsole/commandconsolepage.h)
target_link_libraries(modbus_console_test PRIVATE ElectronicATEConsole ElectronicATEConsoleTransport ElectronicATEInstrumentCore Qt6::Widgets)
foreach(scenario codec exchange viewmodel widget)
    add_test(NAME command_console_modbus_${scenario} COMMAND modbus_console_test ${scenario})
    set_tests_properties(command_console_modbus_${scenario} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
endforeach()

add_executable(configuration_policy_test oscilloscope/configuration_policy_test.cpp)
target_link_libraries(configuration_policy_test PRIVATE ElectronicATETaskPreparation)
add_test(NAME configuration_policies COMMAND configuration_policy_test)

add_executable(test_conditions_model_test testconditions/test_conditions_model_test.cpp)
target_link_libraries(test_conditions_model_test PRIVATE ElectronicATEModels)
add_test(NAME test_conditions_model_updates COMMAND test_conditions_model_test)
set_tests_properties(test_conditions_model_updates PROPERTIES TIMEOUT 10)

add_executable(test_conditions_viewmodel_test testconditions/test_conditions_viewmodel_test.cpp)
target_link_libraries(test_conditions_viewmodel_test PRIVATE ElectronicATEConditions)
add_test(NAME test_conditions_viewmodel_without_widgets COMMAND test_conditions_viewmodel_test)
set_tests_properties(test_conditions_viewmodel_without_widgets PROPERTIES TIMEOUT 10)

add_executable(instrument_setup_viewmodel_test instrumentsetup/instrument_setup_viewmodel_test.cpp)
target_link_libraries(instrument_setup_viewmodel_test PRIVATE ElectronicATEInstrumentSettings)
add_test(NAME instrument_setup_viewmodel_without_widgets COMMAND instrument_setup_viewmodel_test)
set_tests_properties(instrument_setup_viewmodel_without_widgets PROPERTIES TIMEOUT 10)

add_executable(manual_control_model_test manualcontrol/manual_control_model_test.cpp)
target_link_libraries(manual_control_model_test PRIVATE ElectronicATEModels)
add_test(NAME manual_control_model_snapshot COMMAND manual_control_model_test)
set_tests_properties(manual_control_model_snapshot PROPERTIES TIMEOUT 10)

add_executable(instrument_queue_test hardware/instrument_queue_test.cpp)
target_link_libraries(instrument_queue_test PRIVATE ElectronicATEOperations)
add_test(NAME instrument_operation_queue COMMAND instrument_queue_test)
set_tests_properties(instrument_operation_queue PROPERTIES TIMEOUT 15)

add_executable(manual_control_viewmodel_test manualcontrol/manual_control_viewmodel_test.cpp)
target_link_libraries(manual_control_viewmodel_test PRIVATE ElectronicATEManualControl)
add_test(NAME manual_control_viewmodel_without_drivers COMMAND manual_control_viewmodel_test)
set_tests_properties(manual_control_viewmodel_without_drivers PROPERTIES TIMEOUT 20)

add_executable(test_conditions_widget_test testconditions/test_conditions_widget_test.cpp
    ../src/views/testconditions/testconditionspage.cpp ../src/views/testconditions/testconditionspage.h ../src/views/testconditions/conditionrowsview.cpp
    ../src/ui/style/styleutils.cpp)
target_include_directories(test_conditions_widget_test PRIVATE ../src/views/testconditions ../src/ui/style ../src/ui/delegate)
target_link_libraries(test_conditions_widget_test PRIVATE ElectronicATEConditions ElectronicATEManualControl Qt6::Widgets)
add_test(NAME test_conditions_widget_snapshot COMMAND test_conditions_widget_test)
set_tests_properties(test_conditions_widget_snapshot PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(control_page_lock_test integration/control_page_lock_test.cpp)
target_include_directories(control_page_lock_test PRIVATE ../src/ui)
target_link_libraries(control_page_lock_test PRIVATE Qt6::Widgets)
add_test(NAME control_page_lock COMMAND control_page_lock_test)
set_tests_properties(control_page_lock PROPERTIES TIMEOUT 10 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(output_readback_test hardware/output_readback_test.cpp)
target_link_libraries(output_readback_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME output_off_readback COMMAND output_readback_test)
set_tests_properties(output_off_readback PROPERTIES TIMEOUT 10)

add_executable(dcsource_test hardware/dcsource_test.cpp
    ../src/hardware/instrument/dcsource/chroma62000.cpp
    ../src/hardware/instrument/dcsource/dcsourcefactory.cpp
    ../src/hardware/instrument/dcsource/dcsourcespec/chroma62000spec.cpp)
target_include_directories(dcsource_test PRIVATE ../src/hardware/instrument/dcsource)
target_link_libraries(dcsource_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME dcsource_scpi COMMAND dcsource_test)
set_tests_properties(dcsource_scpi PROPERTIES TIMEOUT 10)

add_executable(dcinput_operation_test hardware/dcinput_operation_test.cpp
    ../src/service/instrumentexecutor/dcinputoperation.cpp
    ../src/hardware/instrument/dcsource/chroma62000.cpp
    ../src/hardware/instrument/dcsource/dcsourcefactory.cpp
    ../src/hardware/instrument/dcsource/dcsourcespec/chroma62000spec.cpp)
target_include_directories(dcinput_operation_test PRIVATE
    ../src/service/instrumentexecutor ../src/hardware/instrument/dcsource)
target_link_libraries(dcinput_operation_test PRIVATE ElectronicATEInstrumentCore ElectronicATEData)
add_test(NAME dcinput_operation COMMAND dcinput_operation_test)

add_executable(manual_control_dc_test manualcontrol/manual_control_dc_test.cpp)
add_executable(manual_control_dc_widget_test manualcontrol/manual_control_dc_widget_test.cpp
    ../src/views/manualcontrol/manualcontrolpage.cpp ../src/views/manualcontrol/manualcontrolpage.h ../src/ui/style/styleutils.cpp)
target_include_directories(manual_control_dc_widget_test PRIVATE ../src/views/manualcontrol ../src/ui/widget ../src/ui/style)
target_link_libraries(manual_control_dc_widget_test PRIVATE ElectronicATEManualControl Qt6::Widgets)
add_test(NAME manual_control_dc_widget_controls COMMAND manual_control_dc_widget_test)
set_tests_properties(manual_control_dc_widget_controls PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_executable(manual_control_capture_widget_test manualcontrol/manual_control_capture_widget_test.cpp
    ../src/views/manualcontrol/manualcontrolpage.cpp ../src/views/manualcontrol/manualcontrolpage.h ../src/ui/style/styleutils.cpp)
target_include_directories(manual_control_capture_widget_test PRIVATE ../src/views/manualcontrol ../src/ui/widget ../src/ui/style)
target_link_libraries(manual_control_capture_widget_test PRIVATE ElectronicATEManualControl ElectronicATEMessages Qt6::Widgets)
add_test(NAME manual_control_capture_widget_controls COMMAND manual_control_capture_widget_test)
set_tests_properties(manual_control_capture_widget_controls PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(manual_control_scope_isolation_test manualcontrol/manual_control_scope_isolation_test.cpp
    ../src/views/manualcontrol/manualcontrolpage.cpp ../src/views/manualcontrol/manualcontrolpage.h ../src/ui/style/styleutils.cpp)
target_include_directories(manual_control_scope_isolation_test PRIVATE ../src/views/manualcontrol ../src/ui ../src/ui/widget ../src/ui/style)
target_link_libraries(manual_control_scope_isolation_test PRIVATE ElectronicATEManualControl ElectronicATEMessages Qt6::Widgets)
add_test(NAME manual_control_scope_connection_isolation COMMAND manual_control_scope_isolation_test)
set_tests_properties(manual_control_scope_connection_isolation PROPERTIES TIMEOUT 25 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
target_link_libraries(manual_control_dc_test PRIVATE ElectronicATEManualControl)
add_test(NAME manual_control_dc_controls COMMAND manual_control_dc_test)
set_tests_properties(manual_control_dc_controls PROPERTIES TIMEOUT 15)

add_executable(configuration_xml_test persistence/configuration_xml_test.cpp)
target_link_libraries(configuration_xml_test PRIVATE ElectronicATEInstrumentSettings ElectronicATEConditions ElectronicATEManualControl ElectronicATEPersistence)
add_test(NAME configuration_xml_roundtrip COMMAND configuration_xml_test)


add_executable(normal_trigger_test oscilloscope/normal_trigger_test.cpp
    ../src/service/oscilloscopestrategy/steadystatesearchstrategy.cpp
    ../src/service/oscilloscopestrategy/steadysearch.cpp
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(normal_trigger_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(normal_trigger_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME normal_trigger_acquisition COMMAND normal_trigger_test)
set_tests_properties(normal_trigger_acquisition PROPERTIES TIMEOUT 10)

add_executable(oscilloscope_configuration_test oscilloscope/oscilloscope_configuration_test.cpp
    ../src/service/oscilloscope/oscilloscopeconfiguration.cpp
    ../src/service/oscilloscope/oscilloscopeautoperiod.cpp
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(oscilloscope_configuration_test PRIVATE ../src/service/oscilloscope)
target_link_libraries(oscilloscope_configuration_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME oscilloscope_configuration COMMAND oscilloscope_configuration_test)

add_executable(auto_period_dialog_test testsequence/dialogs/auto_period_dialog_test.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/createwriteoscilloscopedialog.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/msoseries456writedialog.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/oscwritedialogbase.cpp
    ../src/models/testsequence/testsequencemodel.cpp)
target_include_directories(auto_period_dialog_test PRIVATE
    ../src/views/testsequence/dialogs/oscilloscopedialogs ../src/models/testsequence)
target_link_libraries(auto_period_dialog_test PRIVATE ElectronicATEInstrumentCore ElectronicATEData Qt6::Widgets)
add_test(NAME auto_period_dialog_xml COMMAND auto_period_dialog_test)

add_executable(mso_horizontal_test oscilloscope/mso_horizontal_test.cpp
    ../src/service/oscilloscope/oscilloscopeconfiguration.cpp)
target_include_directories(mso_horizontal_test PRIVATE ../src/service/oscilloscope)
target_link_libraries(mso_horizontal_test PRIVATE ElectronicATEDrivers)
add_test(NAME mso_horizontal COMMAND mso_horizontal_test)
set_tests_properties(mso_horizontal PROPERTIES TIMEOUT 30)

add_executable(mso_horizontal_dialog_test oscilloscope/mso_horizontal_dialog_test.cpp
    ../src/service/oscilloscope/oscilloscopeconfiguration.cpp
    ../resource.qrc
    ../src/views/testsequence/dialogs/oscilloscopedialogs/msoseries456writedialog.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/oscwritedialogbase.cpp
    ../src/models/testsequence/testsequencemodel.cpp)
target_include_directories(mso_horizontal_dialog_test PRIVATE
    ../src/service/oscilloscope
    ../src/views/testsequence/dialogs/oscilloscopedialogs ../src/models/testsequence)
target_link_libraries(mso_horizontal_dialog_test PRIVATE ElectronicATEInstrumentCore ElectronicATEData Qt6::Widgets Qt6::Test)
set_target_properties(mso_horizontal_dialog_test PROPERTIES AUTORCC ON)
add_test(NAME mso_horizontal_dialog COMMAND mso_horizontal_dialog_test)
set_tests_properties(mso_horizontal_dialog PROPERTIES TIMEOUT 30)
foreach(scenario scope_tokens scope_ignore scope_channels scope_scale)
    add_test(NAME dialog_${scenario} COMMAND mso_horizontal_dialog_test ${scenario})
    set_tests_properties(dialog_${scenario} PROPERTIES TIMEOUT 30)
endforeach()

set(transient_sources
    ../src/service/oscilloscopestrategy/descentstrategy.cpp
    ../src/service/oscilloscopestrategy/transientstrategy.cpp
    ../src/service/oscilloscopestrategy/transientexecution.cpp
    ../src/service/oscilloscopestrategy/transientsearch.cpp
    ../src/service/oscilloscopestrategy/turnonevent.cpp
    ../src/service/oscilloscopestrategy/turnoffevent.cpp
    ../src/service/oscilloscopestrategy/shortthenturnonevent.cpp
    ../src/service/oscilloscopestrategy/turnonthenshortevent.cpp)

add_executable(transient_strategy_test testsequence/strategies/transient_strategy_test.cpp
    ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopestrategyfactory.cpp
    ../src/service/oscilloscopestrategy/steadystatesearchstrategy.cpp
    ../src/service/oscilloscopestrategy/steadysearch.cpp
    ../src/service/oscilloscopestrategy/turnonstrategy.cpp
    ../src/service/oscilloscopestrategy/turnoffstrategy.cpp
    ../src/service/oscilloscopestrategy/turnonthenshortstrategy.cpp
    ../src/service/oscilloscopestrategy/shortthenturnonstrategy.cpp
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(transient_strategy_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(transient_strategy_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME transient_strategies COMMAND transient_strategy_test)
set_tests_properties(transient_strategies PROPERTIES TIMEOUT 60)

get_target_property(descent_strategy_sources transient_strategy_test SOURCES)
list(REMOVE_ITEM descent_strategy_sources testsequence/strategies/transient_strategy_test.cpp)
add_executable(descent_test testsequence/strategies/descent_test.cpp ${descent_strategy_sources} ../resource.qrc)
set_target_properties(descent_test PROPERTIES AUTORCC ON)
target_include_directories(descent_test PRIVATE ../src/service/oscilloscopestrategy
    ../src/models/testsequence ../src/views/testsequence/dialogs/relaydialogs)
target_link_libraries(descent_test PRIVATE ElectronicATETestRun ElectronicATEModels ElectronicATEDrivers ElectronicATEReport Qt6::Widgets Qt6::Xml)
foreach(scenario success na missing invalid disabled channel reject arm early miss comm late clip clip_query read
    off discharge cancel stopped deadline capture ui precision driver later_read service preflight service_off)
    add_test(NAME descent_${scenario} COMMAND descent_test ${scenario})
    set_tests_properties(descent_${scenario} PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(channel_state_test oscilloscope/channel_state_test.cpp ${descent_strategy_sources})
target_include_directories(channel_state_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(channel_state_test PRIVATE ElectronicATEDrivers)
foreach(kind RANGE 0 6)
    add_test(NAME channel_state_${kind} COMMAND channel_state_test ${kind})
    set_tests_properties(channel_state_${kind} PROPERTIES TIMEOUT 30)
endforeach()

add_executable(page5_preflight_test testsequence/execution/page5_preflight_test.cpp ../src/service/oscilloscope/oscilloscopeconfiguration.cpp
    ../src/service/instrumentexecutor/measurementpreflight.cpp
    ../src/service/instrumentexecutor/measurementpreflight6310.cpp
    ../src/service/inputparameters/parameterparser.cpp)
target_include_directories(page5_preflight_test PRIVATE ../src/service/oscilloscope ../src/models/testsequence
    ../src/service/inputparameters)
target_link_libraries(page5_preflight_test PRIVATE ElectronicATETestRun ElectronicATEModels ElectronicATEDrivers ElectronicATEReport Qt6::Xml)
add_test(NAME channel_state_startup COMMAND channel_state_test startup)
set_tests_properties(channel_state_startup PROPERTIES TIMEOUT 30)
foreach(scenario trigger write log)
    add_test(NAME page5_preflight_${scenario} COMMAND page5_preflight_test ${scenario})
    set_tests_properties(page5_preflight_${scenario} PROPERTIES TIMEOUT 30)
endforeach()

add_executable(transient_dialog_test testsequence/dialogs/transient_dialog_test.cpp
    ../resource.qrc
    ../src/views/testsequence/dialogs/delaydialogs/delaydialog.cpp
    ../src/views/testsequence/dialogs/stressdialogs/staticdialog.cpp
    ../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/turnonoffdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/shorttestdialog.cpp
    ../src/models/testsequence/testsequencemodel.cpp)
set_target_properties(transient_dialog_test PROPERTIES AUTORCC ON)
target_include_directories(transient_dialog_test PRIVATE ../src/views/testsequence/dialogs/turnonoffdialogs
    ../src/views/testsequence/dialogs/relaydialogs ../src/models/testsequence)
target_link_libraries(transient_dialog_test PRIVATE ElectronicATEData Qt6::Widgets)
add_test(NAME transient_dialog_xml COMMAND transient_dialog_test)

add_executable(strategy_matrix_test testsequence/strategies/strategy_matrix_test.cpp
    ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopestrategyfactory.cpp
    ../src/service/oscilloscopestrategy/steadystatesearchstrategy.cpp
    ../src/service/oscilloscopestrategy/steadysearch.cpp
    ../src/service/oscilloscopestrategy/turnonstrategy.cpp
    ../src/service/oscilloscopestrategy/turnoffstrategy.cpp
    ../src/service/oscilloscopestrategy/turnonthenshortstrategy.cpp
    ../src/service/oscilloscopestrategy/shortthenturnonstrategy.cpp
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(strategy_matrix_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(strategy_matrix_test PRIVATE ElectronicATEInstrumentCore)

get_target_property(trial_strategy_sources strategy_matrix_test SOURCES)
list(REMOVE_ITEM trial_strategy_sources testsequence/strategies/strategy_matrix_test.cpp)
add_executable(trial_results_test testsequence/results/trial_results_test.cpp ${trial_strategy_sources}
    ../src/views/testsequence/testsequenceresultdetails.cpp ../src/views/testsequence/testsequencerunpanel.cpp
    ../src/viewmodels/testsequence/testsequenceresultsmodel.cpp)
target_include_directories(trial_results_test PRIVATE ../src/service/oscilloscopestrategy ../src/service/testrun)
target_link_libraries(trial_results_test PRIVATE ElectronicATEMeasurementStrategies ElectronicATEReport ElectronicATEStorage Qt6::Widgets Qt6::Test)
offline_add_qttest(trial_results trial_results_test)
set_tests_properties(trial_results PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set(transient_cases scale_recover scale_limit scale_clamp scale_reject bipolar positive negative no_event intermittent secondary_na dpo_command
    clipping nan_event bad_source disabled_source arm_failure comm_error write_failure
    power_failure relay_failure load_failure unknown_exception cleanup_failure cleanup_exception
    cleanup_unknown cancel_before cancel_wait cancel_read iteration_limit phase_limit invalid_config null_scope)
foreach(kind RANGE 0 3)
    foreach(scenario IN LISTS transient_cases)
        add_test(NAME strategy_${kind}_${scenario} COMMAND strategy_matrix_test transient ${scenario} ${kind})
        set_tests_properties(strategy_${kind}_${scenario} PROPERTIES TIMEOUT 20)
    endforeach()
endforeach()
set(acquisition_cases state_0 state_STOP state_OFF state_header state_1 state_RUN state_ON state_invalid
    late_completion late_running null_wait zero_wait cancel_wait cancel_query
    reference_ok reference_clipped reference_empty reference_range reference_sentinel reference_nan
    reference_timeout reference_comm reference_arm reference_write reference_cleanup reference_throw_cleanup
    reference_throw reference_cancel reference_null reference_zero)
foreach(scenario IN LISTS acquisition_cases)
    add_test(NAME acquisition_${scenario} COMMAND strategy_matrix_test acquisition ${scenario})
    set_tests_properties(acquisition_${scenario} PROPERTIES TIMEOUT 10)
endforeach()
option(STRATEGY_COVERAGE "Instrument local strategy unit tests with gcov" OFF)
if(STRATEGY_COVERAGE)
    foreach(target normal_trigger_test transient_strategy_test strategy_matrix_test)
        target_compile_options(${target} PRIVATE -O0 -g --coverage)
        target_link_options(${target} PRIVATE --coverage)
    endforeach()
endif()

foreach(scenario scale_recover scale_clamp bipolar flat null cancel_before bad_source disabled_source write_edge write_level
    arm_normal comm_normal nan_normal cancel_normal_wait cancel_normal_read cancel_after_level finish_stop_failure iteration_limit)
    add_test(NAME normal_matrix_${scenario} COMMAND strategy_matrix_test normal ${scenario})
    set_tests_properties(normal_matrix_${scenario} PROPERTIES TIMEOUT 10)
endforeach()

add_executable(test_sequence_worker_failure_test testsequence/execution/test_sequence_worker_failure_test.cpp
    ../src/infrastructure/worker/testrunservicefactory.cpp)
target_include_directories(test_sequence_worker_failure_test PRIVATE ../src/infrastructure/worker
    ../src/service/instrumentexecutor ../src/service/oscilloscopestrategy ../src/service/oscilloscope
    ../src/hardware/instrumentcreator ../src/service/conditions)
target_link_libraries(test_sequence_worker_failure_test PRIVATE ElectronicATEAutomatedTasks ElectronicATETestRun ElectronicATEData ElectronicATEInstrumentCore ElectronicATECapture ElectronicATEStorage Qt6::SerialPort)
foreach(dynamic 0 1)
    foreach(scenario auto_trigger_failure auto_trigger_cancel auto_success auto_failure auto_cancel auto_off auto_ignore write_success write_failure write_adjusted write_missing_readback dpo7000 dpo4000 dpo7000_batch dpo4000_batch dpo7000_independent dpo4000_independent success no_scope bad_input bad_load bad_delay missing_strategy
        input_failure input_exception input_unknown load_failure load_exception
        measure_failure measure_exception measure_unknown cancel_before cancel_input cancel_load
        cancel_settle cancel_measure cancel_result cleanup_scope cleanup_input cleanup_load
        cleanup_exception cleanup_unknown retry dc_success sequence_stop transient_cleanup bad_retry large_retry relay_success relay_stop relay_failure relay_cleanup)
        add_test(NAME worker_${dynamic}_${scenario} COMMAND test_sequence_worker_failure_test ${scenario} ${dynamic})
        set_tests_properties(worker_${dynamic}_${scenario} PROPERTIES TIMEOUT 10)
    endforeach()
endforeach()

add_executable(test_sequence_xml_review_test testsequence/xml/test_sequence_xml_review_test.cpp ../src/models/testsequence/testsequencemodel.cpp)
target_include_directories(test_sequence_xml_review_test PRIVATE ../src/models/testsequence)
target_link_libraries(test_sequence_xml_review_test PRIVATE ElectronicATEData Qt6::Xml)
add_test(NAME test_sequence_xml_review COMMAND test_sequence_xml_review_test)
add_test(NAME test_sequence_xml_drafts COMMAND test_sequence_xml_review_test drafts)

include(cmake/review-fixes.cmake)


add_executable(capture_gap_probe testsequence/strategies/capture_gap_probe.cpp
 ${transient_sources}
 ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(capture_gap_probe PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(capture_gap_probe PRIVATE ElectronicATEInstrumentCore)
add_test(NAME capture_gap_regression COMMAND capture_gap_probe)
set_tests_properties(capture_gap_regression PROPERTIES TIMEOUT 30)

add_executable(short_exit_test testsequence/strategies/short_exit_test.cpp
    ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(short_exit_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(short_exit_test PRIVATE ElectronicATEInstrumentCore)
foreach(scenario success off_false off_throw off_unknown discharge_on discharge_off short_off capture_throw
                 stop_capture stop_discharge stop_wait no_trigger arm_failure read_throw)
    add_test(NAME short_exit_${scenario} COMMAND short_exit_test ${scenario})
    set_tests_properties(short_exit_${scenario} PROPERTIES TIMEOUT 20)
endforeach()

add_test(NAME capture_task_removed COMMAND test_sequence_worker_failure_test capture_removed PNG)

add_executable(excel_report_test reporting/excel_report_test.cpp)
target_link_libraries(excel_report_test PRIVATE ElectronicATEReport)
add_test(NAME excel_report COMMAND excel_report_test)

add_executable(vertical_scale_test oscilloscope/vertical_scale_test.cpp ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(vertical_scale_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(vertical_scale_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME vertical_scale_calculation COMMAND vertical_scale_test)

add_test(NAME review_async_poll COMMAND review_fixes_test async_poll)

add_executable(worst_capture_test oscilloscope/worst_capture_test.cpp ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(worst_capture_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(worst_capture_test PRIVATE ElectronicATECapture ElectronicATEInstrumentCore ElectronicATEStorage)
add_test(NAME strategy_waveform_capture COMMAND worst_capture_test)

file(GLOB_RECURSE ARCHITECTURE_DIALOG_SOURCES CONFIGURE_DEPENDS ../src/views/testsequence/dialogs/*.cpp)
add_executable(architecture_test_sequence_test architecture/architecture_test_sequence_test.cpp
    ../src/infrastructure/worker/testrunservicefactory.cpp
    ../src/views/testsequence/testsequencecenterpanel.cpp ../src/views/testsequence/testsequencerunpanel.cpp ../src/views/testsequence/testsequenceresultdetails.cpp
    ../src/views/testsequence/testsequencepage.cpp ../src/views/testsequence/testsequenceleftpanel.cpp ../src/views/testsequence/testsequencerightpanel.cpp
    ../src/ui/widget/sidepanel.cpp ../src/ui/style/testsequencetreestyle.cpp
    ../src/ui/table/tableutils.cpp ${ARCHITECTURE_DIALOG_SOURCES})
target_include_directories(architecture_test_sequence_test PRIVATE
    ../src/ui/widget
    ../src/viewmodels/testsequence ../src/views/testsequence ../src/ui/table ../src/ui/style ../src/viewmodels/presentation
    ../src/infrastructure/worker ../src/service/instrumentexecutor ../src/service/oscilloscope
    ../src/service/oscilloscopestrategy ../src/hardware/instrumentcreator ../src/service/conditions
    ../src/views/testsequence/dialogs/oscilloscopedialogs ../src/views/testsequence/dialogs/turnonoffdialogs
    ../src/views/testsequence/dialogs/stressdialogs ../src/views/testsequence/dialogs/relaydialogs
    ../src/views/testsequence/dialogs/delaydialogs)
target_link_libraries(architecture_test_sequence_test PRIVATE ElectronicATEAutomatedTasks ElectronicATEReportWriter ElectronicATETestRun ElectronicATEModels ElectronicATEInstrumentCore
    ElectronicATECapture ElectronicATEMessages ElectronicATEReport ElectronicATEStorage ElectronicATEPersistence Qt6::Widgets Qt6::Concurrent Qt6::SerialPort Qt6::Network)
add_test(NAME architecture_testSequence COMMAND architecture_test_sequence_test)
add_test(NAME test_sequence_dialog_transactions COMMAND architecture_test_sequence_test dialog_transactions)
set_tests_properties(test_sequence_dialog_transactions PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(architecture_testSequence PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME test_sequence_xml_rollback_identity COMMAND architecture_test_sequence_test xml_rollback)
set_tests_properties(test_sequence_xml_rollback_identity PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(session_direct_service_test hardware/session_direct_service_test.cpp)
target_link_libraries(session_direct_service_test PRIVATE ElectronicATEAutomatedTasks ElectronicATEReportWriter)
foreach(boundary before during report)
    add_test(NAME session_direct_service_${boundary} COMMAND session_direct_service_test ${boundary})
    set_tests_properties(session_direct_service_${boundary} PROPERTIES TIMEOUT 10)
endforeach()

add_executable(refactor_contract_test architecture/refactor_contract_test.cpp)
target_include_directories(refactor_contract_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(refactor_contract_test PRIVATE ElectronicATETestRun)
foreach(scenario transport settings bracket lifecycle missing_dependency cancel cleanup_failure)
    add_test(NAME refactor_${scenario} COMMAND refactor_contract_test ${scenario})
    set_tests_properties(refactor_${scenario} PROPERTIES TIMEOUT 10)
endforeach()

add_executable(report_writer_test reporting/report_writer_test.cpp)
target_link_libraries(report_writer_test PRIVATE ElectronicATEReportWriter)
add_test(NAME report_writer_lifecycle COMMAND report_writer_test)
set_tests_properties(report_writer_lifecycle PROPERTIES TIMEOUT 10)

add_executable(resource_cleanup_test hardware/resource_cleanup_test.cpp ../src/hardware/instrumentcreator/resourcecleaner.cpp)
target_include_directories(resource_cleanup_test PRIVATE ../src/hardware/instrumentcreator
    ../src/hardware/instrument/acsource ../src/hardware/instrument/dcload ../src/hardware/instrument/relay)
target_link_libraries(resource_cleanup_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME resource_cleanup_exception COMMAND resource_cleanup_test)

add_executable(creator_ownership_test hardware/creator_ownership_test.cpp ../src/hardware/instrumentcreator/resourcecleaner.cpp ../src/hardware/instrumentcreator/instrumentcreator.cpp)
target_include_directories(creator_ownership_test PRIVATE ../src/hardware/instrumentcreator ../src/hardware/instrumentcreator
    ../src/hardware/instrument/acsource ../src/hardware/instrument/dcload ../src/hardware/instrument/relay)
target_link_libraries(creator_ownership_test PRIVATE ElectronicATEData ElectronicATEInstrumentCore Qt6::SerialPort)
add_test(NAME creator_ownership_failure COMMAND creator_ownership_test)

add_executable(scope_operation_test oscilloscope/scope_operation_test.cpp)
target_include_directories(scope_operation_test PRIVATE ../src/infrastructure/execution)
target_link_libraries(scope_operation_test PRIVATE ElectronicATEInstrumentCore Qt6::Concurrent)
add_test(NAME scope_operation_lifecycle COMMAND scope_operation_test)
set_tests_properties(scope_operation_lifecycle PROPERTIES TIMEOUT 10)

add_test(NAME capture_options_contract COMMAND refactor_contract_test capture_options)

add_test(NAME typed_task_preflight COMMAND refactor_contract_test typed_preflight)

add_executable(binary_transfer_budget_test oscilloscope/binary_transfer_budget_test.cpp)
target_link_libraries(binary_transfer_budget_test PRIVATE ElectronicATEInstrumentCore)
add_test(NAME binary_transfer_budget COMMAND binary_transfer_budget_test)
set_tests_properties(binary_transfer_budget PROPERTIES TIMEOUT 10)

add_executable(grouped_conditions_test testconditions/grouped_conditions_test.cpp)
target_link_libraries(grouped_conditions_test PRIVATE ElectronicATEConditions)
add_test(NAME grouped_conditions_model COMMAND grouped_conditions_test)

add_executable(chroma63600_quick_test commandconsole/chroma63600_quick_test.cpp)
target_link_libraries(chroma63600_quick_test PRIVATE Qt6::Widgets)
add_test(NAME chroma63600_quick_commands COMMAND chroma63600_quick_test)
set_tests_properties(chroma63600_quick_commands PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(command_console_editor_test commandconsole/command_console_editor_test.cpp ../src/views/commandconsole/commandconsolepage.cpp ../src/views/commandconsole/commandconsolepage.h)
target_link_libraries(command_console_editor_test PRIVATE ElectronicATEConsole ElectronicATEInstrumentCore Qt6::Widgets)
add_test(NAME command_console_offline_editor COMMAND command_console_editor_test)
set_tests_properties(command_console_offline_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)

add_executable(condition_values_test testconditions/condition_values_test.cpp)
target_link_libraries(condition_values_test PRIVATE ElectronicATEConditions)
add_test(NAME condition_values COMMAND condition_values_test)

add_executable(scope_architecture_test architecture/scope_architecture_test.cpp)
target_link_libraries(scope_architecture_test PRIVATE ElectronicATEDrivers ElectronicATEManualScope
    ElectronicATEScopeSession ElectronicATETestRun ElectronicATEModels)
foreach(scenario driver measurement period manual lease rules)
    add_test(NAME scope_architecture_${scenario} COMMAND scope_architecture_test ${scenario})
    set_tests_properties(scope_architecture_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

add_test(NAME architecture_boundaries COMMAND ${CMAKE_COMMAND}
    -DARCHITECTURE_SOURCE_ROOT=${CMAKE_CURRENT_LIST_DIR}/../src
    -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/CheckArchitecture.cmake)
add_test(NAME architecture_guard_rejects_violations COMMAND ${CMAKE_COMMAND}
    -P ${CMAKE_CURRENT_LIST_DIR}/architecture/architecture_guard_test.cmake)
set(architecture_platform_cases "")
if(WIN32)
    list(APPEND architecture_platform_cases source_windows_relative source_windows_basename
        source_windows_cycle target_windows_sources)
endif()
foreach(case
    source_allowed source_service_basename source_service_relative source_service_transitive
    source_self_cycle source_pair_cycle source_long_cycle source_diamond source_ambiguous
    source_explicit_duplicate source_key_collision source_repeat_check
    target_diamond target_self_cycle target_pair_cycle target_long_cycle target_alias_cycle
    target_link_only_cycle target_if_cycle target_config_cycle target_list_cycle target_condition_not_edge
    target_build_interface_cycle target_install_only target_unknown_expression target_subdirectory_cycle
    target_service_transitive target_viewmodel_service target_service_sources_expression incremental_edit incremental_add
    ${architecture_platform_cases})
    add_test(NAME architecture_dependency_${case} COMMAND ${CMAKE_COMMAND}
        -DCASE=${case} -P ${CMAKE_CURRENT_LIST_DIR}/architecture/architecture_dependency_test.cmake)
    set_tests_properties(architecture_dependency_${case} PROPERTIES TIMEOUT 30)
endforeach()

target_link_libraries(oscilloscope_configuration_test PRIVATE ElectronicATEDrivers)

add_executable(local_five_test integration/local_five_test.cpp
    ../src/views/instrumentsetup/instrumentsetuppage.cpp ../src/views/instrumentsetup/instrumentsetuppage.h
    ../src/views/instrumentsetup/dialogs/communicationconfigdialog.cpp
    ../src/views/instrumentsetup/dialogs/communicationconfigdialog.h
    ../src/service/oscilloscope/oscilloscopeconfiguration.cpp)
target_include_directories(local_five_test PRIVATE ../src/views/instrumentsetup/dialogs)
target_link_libraries(local_five_test PRIVATE ElectronicATEInstrumentSettings ElectronicATEDrivers ElectronicATEConnectionTest ElectronicATEScopeSession
    ElectronicATEOperations ElectronicATEModels Qt6::Widgets Qt6::SerialPort)
foreach(scenario connection retirement rules outputs serial instrument_setup_serial tcp)
    add_test(NAME local_five_${scenario} COMMAND local_five_test ${scenario})
    set_tests_properties(local_five_${scenario} PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

target_link_libraries(review_fixes_test PRIVATE ElectronicATEDrivers)

add_executable(strategy_capability_test architecture/strategy_capability_test.cpp
    ../src/service/oscilloscopestrategy/steadystatesearchstrategy.cpp
    ../src/service/oscilloscopestrategy/steadysearch.cpp
    ../src/service/oscilloscopestrategy/oscilloscopestrategyfactory.cpp
    ../src/service/oscilloscopestrategy/turnonstrategy.cpp
    ../src/service/oscilloscopestrategy/turnoffstrategy.cpp
    ../src/service/oscilloscopestrategy/turnonthenshortstrategy.cpp
    ../src/service/oscilloscopestrategy/shortthenturnonstrategy.cpp
    ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(strategy_capability_test PRIVATE ../src/service/oscilloscopestrategy
    ../src/hardware/instrument/oscilloscope)
target_link_libraries(strategy_capability_test PRIVATE ElectronicATEMeasurementStrategies)
add_test(NAME strategy_semantic_capabilities COMMAND strategy_capability_test)

add_executable(scope_ports_test architecture/scope_ports_test.cpp
    ../src/service/oscilloscope/oscilloscopeconfiguration.cpp)
target_link_libraries(scope_ports_test PRIVATE ElectronicATEDrivers ElectronicATEManualScope
    ElectronicATECapture ElectronicATEStorage)
target_include_directories(scope_ports_test PRIVATE ../src/service/oscilloscope)
foreach(scenario configuration capture manual_driver manual_capability)
    add_test(NAME scope_ports_${scenario} COMMAND scope_ports_test ${scenario})
    set_tests_properties(scope_ports_${scenario} PROPERTIES TIMEOUT 20)
endforeach()
set_tests_properties(strategy_semantic_capabilities PROPERTIES TIMEOUT 20)

add_executable(dynamic_group_test testsequence/strategies/dynamic_group_test.cpp)
target_link_libraries(dynamic_group_test PRIVATE ElectronicATETestRun ElectronicATEReport ElectronicATEModels ElectronicATEStorage)
target_include_directories(dynamic_group_test PRIVATE ../src/viewmodels/presentation ../src/service/oscilloscopestrategy)
foreach(scenario success rise fall single dc no_period input_failure load_failure failure period_failure capture_failure cancel retry cleanup preflight capture xml)
    add_test(NAME dynamic_group_${scenario} COMMAND dynamic_group_test ${scenario})
    set_tests_properties(dynamic_group_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

add_executable(dynamic_group_dialog_test testsequence/dialogs/dynamic_group_dialog_test.cpp
    ../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.cpp ../resource.qrc)
set_target_properties(dynamic_group_dialog_test PROPERTIES AUTORCC ON)
target_include_directories(dynamic_group_dialog_test PRIVATE ../src/views/testsequence/dialogs/stressdialogs)
target_link_libraries(dynamic_group_dialog_test PRIVATE ElectronicATETaskPreparation Qt6::Widgets)
add_test(NAME dynamic_group_dialog COMMAND dynamic_group_dialog_test)
set_tests_properties(dynamic_group_dialog PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(group_test testsequence/strategies/group_test.cpp ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_link_libraries(group_test PRIVATE ElectronicATETestRun ElectronicATEReport ElectronicATEModels ElectronicATEStorage)
target_include_directories(group_test PRIVATE ../src/viewmodels/presentation ../src/service/oscilloscopestrategy)
foreach(kind staticGroup dynamicGroup turnOnGroup turnOffGroup shortOnGroup onShortGroup)
    foreach(scenario success failure cancel retry cleanup_contract preflight capture capture_retry xml dual dual_cancel dual_on_failure dual_on_exception dual_off_exception dual_missing dual_bad_index)
        add_test(NAME group_${kind}_${scenario} COMMAND group_test ${kind} ${scenario})
        set_tests_properties(group_${kind}_${scenario} PROPERTIES TIMEOUT 30)
    endforeach()
endforeach()
add_executable(group_dialog_test testsequence/dialogs/group_dialog_test.cpp ../resource.qrc
    ../src/views/testsequence/dialogs/stressdialogs/staticdialog.cpp
    ../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/turnonoffdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/shorttestdialog.cpp
    ../src/views/testsequence/testsequenceleftpanel.cpp ../src/ui/widget/sidepanel.cpp ../src/ui/style/testsequencetreestyle.cpp)
set_target_properties(group_dialog_test PROPERTIES AUTORCC ON)
target_include_directories(group_dialog_test PRIVATE ../src/views/testsequence/dialogs/stressdialogs
    ../src/views/testsequence/dialogs/turnonoffdialogs ../src/views/testsequence/dialogs/relaydialogs
    ../src/views/testsequence ../src/ui/widget ../src/ui/style)
target_link_libraries(group_dialog_test PRIVATE ElectronicATETaskPreparation ElectronicATEModels Qt6::Widgets)
add_test(NAME group_dialogs COMMAND group_dialog_test)
set_tests_properties(group_dialogs PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(checkbox_style_review_test integration/checkbox_style_review_test.cpp ../resource.qrc
    ../src/views/testsequence/dialogs/stressdialogs/staticdialog.cpp
    ../src/views/testsequence/dialogs/stressdialogs/dynamicdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/turnonoffdialog.cpp
    ../src/views/testsequence/dialogs/turnonoffdialogs/shorttestdialog.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/oscwritedialogbase.cpp
    ../src/views/testsequence/dialogs/oscilloscopedialogs/msoseries456writedialog.cpp)
set_target_properties(checkbox_style_review_test PROPERTIES AUTORCC ON)
target_include_directories(checkbox_style_review_test PRIVATE ../src/views/testsequence/dialogs/relaydialogs)
target_link_libraries(checkbox_style_review_test PRIVATE ElectronicATEData ElectronicATEInstrumentCore Qt6::Widgets)
add_test(NAME checkbox_style_review COMMAND checkbox_style_review_test)
set_tests_properties(checkbox_style_review PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(brownout_test testsequence/strategies/brownout_test.cpp)
add_executable(brownout_capture_test testsequence/strategies/brownout_capture_test.cpp)
add_executable(brownout_capture_sync_test testsequence/strategies/brownout_capture_sync_test.cpp)
target_link_libraries(brownout_capture_sync_test PRIVATE ElectronicATETestRun ElectronicATEModels)
foreach(scenario success slow_write query_failure save_failure monitor_failure rearm_failure rearm_timeout stop_capture stop_rearm)
    add_test(NAME brownout_capture_sync_${scenario} COMMAND brownout_capture_sync_test ${scenario})
    set_tests_properties(brownout_capture_sync_${scenario} PROPERTIES TIMEOUT 20)
endforeach()
target_link_libraries(brownout_capture_test PRIVATE ElectronicATETestRun ElectronicATEModels)
foreach(scenario fixed track failure cancel preflight query_failure)
    add_test(NAME brownout_capture_${scenario} COMMAND brownout_capture_test ${scenario})
    set_tests_properties(brownout_capture_${scenario} PROPERTIES TIMEOUT 15)
endforeach()
target_link_libraries(brownout_test PRIVATE ElectronicATETestRun ElectronicATEModels)
foreach(scenario fixed rise fall startup already_off monitor_error timeout rebound no_trigger scope_error
                 invalid cancel cleanup cleanup_empty scope_cleanup slow_cleanup prepare_error xml service service_cleanup
                 delayed_reference long_record invalid_duration slow_ramp current_trigger unit_changed stale_reference)
    add_test(NAME brownout_${scenario} COMMAND brownout_test ${scenario})
    set_tests_properties(brownout_${scenario} PROPERTIES TIMEOUT 15)
endforeach()
add_executable(brownout_driver_test hardware/brownout_driver_test.cpp
    ../src/service/instrumentexecutor/brownoutio.cpp ../src/service/inputparameters/parameterparser.cpp)
target_include_directories(brownout_driver_test PRIVATE ../src/service/instrumentexecutor ../src/service/inputparameters)
target_link_libraries(brownout_driver_test PRIVATE ElectronicATEDrivers)
foreach(scenario fixed normal clipped capture capture_clipped partial encoding mixed restore_error monitor source adapter adapter_dc adapter_6530 adapter_cancel adapter_readback long_timebase invalid_timebase
                 monitor_6310 monitor_6310a adapter_6310 adapter_6310a adapter_6310_startup adapter_6310a_startup
                 adapter_6310_runtime adapter_6310a_runtime adapter_6310_channel adapter_6310a_channel
                 dual_ac dual_dc dual_preflight dual_startup_failure dual_cancel dual_cleanup_failure)
    add_test(NAME brownout_driver_${scenario} COMMAND brownout_driver_test ${scenario})
    set_tests_properties(brownout_driver_${scenario} PROPERTIES TIMEOUT 15)
endforeach()
add_executable(brownout_dialog_test testsequence/dialogs/brownout_dialog_test.cpp ../resource.qrc)
set_target_properties(brownout_dialog_test PROPERTIES AUTORCC ON)
target_include_directories(brownout_dialog_test PRIVATE ../src/views/testsequence/dialogs)
target_link_libraries(brownout_dialog_test PRIVATE ElectronicATEData Qt6::Widgets)
add_test(NAME brownout_dialog COMMAND brownout_dialog_test)
foreach(scenario capture_defaults capture_empty)
    add_test(NAME dialog_${scenario} COMMAND brownout_dialog_test ${scenario})
    set_tests_properties(dialog_${scenario} PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
set_tests_properties(brownout_dialog PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
foreach(worker_test test_sequence_worker_failure_test architecture_test_sequence_test)
    target_sources(${worker_test} PRIVATE ../src/service/instrumentexecutor/brownoutio.cpp
        ../src/service/inputparameters/parameterparser.cpp)
    target_include_directories(${worker_test} PRIVATE ../src/service/inputparameters)
    target_link_libraries(${worker_test} PRIVATE ElectronicATEDrivers)
endforeach()

# Check the actual production targets as well as isolated violation fixtures.
set(ARCHITECTURE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src")
include(../cmake/CheckArchitecture.cmake)

add_executable(audit_output_test testsequence/strategies/audit_output_test.cpp
    ../src/hardware/instrumentcreator/instrumentcreator.cpp
    ../src/hardware/instrumentcreator/resourcecleaner.cpp
    ../src/service/instrumentexecutor/instrumentexecutor_input.cpp
    ../src/service/instrumentexecutor/instrumentexecutor_load.cpp
    ../src/service/instrumentexecutor/instrumentexecutor_dynamicload.cpp
    ../src/service/instrumentexecutor/instrumentexecutor_relay.cpp
    ../src/service/instrumentexecutor/instrumentexecutor_sync.cpp
    ../src/service/instrumentexecutor/measurementpreflight.cpp
    ../src/service/instrumentexecutor/measurementpreflight6310.cpp
    ../src/service/conditions/conditionlookup.cpp
    ../src/service/inputparameters/parameterparser.cpp)
target_include_directories(audit_output_test PRIVATE ../src/service/instrumentexecutor
    ../src/hardware/instrumentcreator ../src/service/conditions ../src/service/inputparameters)
target_link_libraries(audit_output_test PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)
foreach(scenario creator preflight recovery session close_recovery)
    add_test(NAME audit_output_${scenario} COMMAND audit_output_test ${scenario})
endforeach()
foreach(kind "Static Test" "Dynamic Test" "Turn on" "Turn off" "Short then turn on" "Turn on then short"
             "Static Group Test" "Dynamic Group Test" "Turn on Group Test" "Turn off Group Test"
             "Short then turn on Group Test" "Turn on then short Group Test" "Brown Out")
    string(REPLACE " " "_" test_kind "${kind}")
    foreach(input AC DC)
        foreach(mode handoff off_failure)
            add_test(NAME audit_${test_kind}_${input}_${mode} COMMAND audit_output_test ${mode} "${kind}" ${input})
            set_tests_properties(audit_${test_kind}_${input}_${mode} PROPERTIES TIMEOUT 15)
        endforeach()
    endforeach()
endforeach()

foreach(scenario dual_connect_failure dual_dc_connect_failure)
    add_test(NAME brownout_driver_${scenario} COMMAND brownout_driver_test ${scenario})
    set_tests_properties(brownout_driver_${scenario} PROPERTIES TIMEOUT 15)
endforeach()

# Regression for R01/R02: partial construction must preserve OFF and reject ON.
get_target_property(review_probe_sources audit_output_test SOURCES)
list(REMOVE_ITEM review_probe_sources testsequence/strategies/audit_output_test.cpp)
add_executable(short_handoff_test testsequence/strategies/short_handoff_test.cpp ${review_probe_sources})
get_target_property(short_handoff_includes audit_output_test INCLUDE_DIRECTORIES)
target_include_directories(short_handoff_test PRIVATE ${short_handoff_includes})
target_link_libraries(short_handoff_test PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)
foreach(kind "Short then turn on" "Turn on then short" "Short then turn on Group Test" "Turn on then short Group Test")
    string(REPLACE " " "_" short_kind "${kind}")
    foreach(fault ac dc dc2 throw)
        add_test(NAME short_handoff_${short_kind}_${fault} COMMAND short_handoff_test "${kind}" ${fault})
        set_tests_properties(short_handoff_${short_kind}_${fault} PROPERTIES TIMEOUT 20)
    endforeach()
endforeach()
add_executable(steady_transition_test testsequence/strategies/steady_transition_test.cpp ${review_probe_sources})
get_target_property(transition_includes audit_output_test INCLUDE_DIRECTORIES)
target_include_directories(steady_transition_test PRIVATE ${transition_includes})
target_link_libraries(steady_transition_test PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)
foreach(scenario settings success off_failure cleanup_failure config_failure stop_off stop_startup stop_settle stop_measure
                 relay_all_off relay_overlap relay_unmapped relay_invalid_value relay_size relay_index)
    add_test(NAME steady_transition_${scenario} COMMAND steady_transition_test ${scenario})
endforeach()
add_executable(review_followup_probe integration/review_followup_probe.cpp ${review_probe_sources})
get_target_property(review_probe_includes audit_output_test INCLUDE_DIRECTORIES)
target_include_directories(review_followup_probe PRIVATE ${review_probe_includes})
target_link_libraries(review_followup_probe PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)

add_test(NAME review_followup_regression COMMAND review_followup_probe)
set_tests_properties(review_followup_regression PROPERTIES TIMEOUT 15)

# Real-driver failure-injection regression for B01-B05.
add_executable(comprehensive_off_audit_probe testsequence/strategies/comprehensive_off_audit_probe.cpp ${review_probe_sources}
    ../src/service/instrumentexecutor/dcinputoperation.cpp)
target_include_directories(comprehensive_off_audit_probe PRIVATE ${review_probe_includes})
target_link_libraries(comprehensive_off_audit_probe PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)

add_test(NAME comprehensive_off_regression COMMAND comprehensive_off_audit_probe)
set_tests_properties(comprehensive_off_regression PROPERTIES TIMEOUT 30)

# C01: real ManualControl ViewModel/queue sync acknowledgement regression.
add_executable(architecture_review_probe architecture/architecture_review_probe.cpp)
target_link_libraries(architecture_review_probe PRIVATE ElectronicATEManualControl)

add_test(NAME manual_control_sync_retry_regression COMMAND architecture_review_probe)
set_tests_properties(manual_control_sync_retry_regression PROPERTIES TIMEOUT 20)

# Local audit reproductions: successful exit confirms an unfixed defect, not acceptance.
get_target_property(test_sequence_review_strategy_sources strategy_matrix_test SOURCES)
list(REMOVE_ITEM test_sequence_review_strategy_sources testsequence/strategies/strategy_matrix_test.cpp)
# Optional local diagnostics are not part of the fixed CTest inventory.
if(ELECTRONICATE_TEST_PROFILE STREQUAL "local-full"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local/diagnostics/test_sequence_measurement_review_20261014.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local/diagnostics/test_sequence_relay_review_20261014.cpp")
add_executable(test_sequence_measurement_review_20261014 local/diagnostics/test_sequence_measurement_review_20261014.cpp ${test_sequence_review_strategy_sources})
target_include_directories(test_sequence_measurement_review_20261014 PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(test_sequence_measurement_review_20261014 PRIVATE ElectronicATEInstrumentCore)
add_executable(test_sequence_relay_review_20261014 local/diagnostics/test_sequence_relay_review_20261014.cpp ${review_probe_sources})
target_include_directories(test_sequence_relay_review_20261014 PRIVATE ${review_probe_includes})
target_link_libraries(test_sequence_relay_review_20261014 PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)
endif()

add_executable(test_sequence_gap_fixes_test testsequence/execution/test_sequence_gap_fixes_test.cpp ${test_sequence_review_strategy_sources})
target_include_directories(test_sequence_gap_fixes_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(test_sequence_gap_fixes_test PRIVATE ElectronicATEInstrumentCore)
foreach(mode measurement legal_na ready)
    add_test(NAME test_sequence_gap_${mode} COMMAND test_sequence_gap_fixes_test ${mode})
    set_tests_properties(test_sequence_gap_${mode} PROPERTIES TIMEOUT 30)
endforeach()
add_executable(test_sequence_relay_gap_fixes_test testsequence/execution/test_sequence_relay_gap_fixes_test.cpp ${review_probe_sources})
target_include_directories(test_sequence_relay_gap_fixes_test PRIVATE ${review_probe_includes})
target_link_libraries(test_sequence_relay_gap_fixes_test PRIVATE ElectronicATETestRun ElectronicATEDrivers ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::SerialPort)
add_test(NAME test_sequence_gap_relay COMMAND test_sequence_relay_gap_fixes_test)

add_executable(connection_recovery_test hardware/connection_recovery_test.cpp ${review_probe_sources}
    ../src/service/instrumentexecutor/dcinputoperation.cpp)
target_include_directories(connection_recovery_test PRIVATE ${review_probe_includes} ../src/ui)
target_link_libraries(connection_recovery_test PRIVATE ElectronicATETestRun ElectronicATEDrivers
    ElectronicATEManualControl ElectronicATEAutomatedTasks Qt6::Widgets Qt6::SerialPort)
foreach(scenario relay sources state manualControl testSequence)
    add_test(NAME connection_recovery_${scenario} COMMAND connection_recovery_test ${scenario})
    set_tests_properties(connection_recovery_${scenario} PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# Local behavior comparison against the pre-refactor implementation.
add_executable(transient_split_equivalence_test testsequence/strategies/compatibility/transient_split_equivalence_test.cpp
    testsequence/strategies/compatibility/transient_legacy_snapshot.cpp ${transient_sources}
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(transient_split_equivalence_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(transient_split_equivalence_test PRIVATE ElectronicATEInstrumentCore)
foreach(kind RANGE 0 3)
    add_test(NAME transient_split_equivalence_${kind} COMMAND transient_split_equivalence_test ${kind})
    set_tests_properties(transient_split_equivalence_${kind} PROPERTIES TIMEOUT 120)
endforeach()

add_executable(steady_split_equivalence_test testsequence/strategies/compatibility/steady_split_equivalence_test.cpp testsequence/strategies/compatibility/steady_legacy_snapshot.cpp
    ../src/service/oscilloscopestrategy/steadystatesearchstrategy.cpp
    ../src/service/oscilloscopestrategy/steadysearch.cpp
    ../src/service/oscilloscopestrategy/oscilloscopeacquisition.cpp)
target_include_directories(steady_split_equivalence_test PRIVATE ../src/service/oscilloscopestrategy)
target_link_libraries(steady_split_equivalence_test PRIVATE ElectronicATEInstrumentCore)
foreach(direction BOTH MAX MIN INVALID)
    add_test(NAME steady_split_equivalence_${direction} COMMAND steady_split_equivalence_test ${direction})
    set_tests_properties(steady_split_equivalence_${direction} PROPERTIES TIMEOUT 30)
endforeach()
offline_finalize_tests()
