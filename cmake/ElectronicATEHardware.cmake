include(${CMAKE_CURRENT_LIST_DIR}/ElectronicATEDrivers.cmake)
add_library(ElectronicATETransports STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication/commscanner.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication/communicationfactory.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication/gpibcommunication.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication/serialcommunication.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication/tcpcommunication.cpp")
target_include_directories(ElectronicATETransports PUBLIC "${ELECTRONICATE_SOURCE_ROOT}/hardware/communication"
    PRIVATE "${ELECTRONICATE_VISA_INCLUDE_DIR}")
find_library(ELECTRONICATE_VISA_LIBRARY NAMES visa64
    PATHS "${ELECTRONICATE_VISA_LIBRARY_DIR}" NO_DEFAULT_PATH)
if(NOT ELECTRONICATE_VISA_LIBRARY)
    message(FATAL_ERROR "VISA library not found in ELECTRONICATE_VISA_LIBRARY_DIR")
endif()
target_link_libraries(ElectronicATETransports PUBLIC ElectronicATEData Qt6::SerialPort
    PRIVATE "${ELECTRONICATE_VISA_LIBRARY}")

add_library(ElectronicATEHardwareAdapters STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrumentcreator/instrumentcreator.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrumentcreator/resourcecleaner.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/dcinputoperation.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/instrumentexecutor_dynamicload.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/instrumentexecutor_input.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/instrumentexecutor_load.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/instrumentexecutor_relay.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor/instrumentexecutor_sync.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/conditions/conditionlookup.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/service/inputparameters/parameterparser.cpp")
target_include_directories(ElectronicATEHardwareAdapters PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrumentcreator"
    "${ELECTRONICATE_SOURCE_ROOT}/service/instrumentexecutor"
    "${ELECTRONICATE_SOURCE_ROOT}/service/conditions"
    "${ELECTRONICATE_SOURCE_ROOT}/service/inputparameters")
target_link_libraries(ElectronicATEHardwareAdapters PUBLIC ElectronicATEDrivers ElectronicATETransports)
set(ELECTRONICATE_ADAPTER_TARGETS ElectronicATEDrivers ElectronicATETransports ElectronicATEHardwareAdapters)
