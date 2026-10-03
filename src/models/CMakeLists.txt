# Models are shared by the desktop application and offline tests. Keep this
# target independent of views, ViewModels, VISA implementations and Qt Widgets.
add_library(ElectronicATEModels STATIC
    mainwindow/mainwindowmodel.cpp mainwindow/mainwindowmodel.h
    page1/page1model.cpp page1/page1model.h
    page2/page2model.cpp page2/page2model.h
    page3/page3model.cpp page3/page3model.h
    page4/page4model.cpp page4/page4model.h
    page5/page5model.cpp page5/page5model.h
)
target_compile_features(ElectronicATEModels PUBLIC cxx_std_17)
set_target_properties(ElectronicATEModels PROPERTIES AUTOMOC ON)
target_include_directories(ElectronicATEModels PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow
    ${CMAKE_CURRENT_SOURCE_DIR}/page1
    ${CMAKE_CURRENT_SOURCE_DIR}/page2
    ${CMAKE_CURRENT_SOURCE_DIR}/page3
    ${CMAKE_CURRENT_SOURCE_DIR}/page4
    ${CMAKE_CURRENT_SOURCE_DIR}/page5
)
target_link_libraries(ElectronicATEModels PUBLIC ElectronicATEData Qt6::Xml)
