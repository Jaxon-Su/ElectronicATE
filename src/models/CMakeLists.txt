# Models are shared by the desktop application and offline tests. Keep this
# target independent of views, ViewModels, VISA implementations and Qt Widgets.
add_library(ElectronicATEModels STATIC
    mainwindow/mainwindowmodel.cpp mainwindow/mainwindowmodel.h
    instrumentsetup/instrumentsetupmodel.cpp instrumentsetup/instrumentsetupmodel.h
    testconditions/testconditionsmodel.cpp testconditions/testconditionsmodel.h
    manualcontrol/manualcontrolmodel.cpp manualcontrol/manualcontrolmodel.h
    commandconsole/commandconsolemodel.cpp commandconsole/commandconsolemodel.h
    testsequence/testsequencemodel.cpp testsequence/testsequencemodel.h
)
target_compile_features(ElectronicATEModels PUBLIC cxx_std_17)
set_target_properties(ElectronicATEModels PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEModels PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow
    ${CMAKE_CURRENT_SOURCE_DIR}/instrumentsetup
    ${CMAKE_CURRENT_SOURCE_DIR}/testconditions
    ${CMAKE_CURRENT_SOURCE_DIR}/manualcontrol
    ${CMAKE_CURRENT_SOURCE_DIR}/commandconsole
    ${CMAKE_CURRENT_SOURCE_DIR}/testsequence
)
target_link_libraries(ElectronicATEModels PUBLIC ElectronicATEData Qt6::Xml)
