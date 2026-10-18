# Shared by the application UI and Windows executable metadata.
set(ELECTRONICATE_VERSION_NUMBER "0.1.0")
set(ELECTRONICATE_VERSION_SUFFIX "-alpha.2")
set(ELECTRONICATE_VERSION "${ELECTRONICATE_VERSION_NUMBER}${ELECTRONICATE_VERSION_SUFFIX}")
set(ELECTRONICATE_AUTHOR "Jaxon Su")
set(ELECTRONICATE_GITHUB_URL "https://github.com/Jaxon-Su/ElectronicATE")

# VERSIONINFO requires four numeric components; the string retains the prerelease label.
string(REPLACE "." "," ELECTRONICATE_WINDOWS_VERSION "${ELECTRONICATE_VERSION_NUMBER}.0")
set(ELECTRONICATE_VERSION_FLAGS 0)
if(NOT ELECTRONICATE_VERSION_SUFFIX STREQUAL "")
    set(ELECTRONICATE_VERSION_FLAGS 0x2) # VS_FF_PRERELEASE
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/applicationinfo.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/applicationinfo.h" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/applicationinfo.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/applicationinfo.rc" @ONLY)
