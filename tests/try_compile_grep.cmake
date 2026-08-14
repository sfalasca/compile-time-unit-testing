# Wrapper script for expected-compile-failure tests that also verify the
# compiler diagnostic actually contains specific substrings (e.g. the
# real argument values of a failed expect_XXX assertion). Removes cached
# object files first so source changes are always detected.

file(GLOB_RECURSE artifacts "${OBJ_DIR}/*.o" "${OBJ_DIR}/*.obj")
if(artifacts)
    file(REMOVE ${artifacts})
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" --target "${TARGET}" --config "${CONFIG}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)

if(result EQUAL 0)
    message(FATAL_ERROR "Build unexpectedly succeeded (expected a compile failure)")
endif()

set(combined_output "${build_stdout}${build_stderr}")

foreach(needle IN LISTS EXPECT_SUBSTRINGS)
    string(FIND "${combined_output}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR
            "Expected substring '${needle}' not found in compiler output.\n"
            "--- compiler output ---\n${combined_output}")
    endif()
endforeach()
