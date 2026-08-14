cmake_minimum_required(VERSION 3.20)
project(compile_time_ut LANGUAGES CXX)

add_library(compile_time_ut INTERFACE)
target_include_directories(compile_time_ut INTERFACE include)
target_compile_features(compile_time_ut INTERFACE cxx_std_20)

# --- Static analysis (opt-in; auto-skips when a tool isn't installed) ---

option(CTUT_ENABLE_CLANG_TIDY "Run clang-tidy on every compiled target" OFF)
if(CTUT_ENABLE_CLANG_TIDY)
    find_program(CTUT_CLANG_TIDY_EXE
        NAMES clang-tidy-21 clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy-17 clang-tidy
        HINTS /usr/lib/llvm-21/bin /usr/lib/llvm-20/bin /usr/lib/llvm-19/bin)
    if(CTUT_CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CTUT_CLANG_TIDY_EXE}")
    else()
        message(WARNING "CTUT_ENABLE_CLANG_TIDY=ON but no clang-tidy executable was found; skipping")
    endif()
endif()

option(CTUT_ENABLE_CPPCHECK "Run cppcheck on every compiled target" OFF)
if(CTUT_ENABLE_CPPCHECK)
    find_program(CTUT_CPPCHECK_EXE NAMES cppcheck)
    if(CTUT_CPPCHECK_EXE)
        set(CMAKE_CXX_CPPCHECK
            "${CTUT_CPPCHECK_EXE}"
            "--enable=warning,style,performance,portability"
            "--std=c++20"
            "--inline-suppr"
            "--suppress=missingIncludeSystem"
            "--error-exitcode=1")
    else()
        message(WARNING "CTUT_ENABLE_CPPCHECK=ON but no cppcheck executable was found; skipping")
    endif()
endif()

# Whole-build Clang Static Analyzer pass. Unlike the two toggles above,
# this only needs clang itself (not the separate clang-tidy/cppcheck
# packages), so it's registered whenever scan-build is found rather than
# gated behind an option. Run it explicitly: cmake --build . --target
# static_analysis.
find_program(CTUT_SCAN_BUILD_EXE
    NAMES scan-build-21 scan-build-20 scan-build-19 scan-build
    HINTS /usr/lib/llvm-21/bin /usr/lib/llvm-20/bin /usr/lib/llvm-19/bin)
if(CTUT_SCAN_BUILD_EXE)
    add_custom_target(static_analysis
        COMMAND ${CTUT_SCAN_BUILD_EXE} --status-bugs
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all --clean-first
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running the Clang Static Analyzer (scan-build) over the full build"
        USES_TERMINAL)
endif()

enable_testing()
add_subdirectory(tests)
