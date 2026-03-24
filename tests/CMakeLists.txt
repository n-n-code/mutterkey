function(mutterkey_add_qt_test target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(${target_name} PRIVATE Qt6::Core Qt6::Multimedia Qt6::Test)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

mutterkey_add_qt_test(configtest
    configtest.cpp
    ../src/config.cpp
)

mutterkey_add_qt_test(commanddispatchtest
    commanddispatchtest.cpp
    ../src/commanddispatch.cpp
    ../src/config.cpp
)

mutterkey_add_qt_test(recordingnormalizertest
    recordingnormalizertest.cpp
    ../src/audio/recordingnormalizer.cpp
)

target_link_libraries(configtest PRIVATE whisper)
target_link_libraries(commanddispatchtest PRIVATE whisper)

if(TARGET clang-tidy)
    add_dependencies(clang-tidy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen)
endif()

if(TARGET clazy)
    add_dependencies(clazy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen)
endif()
