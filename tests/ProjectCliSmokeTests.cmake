# End-to-end headless workflow exercised through the built CLI executable.
if(NOT DEFINED PROJECT_TOOL OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "PROJECT_TOOL and TEST_DIR are required")
endif()
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")

execute_process(
    COMMAND "${PROJECT_TOOL}" create "${TEST_DIR}" "LuaStarter"
    RESULT_VARIABLE create_code OUTPUT_VARIABLE create_output ERROR_VARIABLE create_error
)
if(NOT create_code EQUAL 0)
    message(FATAL_ERROR "Project create failed: ${create_error}")
endif()

execute_process(
    COMMAND "${PROJECT_TOOL}" validate "${TEST_DIR}/LuaStarter"
    RESULT_VARIABLE valid_code OUTPUT_VARIABLE valid_output ERROR_VARIABLE valid_error
)
if(NOT valid_code EQUAL 0 OR NOT valid_output MATCHES "Lua script")
    message(FATAL_ERROR "Generated project validation failed: ${valid_error} ${valid_output}")
endif()

file(WRITE "${TEST_DIR}/LuaStarter/Assets/Scripts/Broken.lua"
     "function broken( this is not valid Lua\n")
execute_process(
    COMMAND "${PROJECT_TOOL}" validate "${TEST_DIR}/LuaStarter"
    RESULT_VARIABLE bad_code OUTPUT_VARIABLE bad_output ERROR_VARIABLE bad_error
)
if(bad_code EQUAL 0 OR NOT bad_error MATCHES "Validation:")
    message(FATAL_ERROR "Invalid Lua script was not rejected by CLI.")
endif()
file(REMOVE_RECURSE "${TEST_DIR}")
