# =============================================================================
# ARBS Studio - Add-On per Archicad 28, 29 dhe me te reja
# Bazuar ne GRAPHISOFT/archicad-addon-cmake (submoduli Tools/)
#
#   python Tools/BuildAddOn.py --configFile config.json --acVersion 28 29
#
# Ose me DevKit lokal:
#   cmake -B Build -G "Visual Studio 17 2022" -A x64 -T v142 -DAC_VERSION=28 -DAC_API_DEVKIT_DIR=<DevKit>/Support
# =============================================================================
cmake_minimum_required (VERSION 3.19)

include (Tools/CMakeCommon.cmake)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)
set (AC_API_DEVKIT_DIR "$ENV{AC_API_DEVKIT_DIR}" CACHE PATH "API DevKit directory.")

if (NOT DEFINED AC_VERSION)
    message (FATAL_ERROR "AC_VERSION nuk eshte vendosur (p.sh. -DAC_VERSION=28)")
endif ()
if (AC_VERSION LESS 28)
    message (WARNING "ARBS Studio eshte projektuar per Archicad 28 e me te ri (AC_VERSION=${AC_VERSION}).")
endif ()

message (STATUS "Archicad Version: ${AC_VERSION}")
SetGlobalCompilerDefinitions (${AC_VERSION})

ReadConfigJson ()
set (AC_ADDON_LANGUAGE "${addOnDefaultLanguage}" CACHE STRING "Add-On language code.")

project (${addOnName}-${AC_VERSION})

set (AC_ADDON_FOR_DISTRIBUTION OFF CACHE BOOL "")
# MDID (Developer ID / Local ID) vendoset ne Src/ResourceIds.hpp - shih Docs/Instalimi.md

set (AddOnSourcesFolder Src)
set (AddOnResourcesFolder .)
set (PCH_HEADER "Src/ArbsPrecompiledHeader.hpp")

# --- te dhenat e ngulitura (Data/*.json, UI, logo) ----------------------------
set (ARBS_GENERATED_DIR "${CMAKE_BINARY_DIR}/Generated")
set (ARBS_EMBEDDED_CPP "${ARBS_GENERATED_DIR}/EmbeddedData.cpp")
file (MAKE_DIRECTORY "${ARBS_GENERATED_DIR}")
file (GLOB ARBS_EMBED_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Data/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/UI/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.png"
)
# gjenerim fillestar gjate konfigurimit + rigjenerim kur ndryshojne te dhenat
execute_process (COMMAND ${CMAKE_COMMAND} -DOUTPUT=${ARBS_EMBEDDED_CPP} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFiles.cmake)
add_custom_command (
    OUTPUT "${ARBS_EMBEDDED_CPP}"
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${ARBS_EMBEDDED_CPP} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFiles.cmake
    DEPENDS ${ARBS_EMBED_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFiles.cmake"
    COMMENT "ARBS Studio: ngulitja e te dhenave"
)

GenerateAddOnProject (ArbsStudio ${AC_VERSION} "${AC_API_DEVKIT_DIR}" ${AddOnSourcesFolder} ${AddOnResourcesFolder} ${AC_ADDON_LANGUAGE} ${addOnDefaultLanguage} ${PCH_HEADER})

target_sources (ArbsStudio PRIVATE "${ARBS_EMBEDDED_CPP}")
set_source_files_properties ("${ARBS_EMBEDDED_CPP}" PROPERTIES GENERATED ON)
target_include_directories (ArbsStudio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Src")

# Berthama (Src/Core) eshte kod standard C++17, i testuar ne Linux/Windows/macOS.
# Paralajmerimet e konvertimeve numerike nuk trajtohen si gabime per te.
file (GLOB ARBS_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Src/Core/*.cpp")
if (MSVC)
    set_source_files_properties (${ARBS_CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4267;/wd4456;/wd4457;/wd4458;/wd4459;/wd4100;/wd4189;/wd4702")
else ()
    set_source_files_properties (${ARBS_CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-shadow;-Wno-conversion;-Wno-unused-function")
endif ()
