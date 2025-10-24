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

# ========================================
# 編譯時自動複製資源（給開發用）
# ========================================

# 複製 XML 資料夾到 build 目錄（開發測試用）
add_custom_command(TARGET ElectronicATE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/XML
        $<TARGET_FILE_DIR:ElectronicATE>/XML
    COMMENT "Copying XML folder to build directory"
)

# ========================================
# Windows 自動部署 Qt DLL / plugins
# ========================================
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

        # 編譯後自動執行 windeployqt
        add_custom_command(TARGET ElectronicATE POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --release
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

# ========================================
# 安裝規則（用於正式發布部署）
# cmake --install . --prefix <目標資料夾>
# ========================================

# 1. 安裝執行檔
install(TARGETS ElectronicATE
    RUNTIME DESTINATION .
    COMPONENT Application
)

# 2. 安裝 XML 資料夾（儀器設定檔）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/XML
    DESTINATION .
    COMPONENT Resources
    FILES_MATCHING PATTERN "*.xml"
)

# 3. 安裝 README 文件
install(FILES ${CMAKE_SOURCE_DIR}/README.md
    DESTINATION .
    COMPONENT Documentation
    RENAME README.txt
)

# 4. 安裝程式架構總覽（如果存在）
install(FILES ${CMAKE_SOURCE_DIR}/程式架構總覽.txt
    DESTINATION .
    COMPONENT Documentation
    OPTIONAL
)

# 5. 可選：安裝範例設定檔（如果有的話）
# install(FILES
#     ${CMAKE_SOURCE_DIR}/config-example.xml
#     DESTINATION .
#     COMPONENT Configuration
#     OPTIONAL
# )

# 6. 可選：安裝圖片資源（如果執行時需要）
# install(DIRECTORY ${CMAKE_SOURCE_DIR}/images
#     DESTINATION .
#     COMPONENT Resources
#     FILES_MATCHING
#     PATTERN "*.png"
#     PATTERN "*.jpg"
#     PATTERN "*.ico"
# )

# ========================================
# 建立安裝後執行 windeployqt 的腳本
# ========================================
if(WIN32)
    # 產生一個部署腳本
    set(DEPLOY_SCRIPT "${CMAKE_BINARY_DIR}/deploy_qt.bat")
    file(WRITE ${DEPLOY_SCRIPT}
        "@echo off\n"
        "echo Deploying Qt dependencies...\n"
        "\"${WINDEPLOYQT_EXECUTABLE}\" --release --no-translations \"%~dp0\\ElectronicATE.exe\"\n"
        "echo Qt deployment completed!\n"
        "pause\n"
    )

    # 安裝部署腳本（可選）
    install(FILES ${DEPLOY_SCRIPT}
        DESTINATION .
        COMPONENT Scripts
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
        OPTIONAL
    )
endif()

# ========================================
# 顯示安裝資訊
# ========================================
message(STATUS "========================================")
message(STATUS "ElectronicATE Build Configuration")
message(STATUS "========================================")
message(STATUS "Qt Version: ${QT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source Dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary Dir: ${CMAKE_BINARY_DIR}")
message(STATUS "")
message(STATUS "Development Build:")
message(STATUS "  - XML folder will be copied to build directory")
message(STATUS "  - windeployqt will run automatically after build")
message(STATUS "")
message(STATUS "Release Deployment:")
message(STATUS "  1. cmake --build . --config Release")
message(STATUS "  2. cmake --install . --prefix ../deploy")
message(STATUS "  3. cd ../deploy && deploy_qt.bat")
message(STATUS "========================================")
