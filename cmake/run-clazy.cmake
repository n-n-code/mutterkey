if(NOT DEFINED TOOL_BIN OR TOOL_BIN STREQUAL "" OR TOOL_BIN MATCHES "-NOTFOUND$")
    message(FATAL_ERROR "clazy target requested, but clazy-standalone was not found in PATH")
endif()

if(NOT DEFINED BUILD_DIR OR NOT EXISTS "${BUILD_DIR}/compile_commands.json")
    message(FATAL_ERROR "compile_commands.json is missing; reconfigure the build directory first")
endif()

if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR was not provided")
endif()

if(NOT DEFINED CHECKS OR CHECKS STREQUAL "")
    message(FATAL_ERROR "No clazy checks were configured")
endif()

set(compile_commands_path "${BUILD_DIR}/compile_commands.json")
file(READ "${compile_commands_path}" compile_commands_json)
string(REPLACE " -mno-direct-extern-access" "" compile_commands_json "${compile_commands_json}")

set(clazy_build_dir "${BUILD_DIR}/clazy")
file(MAKE_DIRECTORY "${clazy_build_dir}")
file(WRITE "${clazy_build_dir}/compile_commands.json" "${compile_commands_json}")

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/tests/*.cpp"
)

set(failed FALSE)
foreach(source_file IN LISTS SOURCE_FILES)
    string(FIND "${compile_commands_json}" "\"file\": \"${source_file}\"" compile_command_index)
    if(compile_command_index EQUAL -1)
        message(STATUS "Skipping clazy-standalone for ${source_file} because it is not part of the configured build")
        continue()
    endif()

    message(STATUS "Running clazy-standalone on ${source_file}")
    execute_process(
        COMMAND "${TOOL_BIN}"
            -p "${clazy_build_dir}"
            "-checks=${CHECKS}"
            "--ignore-dirs=.*/(third_party|[^/]*_autogen)/.*"
            "${source_file}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        set(failed TRUE)
    endif()
endforeach()

if(failed)
    message(FATAL_ERROR "clazy-standalone reported failures")
endif()
