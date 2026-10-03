# Production protocol drivers can be tested against ICommunication without VISA or Widgets.
add_library(ElectronicATEDrivers STATIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/msoconfiguration.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/msocapturemetadata.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/msomeasurement.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/msoperiodsession.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource/acsourcefactory.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource/chroma61505.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource/chroma61509.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource/chroma6530.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource/deltaa3000.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/chroma6310.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/chroma6310a.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/chroma63200a.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/chroma63600.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/chroma63804.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadfactory.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadspec/chromaload6310aspec.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadspec/chromaload6310spec.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadspec/chromaload63200aspec.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadspec/chromaload63600spec.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcsource/chroma62000.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcsource/dcsourcefactory.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcsource/dcsourcespec/chroma62000spec.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/msoseries456.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope/oscilloscopefactory.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/relay/relay_rtu_4.cpp"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/relay/relayfactory.cpp")
target_include_directories(ElectronicATEDrivers PUBLIC
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/acsource"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcload/dcloadspec"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcsource"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/dcsource/dcsourcespec"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/oscilloscope"
    "${ELECTRONICATE_SOURCE_ROOT}/hardware/instrument/relay")
target_link_libraries(ElectronicATEDrivers PUBLIC ElectronicATEInstrumentCore ElectronicATEData PRIVATE ElectronicATEStorage)
target_compile_features(ElectronicATEDrivers PUBLIC cxx_std_17)
