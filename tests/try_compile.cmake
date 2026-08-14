# Wrapper script for expected-compile-failure tests.
# Removes cached object files before building so that source changes
# are always detected, then exits with the build's return code.

file(GLOB_RECURSE artifacts "${OBJ_DIR}/*.o" "${OBJ_DIR}/*.obj")
if(artifacts)
    file(REMOVE ${artifacts})
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" --target "${TARGET}" --config "${CONFIG}"
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Build failed")
endif()
