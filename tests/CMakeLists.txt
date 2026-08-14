# --- Maximum-pedantry warnings, applied only to this project's own
# test targets (not forced onto downstream consumers of the header). ---

add_library(ctut_warnings INTERFACE)
target_compile_options(ctut_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual
        -Wnull-dereference -Wdouble-promotion -Wuseless-cast -Wlogical-op
        -Wduplicated-cond -Wduplicated-branches -Werror>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual
        -Wnull-dereference -Wdouble-promotion -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# --- Positive tests ---

# basic_tests.cpp: compile-only (all tests are static_assert, no main)
add_library(basic_tests OBJECT basic_tests.cpp)
target_link_libraries(basic_tests PRIVATE compile_time_ut ctut_warnings)

# precondition_tests.cpp: executable with runtime tests
add_executable(precondition_tests precondition_tests.cpp)
target_link_libraries(precondition_tests PRIVATE compile_time_ut ctut_warnings)
add_test(NAME precondition_tests COMMAND precondition_tests)

# noexcept_tests.cpp: proves the whole header (expect_XXX, CTUT_EXPECT_XXX,
# preconditions/postconditions/invariants) compiles under -fno-exceptions,
# and that a genuine runtime failure reports + aborts instead of relying
# on throw/catch. Uses whatever C++ standard the build is configured
# with (no forced cxx_std_23: none of this needs <format>, which is used
# opportunistically when available).
add_executable(noexcept_tests noexcept_tests.cpp)
target_link_libraries(noexcept_tests PRIVATE compile_time_ut ctut_warnings)
target_compile_options(noexcept_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>)
add_test(NAME noexcept_tests
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:noexcept_tests>
        # "expect_eq failed" is always printed; the ": 9 vs 8" value
        # suffix only appears when <format>/std::formattable are
        # available (C++23), so the check below only requires the
        # substring common to both outputs.
        "-DEXPECT_MESSAGE=expect_eq failed"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_noexcept_abort_check.cmake)

# thread_safety_tests.cpp: stresses detail::violation_handler_ref()
# (std::atomic<ViolationHandler>) with concurrent set_violation_handler()
# writers and concurrent contract-violation readers under
# ThreadSanitizer. A clean run (no "data race" report, exit 0) is the
# pass criterion; verified manually (see the file's trailing comment)
# that reverting the atomic to a bare pointer makes TSan catch it
# within the first few iterations.
add_executable(thread_safety_tests thread_safety_tests.cpp)
target_link_libraries(thread_safety_tests PRIVATE compile_time_ut ctut_warnings)
find_package(Threads REQUIRED)
target_link_libraries(thread_safety_tests PRIVATE Threads::Threads)
target_compile_options(thread_safety_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=thread -g -O1>)
target_link_options(thread_safety_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=thread>)
add_test(NAME thread_safety_tests COMMAND thread_safety_tests)

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

# --- Category F: Array ordering expected failures ---

add_compile_failure_test(array_lt_fail
    expected_failures/array_lt_fail.cpp LABELS array)
add_compile_failure_test(array_le_fail
    expected_failures/array_le_fail.cpp LABELS array)
add_compile_failure_test(array_gt_fail
    expected_failures/array_gt_fail.cpp LABELS array)
add_compile_failure_test(array_ge_fail
    expected_failures/array_ge_fail.cpp LABELS array)

# --- Category G: Contract expected failures ---

add_compile_failure_test(contract_precondition_safe
    expected_failures/contract_precondition_safe.cpp LABELS contract)
add_compile_failure_test(contract_precondition
    expected_failures/contract_precondition.cpp LABELS contract)
add_compile_failure_test(contract_precondition_aggressive
    expected_failures/contract_precondition_aggressive.cpp LABELS contract)
add_compile_failure_test(contract_postcondition_safe
    expected_failures/contract_postcondition_safe.cpp LABELS contract)
add_compile_failure_test(contract_postcondition
    expected_failures/contract_postcondition.cpp LABELS contract)
add_compile_failure_test(contract_postcondition_aggressive
    expected_failures/contract_postcondition_aggressive.cpp LABELS contract)
add_compile_failure_test(contract_invariant_safe
    expected_failures/contract_invariant_safe.cpp LABELS contract)
add_compile_failure_test(contract_invariant
    expected_failures/contract_invariant.cpp LABELS contract)
add_compile_failure_test(contract_invariant_aggressive
    expected_failures/contract_invariant_aggressive.cpp LABELS contract)

# --- Category H: CTUT_EXPECT_XXX macro expected failures (value content verified) ---

add_compile_failure_test_with_values(ctut_case2_expect_eq
    expected_failures/ctut_case2_expect_eq.cpp LABELS ctut
    EXPECT 7331 8442)
add_compile_failure_test_with_values(ctut_case2_expect_true
    expected_failures/ctut_case2_expect_true.cpp LABELS ctut
    EXPECT 6217 9999)
