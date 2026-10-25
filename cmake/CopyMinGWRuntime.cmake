# Inspect PE imports just like the release workflow, and copy only DLLs from
# this compiler's bin directory. Windows system DLLs are never bundled.
cmake_minimum_required(VERSION 3.18)

# Executables share an output directory; serialize copies during parallel builds.
file(LOCK "${DESTINATION}/.runtime-dlls.lock" GUARD PROCESS TIMEOUT 60)
set(pending "${EXECUTABLE}")
set(visited)

while(pending)
    list(POP_FRONT pending binary)
    execute_process(
        COMMAND "${OBJDUMP}" -p "${binary}"
        RESULT_VARIABLE inspect_result
        OUTPUT_VARIABLE imports
        ERROR_VARIABLE inspect_error
    )
    if(NOT inspect_result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect ${binary}: ${inspect_error}")
    endif()

    string(REGEX MATCHALL "DLL Name: [^\r\n]+" dll_imports "${imports}")
    foreach(dll_import IN LISTS dll_imports)
        string(REPLACE "DLL Name: " "" dll_name "${dll_import}")
        string(STRIP "${dll_name}" dll_name)
        string(TOLOWER "${dll_name}" dll_key)
        set(dependency "${RUNTIME_DIRECTORY}/${dll_name}")
        if(dll_key IN_LIST visited OR NOT EXISTS "${dependency}")
            continue()
        endif()

        list(APPEND visited "${dll_key}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${dependency}" "${DESTINATION}"
            RESULT_VARIABLE copy_result
        )
        if(NOT copy_result EQUAL 0)
            message(FATAL_ERROR "Cannot copy runtime DLL: ${dependency}")
        endif()
        list(APPEND pending "${dependency}")
    endforeach()
endwhile()
