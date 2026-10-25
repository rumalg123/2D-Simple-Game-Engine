# Exercise the public, headless asset CLI from an actual generated project.
if(NOT DEFINED PROJECT_CLI OR NOT DEFINED WORK)
    message(FATAL_ERROR "PROJECT_CLI and WORK are required")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${PROJECT_CLI}" create "${WORK}" "AssetGame"
    RESULT_VARIABLE code ERROR_VARIABLE log)
if(NOT code EQUAL 0) 
    message(FATAL_ERROR "Cannot create test project: ${log}")
endif()
set(game "${WORK}/AssetGame")
file(WRITE "${game}/Assets/Sprites/icon.ppm" "P3\n1 1\n255\n15 20 30\n")
execute_process(COMMAND "${PROJECT_CLI}" assets-sync "${game}"
    RESULT_VARIABLE code OUTPUT_VARIABLE log ERROR_VARIABLE err)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Asset sync rejected generated project: ${log} ${err}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-audit "${game}"
    RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Asset audit rejected valid project: ${err}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-list "${game}"
    OUTPUT_VARIABLE entries RESULT_VARIABLE code)
if(NOT code EQUAL 0 OR NOT entries MATCHES "texture:sprites/icon")
    message(FATAL_ERROR "Catalog list did not include imported texture.")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-move "${game}"
    "texture:sprites/icon" "Sprites/renamed.ppm"
    RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Managed asset CLI move failed: ${err}")
endif()
if(EXISTS "${game}/Assets/Sprites/icon.ppm" OR
   NOT EXISTS "${game}/Assets/Sprites/renamed.ppm")
    message(FATAL_ERROR "Managed move did not rename the source file.")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-audit "${game}"
    RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Catalog audit broke after asset move: ${err}")
endif()
execute_process(COMMAND "${PROJECT_CLI}" assets-move "${game}"
    "texture:sprites/icon" "../escape.ppm"
    RESULT_VARIABLE badCode ERROR_VARIABLE err)
if(badCode EQUAL 0)
    message(FATAL_ERROR "Asset CLI accepted path traversal.")
endif()
file(REMOVE "${game}/Assets/Sprites/renamed.ppm")
execute_process(COMMAND "${PROJECT_CLI}" assets-audit "${game}"
    RESULT_VARIABLE badCode ERROR_VARIABLE err)
if(badCode EQUAL 0)
    message(FATAL_ERROR "Asset audit failed to report missing source.")
endif()
file(REMOVE_RECURSE "${WORK}")
