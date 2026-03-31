cmake_minimum_required(VERSION 3.21)

project(mutterkey LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(MUTTERKEY_LICENSE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MUTTERKEY_ENABLE_ASAN "Enable AddressSanitizer for repo-owned code and vendored whisper.cpp" OFF)
option(MUTTERKEY_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for repo-owned code and vendored whisper.cpp" OFF)
option(MUTTERKEY_ENABLE_LEGACY_WHISPER "Build the legacy whisper.cpp runtime for migration and parity validation" ON)
option(MUTTERKEY_ENABLE_WHISPER_CUDA "Enable whisper.cpp CUDA backend support (NVIDIA)" OFF)
option(MUTTERKEY_ENABLE_WHISPER_VULKAN "Enable whisper.cpp Vulkan backend support" OFF)
option(MUTTERKEY_ENABLE_WHISPER_BLAS "Enable whisper.cpp BLAS CPU acceleration" OFF)
set(MUTTERKEY_WHISPER_BLAS_VENDOR "Generic" CACHE STRING "BLAS vendor passed to whisper.cpp when BLAS acceleration is enabled")
set_property(CACHE MUTTERKEY_WHISPER_BLAS_VENDOR PROPERTY STRINGS "Generic;OpenBLAS;FLAME;ATLAS;FlexiBLAS;Intel;NVHPC;Apple")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia Network Widgets)
find_package(KF6GlobalAccel CONFIG REQUIRED)
find_package(KF6GuiAddons CONFIG REQUIRED)
find_package(Doxygen QUIET)

set(MUTTERKEY_CORE_SOURCES
    src/audio/audiorecorder.cpp
    src/audio/audiorecorder.h
    src/audio/recordingnormalizer.cpp
    src/audio/recordingnormalizer.h
    src/audio/recording.h
    src/clipboardwriter.cpp
    src/clipboardwriter.h
    src/commanddispatch.cpp
    src/commanddispatch.h
    src/config.cpp
    src/config.h
    src/hotkeymanager.cpp
    src/hotkeymanager.h
    src/service.cpp
    src/service.h
    src/transcription/transcriptiontypes.h
    src/transcription/transcriptionengine.cpp
    src/transcription/transcriptionengine.h
    src/transcription/audiochunker.cpp
    src/transcription/audiochunker.h
    src/transcription/cpudecoderforward.cpp
    src/transcription/cpudecoderforward.h
    src/transcription/cpudecoderruntime.cpp
    src/transcription/cpudecoderruntime.h
    src/transcription/cpudecodergraph.cpp
    src/transcription/cpudecodergraph.h
    src/transcription/cpuencoderforward.cpp
    src/transcription/cpuencoderforward.h
    src/transcription/cpufeatureextractor.cpp
    src/transcription/cpufeatureextractor.h
    src/transcription/cpugreedysearch.cpp
    src/transcription/cpugreedysearch.h
    src/transcription/cpumelspectrogram.cpp
    src/transcription/cpumelspectrogram.h
    src/transcription/cpumodelweights.cpp
    src/transcription/cpumodelweights.h
    src/transcription/cpureferencemodel.cpp
    src/transcription/cpureferencemodel.h
    src/transcription/cputensor.cpp
    src/transcription/cputensor.h
    src/transcription/cpureferencetranscriber.cpp
    src/transcription/cpureferencetranscriber.h
    src/transcription/cpusessionstate.cpp
    src/transcription/cpusessionstate.h
    src/transcription/cputimestamps.cpp
    src/transcription/cputimestamps.h
    src/transcription/cputokensearch.cpp
    src/transcription/cputokensearch.h
    src/transcription/cputokenizer.cpp
    src/transcription/cputokenizer.h
    src/transcription/cpuwhispertokenizer.cpp
    src/transcription/cpuwhispertokenizer.h
    src/transcription/modelcatalog.cpp
    src/transcription/modelcatalog.h
    src/transcription/modelpackage.cpp
    src/transcription/modelpackage.h
    src/transcription/modelvalidator.cpp
    src/transcription/modelvalidator.h
    src/transcription/rawwhisperimporter.cpp
    src/transcription/rawwhisperimporter.h
    src/transcription/rawwhisperprobe.cpp
    src/transcription/rawwhisperprobe.h
    src/transcription/runtimeselector.cpp
    src/transcription/runtimeselector.h
    src/transcription/transcriptassembler.cpp
    src/transcription/transcriptassembler.h
    src/transcription/transcriptioncompat.cpp
    src/transcription/transcriptioncompat.h
    src/transcription/transcriptionworker.cpp
    src/transcription/transcriptionworker.h
)

if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    list(APPEND MUTTERKEY_CORE_SOURCES
        src/transcription/whispercpptranscriber.cpp
        src/transcription/whispercpptranscriber.h
    )
endif()

set(MUTTERKEY_CONTROL_SOURCES
    src/control/daemoncontrolclient.cpp
    src/control/daemoncontrolclient.h
    src/control/daemoncontrolprotocol.cpp
    src/control/daemoncontrolprotocol.h
    src/control/daemoncontrolserver.cpp
    src/control/daemoncontrolserver.h
    src/control/daemoncontroltypes.cpp
    src/control/daemoncontroltypes.h
)

add_library(mutterkey_core STATIC ${MUTTERKEY_CORE_SOURCES})
add_library(mutterkey_control STATIC ${MUTTERKEY_CONTROL_SOURCES})
add_library(mutterkey_app STATIC
    src/app/applicationcommands.cpp
    src/app/applicationcommands.h
)

add_executable(mutterkey
    src/main.cpp
)

add_executable(mutterkey-tray
    src/tray/traystatuswindow.cpp
    src/tray/traystatuswindow.h
    src/traymain.cpp
)

target_include_directories(mutterkey_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mutterkey_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mutterkey_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mutterkey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mutterkey-tray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mutterkey_core PUBLIC Qt6::Core Qt6::Gui Qt6::Multimedia KF6::GlobalAccel KF6::GuiAddons)
target_link_libraries(mutterkey_control PUBLIC Qt6::Core Qt6::Network mutterkey_core)
target_link_libraries(mutterkey_app PUBLIC Qt6::Core Qt6::Gui mutterkey_control)
target_link_libraries(mutterkey PRIVATE mutterkey_app)
target_link_libraries(mutterkey-tray PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets mutterkey_control)
set_target_properties(mutterkey PROPERTIES
    BUILD_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN/../lib"
)
set_target_properties(mutterkey-tray PROPERTIES
    BUILD_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN/../lib"
)

if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    target_compile_definitions(mutterkey_core PRIVATE MUTTERKEY_WITH_LEGACY_WHISPER)
endif()

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

mutterkey_enable_sanitizers(mutterkey_core)
mutterkey_enable_sanitizers(mutterkey_control)
mutterkey_enable_sanitizers(mutterkey_app)
mutterkey_enable_sanitizers(mutterkey)
mutterkey_enable_sanitizers(mutterkey-tray)

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

if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/CMakeLists.txt")
        message(FATAL_ERROR "Vendored whisper.cpp dependency is missing from third_party/whisper.cpp")
    endif()

    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(WHISPER_SANITIZE_ADDRESS ${MUTTERKEY_ENABLE_ASAN} CACHE BOOL "" FORCE)
    set(WHISPER_SANITIZE_UNDEFINED ${MUTTERKEY_ENABLE_UBSAN} CACHE BOOL "" FORCE)
    set(GGML_CUDA ${MUTTERKEY_ENABLE_WHISPER_CUDA} CACHE BOOL "" FORCE)
    set(GGML_VULKAN ${MUTTERKEY_ENABLE_WHISPER_VULKAN} CACHE BOOL "" FORCE)
    set(GGML_BLAS ${MUTTERKEY_ENABLE_WHISPER_BLAS} CACHE BOOL "" FORCE)
    set(GGML_BLAS_VENDOR ${MUTTERKEY_WHISPER_BLAS_VENDOR} CACHE STRING "" FORCE)
    add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)

    # Mutterkey ships the vendored shared libraries, but it does not install their
    # upstream public headers as part of its own package layout.
    set_target_properties(whisper ggml PROPERTIES PUBLIC_HEADER "")

    target_link_libraries(mutterkey_core PRIVATE whisper)
endif()

install(TARGETS mutterkey RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS mutterkey-tray RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    install(TARGETS whisper ggml ggml-base
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    if(TARGET ggml-cpu)
        install(TARGETS ggml-cpu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    if(TARGET ggml-cuda)
        install(TARGETS ggml-cuda LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    if(TARGET ggml-vulkan)
        install(TARGETS ggml-vulkan LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    if(TARGET ggml-blas)
        install(TARGETS ggml-blas LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif()
install(FILES contrib/org.mutterkey.mutterkey.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${MUTTERKEY_LICENSE_INSTALL_DIR})
if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    install(FILES third_party/whisper.cpp/LICENSE DESTINATION ${MUTTERKEY_LICENSE_INSTALL_DIR}/third_party/whisper.cpp)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
