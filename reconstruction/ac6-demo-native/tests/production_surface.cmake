execute_process(
    COMMAND "${AC6DEMO_NM}" -C "${AC6DEMO_CORE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "production symbol audit failed: ${nm_error}")
endif()
if(nm_output MATCHES "ac6demo_native::testing|import_fixture|verify_fixture")
    message(FATAL_ERROR "fixture API leaked into production core")
endif()

execute_process(
    COMMAND "${AC6DEMO_CLI}" import /definitely-missing --profile relaxed
    RESULT_VARIABLE cli_result
    OUTPUT_VARIABLE cli_output
    ERROR_VARIABLE cli_error
)
if(cli_result EQUAL 0)
    message(FATAL_ERROR "production CLI accepted a profile override")
endif()
if(NOT "${cli_output}${cli_error}" MATCHES "usage:")
    message(FATAL_ERROR "unexpected CLI response to profile override: ${cli_output}${cli_error}")
endif()
