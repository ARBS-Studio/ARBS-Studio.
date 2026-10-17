# =============================================================================
# ARBS Studio - testet e berthames (pa Archicad). Funksionon ne Windows/macOS/Linux.
#   cmake -S Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
# =============================================================================
cmake_minimum_required (VERSION 3.16)
project (ARBS_Core_Tests CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

file (GLOB CORE_SOURCES "${ROOT}/Src/Core/*.cpp")
add_library (arbs_core STATIC ${CORE_SOURCES})
target_include_directories (arbs_core PUBLIC "${ROOT}/Src")
if (MSVC)
    target_compile_options (arbs_core PRIVATE /W4 /utf-8)
else ()
    target_compile_options (arbs_core PRIVATE -Wall -Wextra -Wshadow -Werror)
endif ()

add_executable (arbs_tests TestMain.cpp)
target_link_libraries (arbs_tests arbs_core)
if (MSVC)
    target_compile_options (arbs_tests PRIVATE /utf-8)
endif ()

add_executable (arbs_samples SampleReports.cpp)
target_link_libraries (arbs_samples arbs_core)
if (MSVC)
    target_compile_options (arbs_samples PRIVATE /utf-8)
endif ()

enable_testing ()
add_test (NAME core COMMAND arbs_tests "${ROOT}/Data")
