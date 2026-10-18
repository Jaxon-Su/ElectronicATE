cmake_minimum_required(VERSION 3.21)
project(WfmViewerTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Core Concurrent Widgets Test)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(OFFLINE_SUITE wfm)
include(../cmake/offline-policy.cmake)
set(ELECTRONICATE_WFM_FIXTURES "" CACHE PATH "Verified official WFM fixtures")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${REPO}/tests/ci/prepare_test_dependencies.py"
    --verify-wfm "${ELECTRONICATE_WFM_FIXTURES}" RESULT_VARIABLE fixture_status)
if(NOT fixture_status EQUAL 0)
    message(FATAL_ERROR "Prepare the pinned WFM fixtures before configuring tests")
endif()
add_executable(wfm_viewer_test wfm_viewer_test.cpp
    "${REPO}/src/service/waveform/waveformstatistics.cpp"
    "${REPO}/src/infrastructure/waveform/tekwfmreader.cpp"
    "${REPO}/src/infrastructure/waveform/waveformcsv.cpp"
    "${REPO}/src/viewmodels/wfmviewer/wfmviewerviewmodel.cpp"
    "${REPO}/src/views/wfmviewer/waveformplot.cpp"
    "${REPO}/src/views/wfmviewer/wfmviewerpage.cpp")
target_link_libraries(wfm_viewer_test PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent Qt6::Widgets)
file(TO_CMAKE_PATH "${ELECTRONICATE_WFM_FIXTURES}" WFM_FIXTURE_DIR)
target_compile_definitions(wfm_viewer_test PRIVATE WFM_FIXTURE_DIR="${WFM_FIXTURE_DIR}")
# Run all slots; the wrapper also verifies the fixed function/data-row inventory.
offline_add_qttest(wfmviewer.all wfm_viewer_test -o -,txt)
set_tests_properties(wfmviewer.all PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
offline_finalize_tests()
