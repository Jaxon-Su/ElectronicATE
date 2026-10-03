# Common value types and read-only contracts require QtCore only.
add_library(ElectronicATEData INTERFACE)
target_compile_features(ElectronicATEData INTERFACE cxx_std_17)
target_include_directories(ElectronicATEData INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ElectronicATEData INTERFACE Qt6::Core)
