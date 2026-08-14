# --- Maximum-pedantry warnings, applied only to this project's own
# test targets (not forced onto downstream consumers of the header). ---

add_library(ctut_warnings INTERFACE)
target_compile_options(ctut_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual
        -Wnull-dereference -Wdouble-promotion -Wuseless-cast -Wlogical-op
        -Wduplicated-cond -Wduplicated-branches
        # See the matching Clang branch below for why: older GCC (10-13)
        # also flags a discarded consteval expect_* call as "statement
        # has no effect" on the std::span overloads specifically.
        -Wno-unused-value
        -Werror>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual
        -Wnull-dereference -Wdouble-promotion
        # Every expect_* is consteval, so Clang folds a passing call to
        # a compile-time-known bool and flags the discarded result the
        # same as a bare literal statement (GCC doesn't). Calling
        # expect_* as a bare statement is the intentional, documented
        # usage — the return value only ever carries information on
        # the failure path, which halts evaluation before returning.
        -Wno-unused-value
        -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# --- Positive tests ---

# basic_tests.cpp: compile-only (all tests are static_assert, no main)
add_library(basic_tests OBJECT basic_tests.cpp)
target_link_libraries(basic_tests PRIVATE compile_time_ut ctut_warnings)

# noexcept_tests.cpp: proves the whole compile-time expect_XXX surface
# compiles under -fno-exceptions. Compile-only, like basic_tests: every
# expect_* is consteval, so there's no genuine-runtime call path left
# to build an executable around. Uses whatever C++ standard the build
# is configured with.
add_library(noexcept_tests OBJECT noexcept_tests.cpp)
target_link_libraries(noexcept_tests PRIVATE compile_time_ut ctut_warnings)
target_compile_options(noexcept_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>)

# --- Expected-compile-failure test helper ---

function(add_compile_failure_test name source)
    cmake_parse_arguments(ACFT "" "" "LABELS;DEFINES" ${ARGN})
    add_library(${name} OBJECT EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${name} PRIVATE compile_time_ut ctut_warnings)
    if(ACFT_DEFINES)
        target_compile_definitions(${name} PRIVATE ${ACFT_DEFINES})
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DOBJ_DIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DTARGET=${name}
            -DCONFIG=$<CONFIG>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/try_compile.cmake)
    set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE)
    if(ACFT_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${ACFT_LABELS}")
    endif()
endfunction()

# --- Expected-compile-failure test helper (verifies diagnostic content) ---
#
# Like add_compile_failure_test, but additionally asserts that the
# compiler's diagnostic output contains every string in EXPECT_SUBSTRINGS
# — used to prove that expect_XXX argument values actually reach the
# compiler output, not just that the build fails.

function(add_compile_failure_test_with_values name source)
    cmake_parse_arguments(ACFTV "" "" "LABELS;DEFINES;EXPECT" ${ARGN})
    add_library(${name} OBJECT EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${name} PRIVATE compile_time_ut ctut_warnings)
    if(ACFTV_DEFINES)
        target_compile_definitions(${name} PRIVATE ${ACFTV_DEFINES})
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DOBJ_DIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DTARGET=${name}
            -DCONFIG=$<CONFIG>
            "-DEXPECT_SUBSTRINGS=${ACFTV_EXPECT}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/try_compile_grep.cmake)
    if(ACFTV_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${ACFTV_LABELS}")
    endif()
endfunction()

# --- Category A: NTTP expected failures ---

add_compile_failure_test(nttp_expect_true_false
    expected_failures/nttp_expect_true_false.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_true_0
    expected_failures/nttp_expect_true_0.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_false_true
    expected_failures/nttp_expect_false_true.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_false_1
    expected_failures/nttp_expect_false_1.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_eq_1_2
    expected_failures/nttp_expect_eq_1_2.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_eq_a_b
    expected_failures/nttp_expect_eq_a_b.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_ne_1_1
    expected_failures/nttp_expect_ne_1_1.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_ne_a_a
    expected_failures/nttp_expect_ne_a_a.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_lt_2_1
    expected_failures/nttp_expect_lt_2_1.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_le_2_1
    expected_failures/nttp_expect_le_2_1.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_gt_1_2
    expected_failures/nttp_expect_gt_1_2.cpp LABELS nttp)
add_compile_failure_test(nttp_expect_ge_1_2
    expected_failures/nttp_expect_ge_1_2.cpp LABELS nttp)

# --- Category B: val<> wrapper expected failures ---

add_compile_failure_test(val_expect_true_0
    expected_failures/val_expect_true_0.cpp LABELS val)
add_compile_failure_test(val_expect_false_1
    expected_failures/val_expect_false_1.cpp LABELS val)
add_compile_failure_test(val_expect_eq_6_5
    expected_failures/val_expect_eq_6_5.cpp LABELS val)
add_compile_failure_test(val_expect_ne_3_3
    expected_failures/val_expect_ne_3_3.cpp LABELS val)
add_compile_failure_test(val_expect_lt_3_2
    expected_failures/val_expect_lt_3_2.cpp LABELS val)
add_compile_failure_test(val_expect_ge_1_2
    expected_failures/val_expect_ge_1_2.cpp LABELS val)
# expect_near itself only exists when CTUT_HAS_FLOAT_NTTP; without it,
# these would still compile-fail (undeclared expect_near) and so still
# report as passing WILL_FAIL tests, but for the wrong reason. Skipped
# entirely instead so a real regression can't hide behind that.
if(CTUT_HAS_FLOAT_NTTP)
    add_compile_failure_test(nttp_expect_near_10_20_5
        expected_failures/nttp_expect_near_10_20_5.cpp LABELS nttp)
    add_compile_failure_test(val_expect_near_10_20_5
        expected_failures/val_expect_near_10_20_5.cpp LABELS val)
    add_compile_failure_test(nttp_expect_near_float_fail
        expected_failures/nttp_expect_near_float_fail.cpp LABELS nttp)
    add_compile_failure_test(val_expect_near_float_fail
        expected_failures/val_expect_near_float_fail.cpp LABELS val)
endif()

# --- Category D: Array size mismatch expected failures ---

add_compile_failure_test(array_eq_size_mismatch_str
    expected_failures/array_eq_size_mismatch_str.cpp LABELS array)
add_compile_failure_test(array_eq_size_mismatch_int
    expected_failures/array_eq_size_mismatch_int.cpp LABELS array)

# --- Category E: Array element mismatch expected failures ---

add_compile_failure_test(array_eq_mismatch_index0
    expected_failures/array_eq_mismatch_index0.cpp LABELS array)
add_compile_failure_test(array_eq_mismatch_index1
    expected_failures/array_eq_mismatch_index1.cpp LABELS array)
add_compile_failure_test(array_eq_mismatch_index2
    expected_failures/array_eq_mismatch_index2.cpp LABELS array)
add_compile_failure_test(array_eq_mismatch_large
    expected_failures/array_eq_mismatch_large.cpp LABELS array)

# --- Category F: Array ordering expected failures ---

add_compile_failure_test(array_lt_fail
    expected_failures/array_lt_fail.cpp LABELS array)
add_compile_failure_test(array_le_fail
    expected_failures/array_le_fail.cpp LABELS array)
add_compile_failure_test(array_gt_fail
    expected_failures/array_gt_fail.cpp LABELS array)
add_compile_failure_test(array_ge_fail
    expected_failures/array_ge_fail.cpp LABELS array)

# --- Category H: val<> case-2 expected failures (value content verified) ---

add_compile_failure_test_with_values(val_case2_expect_eq
    expected_failures/val_case2_expect_eq.cpp LABELS val
    EXPECT 7331 8442)
add_compile_failure_test_with_values(val_case2_expect_ge
    expected_failures/val_case2_expect_ge.cpp LABELS val
    EXPECT 6217 9999)
