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

set(compile_commands_path "${BUILD_DIR}/compile_commands.json")
file(READ "${compile_commands_path}" compile_commands_json)
string(REPLACE " -mno-direct-extern-access" "" compile_commands_json "${compile_commands_json}")

set(tidy_build_dir "${BUILD_DIR}/clang-tidy")
file(MAKE_DIRECTORY "${tidy_build_dir}")
file(WRITE "${tidy_build_dir}/compile_commands.json" "${compile_commands_json}")

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/tests/*.cpp"
)

set(failed FALSE)
foreach(source_file IN LISTS SOURCE_FILES)
    message(STATUS "Running clang-tidy on ${source_file}")
    execute_process(
        COMMAND "${TOOL_BIN}"
            -p "${tidy_build_dir}"
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
