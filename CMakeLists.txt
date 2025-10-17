cmake_minimum_required(VERSION 3.16)
project(ElectronicATE VERSION 0.1 LANGUAGES CXX)

include_directories("C:/Program Files (x86)/IVI Foundation/VISA/WinNT/Include")
link_directories("C:/Program Files (x86)/IVI Foundation/VISA/WinNT/Lib_x64/msc")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自動處理 .ui / moc / rcc
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 找 Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Xml Network SerialPort Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Xml Network SerialPort Concurrent)

# 把所有子資料夾的 header 都加進來
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/communication
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/delegate
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/style
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/widget
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/instrument/acsource
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/instrument/dcload
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/instrument/oscilloscope
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/data
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/controller
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/service
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/worker

  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/mainwindow
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/page1
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/page2
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/page3
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/page4

  ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/mainwindow
  ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/page1
  ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/page2
  ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/page3
  ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/page4

  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/page1
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/page2
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/page3
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/page4
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/tablewidget
)

# 自動蒐集所有來源檔案
file(GLOB_RECURSE SHARED_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/**/*.cpp"
)
file(GLOB_RECURSE MODEL_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/models/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/models/**/*.cpp"
)
file(GLOB_RECURSE VIEWMODEL_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/**/*.cpp"
)
file(GLOB_RECURSE VIEW_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/views/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/views/**/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/views/**/*.ui"
)

set(PROJECT_SOURCES
  main.cpp
  ${SHARED_SRC}
  ${MODEL_SRC}
  ${VIEWMODEL_SRC}
  ${VIEW_SRC}
  resource.qrc
)

# 建立執行檔
if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(ElectronicATE
        WIN32
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(ElectronicATE
        WIN32
        ${PROJECT_SOURCES}
    )
endif()

# 連結 Qt 模組
target_link_libraries(ElectronicATE PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Concurrent
    visa64
)

# Qt6 的 finalize 步驟
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ElectronicATE)
endif()

# === 複製 XML 資料夾 ===============================================
add_custom_command(TARGET ElectronicATE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/XML
        $<TARGET_FILE_DIR:ElectronicATE>/XML
    COMMENT "Copying XML folder to build directory"
)

# === Windows 自動部署 Qt DLL / plugins ===============================
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    # 尋找 windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
            "$ENV{QTDIR}/bin"
        DOC "Path to windeployqt executable"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # 直接使用 POST_BUILD，不建立額外的 target
        add_custom_command(TARGET ElectronicATE POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    "$<TARGET_FILE:ElectronicATE>"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:ElectronicATE>"
            COMMENT "Deploying Qt runtime libraries..."
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found. Manual deployment required.")
        message(WARNING "After building, run: windeployqt.exe ElectronicATE.exe")
    endif()
endif()
