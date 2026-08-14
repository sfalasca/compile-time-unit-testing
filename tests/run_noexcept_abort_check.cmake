# Runs a subprocess that is expected to abort() (no exceptions available)
# after printing a specific message, and reports PASS/FAIL via this
# script's own exit code — CTest treats a supervised child crashing as an
# unconditional failure, so the crashing process must run one level down.

execute_process(
    COMMAND ${EXE}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE proc_stdout
    ERROR_VARIABLE proc_stderr
)

set(combined_output "${proc_stdout}${proc_stderr}")

string(FIND "${combined_output}" "${EXPECT_MESSAGE}" pos)
if(pos EQUAL -1)
    message(FATAL_ERROR
        "Expected abort message '${EXPECT_MESSAGE}' not found.\n"
        "--- process output ---\n${combined_output}")
endif()

if(result EQUAL 0)
    message(FATAL_ERROR
        "Process exited cleanly (0); expected it to abort() via fail_at_runtime().")
endif()
