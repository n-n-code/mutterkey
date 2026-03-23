cmake_minimum_required(VERSION 3.21)

project(mutterkey LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(MUTTERKEY_LICENSE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MUTTERKEY_ENABLE_ASAN "Enable AddressSanitizer for repo-owned code and vendored whisper.cpp" OFF)
option(MUTTERKEY_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for repo-owned code and vendored whisper.cpp" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia)
find_package(KF6GlobalAccel CONFIG REQUIRED)
find_package(KF6GuiAddons CONFIG REQUIRED)
find_package(Doxygen QUIET)

set(MUTTERKEY_APP_SOURCES
    src/audio/audiorecorder.cpp
    src/audio/audiorecorder.h
    src/audio/recordingnormalizer.cpp
    src/audio/recordingnormalizer.h
    src/audio/recording.h
    src/clipboardwriter.cpp
    src/clipboardwriter.h
    src/config.cpp
    src/config.h
    src/hotkeymanager.cpp
    src/hotkeymanager.h
    src/main.cpp
    src/service.cpp
    src/service.h
    src/transcription/transcriptiontypes.h
    src/transcription/transcriptionworker.cpp
    src/transcription/transcriptionworker.h
    src/transcription/whispercpptranscriber.cpp
    src/transcription/whispercpptranscriber.h
)

add_executable(mutterkey ${MUTTERKEY_APP_SOURCES})

target_include_directories(mutterkey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mutterkey PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia KF6::GlobalAccel KF6::GuiAddons)
set_target_properties(mutterkey PROPERTIES
    BUILD_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN/../lib"
)

function(mutterkey_enable_sanitizers target_name)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(WARNING "Sanitizers were requested, but ${CMAKE_CXX_COMPILER_ID} is not configured for repo-owned sanitizer flags")
        return()
    endif()

    set(sanitizer_compile_flags "")
    set(sanitizer_link_flags "")

    if(MUTTERKEY_ENABLE_ASAN)
        list(APPEND sanitizer_compile_flags -fsanitize=address -fno-omit-frame-pointer)
        list(APPEND sanitizer_link_flags -fsanitize=address)
    endif()

    if(MUTTERKEY_ENABLE_UBSAN)
        list(APPEND sanitizer_compile_flags -fsanitize=undefined)
        list(APPEND sanitizer_link_flags -fsanitize=undefined)
    endif()

    if(sanitizer_compile_flags)
        target_compile_options(${target_name} PRIVATE ${sanitizer_compile_flags})
        target_link_options(${target_name} PRIVATE ${sanitizer_link_flags})
    endif()
endfunction()

mutterkey_enable_sanitizers(mutterkey)

set(MUTTERKEY_CLAZY_CHECKS "level0" CACHE STRING "Checks passed to clazy-standalone")

find_program(MUTTERKEY_CLANG_TIDY_BIN NAMES clang-tidy)
find_program(MUTTERKEY_CLAZY_BIN NAMES clazy-standalone)

add_custom_target(clang-tidy
    COMMAND ${CMAKE_COMMAND}
        -DTOOL_BIN=${MUTTERKEY_CLANG_TIDY_BIN}
        -DTOOL_NAME=clang-tidy
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DCONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-clang-tidy.cmake
    USES_TERMINAL
    VERBATIM
)

add_custom_target(clazy
    COMMAND ${CMAKE_COMMAND}
        -DTOOL_BIN=${MUTTERKEY_CLAZY_BIN}
        -DTOOL_NAME=clazy-standalone
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DCHECKS=${MUTTERKEY_CLAZY_CHECKS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run-clazy.cmake
    USES_TERMINAL
    VERBATIM
)

add_custom_target(lint)
add_dependencies(lint clang-tidy clazy)

add_custom_target(valgrind
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-valgrind.sh ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    VERBATIM
)

if(DOXYGEN_FOUND)
    set(MUTTERKEY_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    set(MUTTERKEY_DOXYGEN_CONFIG "${CMAKE_BINARY_DIR}/Doxyfile")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
        ${MUTTERKEY_DOXYGEN_CONFIG}
        @ONLY
    )

    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MUTTERKEY_DOXYGEN_OUTPUT_DIR}
        COMMAND ${DOXYGEN_EXECUTABLE} ${MUTTERKEY_DOXYGEN_CONFIG}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(STATUS "Doxygen not found; the docs target will be unavailable")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/CMakeLists.txt")
    message(FATAL_ERROR "Vendored whisper.cpp dependency is missing from third_party/whisper.cpp")
endif()

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_SANITIZE_ADDRESS ${MUTTERKEY_ENABLE_ASAN} CACHE BOOL "" FORCE)
set(WHISPER_SANITIZE_UNDEFINED ${MUTTERKEY_ENABLE_UBSAN} CACHE BOOL "" FORCE)
add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)

# Mutterkey ships the vendored shared libraries, but it does not install their
# upstream public headers as part of its own package layout.
set_target_properties(whisper ggml PROPERTIES PUBLIC_HEADER "")

target_link_libraries(mutterkey PRIVATE whisper)

install(TARGETS mutterkey RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS whisper ggml ggml-base
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(TARGET ggml-cpu)
    install(TARGETS ggml-cpu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(FILES contrib/org.mutterkey.mutterkey.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${MUTTERKEY_LICENSE_INSTALL_DIR})
install(FILES third_party/whisper.cpp/LICENSE DESTINATION ${MUTTERKEY_LICENSE_INSTALL_DIR}/third_party/whisper.cpp)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
