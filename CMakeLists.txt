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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia)
find_package(KF6GlobalAccel CONFIG REQUIRED)
find_package(KF6GuiAddons CONFIG REQUIRED)

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

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/CMakeLists.txt")
    message(FATAL_ERROR "Vendored whisper.cpp dependency is missing from third_party/whisper.cpp")
endif()

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)
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
