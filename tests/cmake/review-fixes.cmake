set(REVIEW_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../src)
add_executable(review_fixes_test integration/review_fixes_test.cpp
    ${REVIEW_ROOT}/hardware/instrument/acsource/chroma61505.cpp
    ${REVIEW_ROOT}/hardware/instrument/acsource/chroma61509.cpp
    ${REVIEW_ROOT}/service/instrumentexecutor/instrumentexecutor_load.cpp
    ${REVIEW_ROOT}/service/instrumentexecutor/instrumentexecutor_input.cpp
    ${REVIEW_ROOT}/service/inputparameters/parameterparser.cpp
    ${REVIEW_ROOT}/hardware/instrument/dcload/chroma63600.cpp
    ${REVIEW_ROOT}/hardware/instrument/dcload/dcloadspec/chromaload63600spec.cpp
    ${REVIEW_ROOT}/hardware/instrument/relay/relay_rtu_4.cpp
    ${REVIEW_ROOT}/service/instrumentexecutor/instrumentexecutor_sync.cpp
    ${REVIEW_ROOT}/service/instrumentexecutor/instrumentexecutor_dynamicload.cpp
    ${REVIEW_ROOT}/service/instrumentexecutor/instrumentexecutor_relay.cpp
    ${REVIEW_ROOT}/service/conditions/conditionlookup.cpp
    ${REVIEW_ROOT}/hardware/instrument/oscilloscope/msoseries456.cpp
    ${REVIEW_ROOT}/ui/controller/msoseries456triggercontroller.cpp
    ${REVIEW_ROOT}/service/oscilloscope/itriggercontroller.h
    ${REVIEW_ROOT}/ui/widget/smartstepspinbox.cpp
    ${REVIEW_ROOT}/ui/widget/msoseries456triggerwidget.cpp)
target_include_directories(review_fixes_test PRIVATE
    ${REVIEW_ROOT}/hardware/instrument/acsource ${REVIEW_ROOT}/hardware/instrument/relay
    ${REVIEW_ROOT}/hardware/instrument/dcload ${REVIEW_ROOT}/hardware/instrument/dcload/dcloadspec
    ${REVIEW_ROOT}/hardware/instrumentcreator ${REVIEW_ROOT}/service/instrumentexecutor
    ${REVIEW_ROOT}/service/inputparameters
    ${REVIEW_ROOT}/service/conditions ${REVIEW_ROOT}/hardware/instrumentcreator
    ${REVIEW_ROOT}/hardware/instrument/oscilloscope ${REVIEW_ROOT}/ui/controller
    ${REVIEW_ROOT}/service/oscilloscope ${REVIEW_ROOT}/infrastructure/execution ${REVIEW_ROOT}/infrastructure/worker
    ${REVIEW_ROOT}/infrastructure/storage ${REVIEW_ROOT}/ui/widget)
target_link_libraries(review_fixes_test PRIVATE ElectronicATEManualScope ElectronicATEInstrumentCore ElectronicATEStorage ElectronicATEMessages ElectronicATEData Qt6::Widgets)
foreach(scenario connection_state static_validation write relay_ack relay_mapping off dynamic poll channels manual_async manual_destroy step_scale step_scale_destroy)
    add_test(NAME review_fix_${scenario} COMMAND review_fixes_test ${scenario})
    set_tests_properties(review_fix_${scenario} PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

set(ELECTRONICATE_VISA_INCLUDE "C:/Program Files (x86)/IVI Foundation/VISA/WinNT/Include" CACHE PATH "Official VISA SDK headers for all profiles")
if(ELECTRONICATE_TEST_PROFILE MATCHES "^(local-full|ci-offline)$")
    if(NOT EXISTS "${ELECTRONICATE_VISA_INCLUDE}/visa.h")
        message(FATAL_ERROR "${ELECTRONICATE_TEST_PROFILE} requires the official VISA SDK: ${ELECTRONICATE_VISA_INCLUDE}/visa.h")
    endif()
    add_executable(visa_partial_test hardware/visa_partial_test.cpp
        ${REVIEW_ROOT}/hardware/communication/tcpcommunication.cpp
        ${REVIEW_ROOT}/hardware/communication/gpibcommunication.cpp)
    target_include_directories(visa_partial_test PRIVATE ${ELECTRONICATE_VISA_INCLUDE} ${REVIEW_ROOT}/hardware/communication)
    target_link_libraries(visa_partial_test PRIVATE Qt6::Core)
    add_test(NAME review_fix_visa_partial COMMAND visa_partial_test)
endif()
