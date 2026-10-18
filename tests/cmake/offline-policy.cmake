# Test-only policy; product targets and assertions are unchanged.
set(ELECTRONICATE_TEST_PROFILE "local-full" CACHE STRING "Offline test profile")
set_property(CACHE ELECTRONICATE_TEST_PROFILE PROPERTY STRINGS local-full ci-offline)
if(NOT ELECTRONICATE_TEST_PROFILE MATCHES "^(local-full|ci-offline)$")
    message(FATAL_ERROR "Unknown ELECTRONICATE_TEST_PROFILE: ${ELECTRONICATE_TEST_PROFILE}")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(OFFLINE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(OFFLINE_MANIFEST "${OFFLINE_TEST_ROOT}/ctest-manifest.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${OFFLINE_MANIFEST}" "${OFFLINE_TEST_ROOT}/tools/ctest_gate.py")

function(offline_add_qttest name target)
    add_test(NAME "${name}" COMMAND "${Python3_EXECUTABLE}"
        "${OFFLINE_TEST_ROOT}/tools/run_qttest.py"
        --manifest "${OFFLINE_MANIFEST}" --profile "${ELECTRONICATE_TEST_PROFILE}"
        --suite "${OFFLINE_SUITE}" --case "${name}"
        -- "$<TARGET_FILE:${target}>" ${ARGN})
endfunction()

function(offline_finalize_tests)
    get_property(registered DIRECTORY PROPERTY TESTS)
    string(REPLACE ";" "\n" names "${registered}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/offline-registered.txt" "${names}\n")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${OFFLINE_TEST_ROOT}/tools/ctest_gate.py"
        --manifest "${OFFLINE_MANIFEST}" --suite "${OFFLINE_SUITE}"
        --profile "${ELECTRONICATE_TEST_PROFILE}"
        --names "${CMAKE_CURRENT_BINARY_DIR}/offline-registered.txt"
        --output "${CMAKE_CURRENT_BINARY_DIR}/offline-properties.cmake"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Offline inventory rejected configuration: ${output}${error}")
    endif()
    include("${CMAKE_CURRENT_BINARY_DIR}/offline-properties.cmake")
endfunction()
