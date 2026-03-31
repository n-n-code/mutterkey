if(NOT DEFINED TOOL_BIN OR TOOL_BIN STREQUAL "" OR TOOL_BIN MATCHES "-NOTFOUND$")
    message(FATAL_ERROR "clang-tidy target requested, but clang-tidy was not found in PATH")
endif()

if(NOT DEFINED BUILD_DIR OR NOT EXISTS "${BUILD_DIR}/compile_commands.json")
    message(FATAL_ERROR "compile_commands.json is missing; reconfigure the build directory first")
endif()

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR was not provided")
endif()

if(NOT DEFINED CONFIG_FILE OR NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR "clang-tidy config file was not found")
endif()

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/tests/*.cpp"
)

list(LENGTH SOURCE_FILES source_file_count)
if(source_file_count EQUAL 0)
    message(FATAL_ERROR "No source files matched the clang-tidy target")
endif()

list(GET SOURCE_FILES 0 first_source_file)
execute_process(
    COMMAND "${TOOL_BIN}"
        -p "${BUILD_DIR}"
        --quiet
        --config-file=${CONFIG_FILE}
        --verify-config
        "${first_source_file}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE verify_result
)
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR "clang-tidy configuration verification failed")
endif()

file(READ "${BUILD_DIR}/compile_commands.json" COMPILE_COMMANDS_JSON)

set(failed FALSE)
foreach(source_file IN LISTS SOURCE_FILES)
    string(FIND "${COMPILE_COMMANDS_JSON}" "\"file\": \"${source_file}\"" compile_command_index)
    if(compile_command_index EQUAL -1)
        message(STATUS "Skipping clang-tidy for ${source_file} because it is not part of the configured build")
        continue()
    endif()

    message(STATUS "Running clang-tidy on ${source_file}")
    execute_process(
        COMMAND "${TOOL_BIN}"
            -p "${BUILD_DIR}"
            --quiet
            --config-file=${CONFIG_FILE}
            "${source_file}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        set(failed TRUE)
    endif()
endforeach()

if(failed)
    message(FATAL_ERROR "clang-tidy reported failures")
endif()
