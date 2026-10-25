if(NOT DEFINED PROJECT_CLI OR NOT DEFINED WORK)
    message(FATAL_ERROR "PROJECT_CLI and WORK are required")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${PROJECT_CLI}" create "${WORK}" "ImportedGame"
    RESULT_VARIABLE status ERROR_VARIABLE issue)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Project creation failed: ${issue}")
endif()
set(game "${WORK}/ImportedGame")
file(WRITE "${game}/Assets/Sprites/item.ppm" "P3\n2 1\n255\n255 0 0  0 255 0\n")
execute_process(COMMAND "${PROJECT_CLI}" assets-build "${game}"
    RESULT_VARIABLE status OUTPUT_VARIABLE first ERROR_VARIABLE issue)
if(NOT status EQUAL 0 OR NOT first MATCHES "rebuilt")
    message(FATAL_ERROR "Cold asset import failed: ${first} ${issue}")
endif()
if(NOT EXISTS "${game}/.simpleengine/cache/windows-x64/artifact-map.tsv")
    message(FATAL_ERROR "Missing import map.")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-build "${game}"
    RESULT_VARIABLE status OUTPUT_VARIABLE second ERROR_VARIABLE issue)
if(NOT status EQUAL 0 OR NOT second MATCHES "0 rebuilt")
    message(FATAL_ERROR "Warm asset cache miss: ${second} ${issue}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-import-settings "${game}"
    "texture:sprites/item" "1" "1" "0"
    RESULT_VARIABLE status ERROR_VARIABLE issue)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Texture settings failed: ${issue}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-build "${game}"
    RESULT_VARIABLE status OUTPUT_VARIABLE rebuild ERROR_VARIABLE issue)
if(NOT status EQUAL 0 OR rebuild MATCHES "0 rebuilt")
    message(FATAL_ERROR "Changed settings did not recompile: ${rebuild} ${issue}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-clean "${game}"
    RESULT_VARIABLE status ERROR_VARIABLE issue)
if(NOT status EQUAL 0 OR
   EXISTS "${game}/.simpleengine/cache/windows-x64")
    message(FATAL_ERROR "Cache clean failed: ${issue}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-build "${game}" "../bad"
    RESULT_VARIABLE status)
if(status EQUAL 0)
    message(FATAL_ERROR "Unsafe platform token was accepted.")
endif()
file(REMOVE_RECURSE "${WORK}")
