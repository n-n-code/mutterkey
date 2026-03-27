function(mutterkey_add_qt_test target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(${target_name} PRIVATE Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Test Qt6::Network Qt6::Widgets)
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
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

mutterkey_add_qt_test(streamingtranscriptiontest
    streamingtranscriptiontest.cpp
    ../src/audio/recordingnormalizer.cpp
    ../src/transcription/audiochunker.cpp
    ../src/transcription/transcriptassembler.cpp
    ../src/transcription/transcriptioncompat.cpp
)

mutterkey_add_qt_test(daemoncontrolprotocoltest
    daemoncontrolprotocoltest.cpp
    ../src/control/daemoncontrolprotocol.cpp
)

mutterkey_add_qt_test(daemoncontroltypestest
    daemoncontroltypestest.cpp
    ../src/config.cpp
    ../src/control/daemoncontroltypes.cpp
)

mutterkey_add_qt_test(daemoncontrolclientservertest
    daemoncontrolclientservertest.cpp
)

mutterkey_add_qt_test(transcriptionworkertest
    transcriptionworkertest.cpp
    ../src/audio/recordingnormalizer.cpp
    ../src/transcription/audiochunker.cpp
    ../src/transcription/transcriptassembler.cpp
    ../src/transcription/transcriptioncompat.cpp
    ../src/transcription/transcriptionworker.cpp
)

mutterkey_add_qt_test(whispercpptranscribertest
    whispercpptranscribertest.cpp
    ../src/transcription/transcriptionengine.cpp
    ../src/transcription/whispercpptranscriber.cpp
)

mutterkey_add_qt_test(platformlogicstest
    platformlogicstest.cpp
)

mutterkey_add_qt_test(traystatuswindowtest
    traystatuswindowtest.cpp
    ../src/control/daemoncontrolclient.cpp
    ../src/control/daemoncontrolprotocol.cpp
    ../src/control/daemoncontroltypes.cpp
    ../src/config.cpp
    ../src/tray/traystatuswindow.cpp
)

target_link_libraries(daemoncontrolclientservertest PRIVATE mutterkey_control)
target_link_libraries(platformlogicstest PRIVATE mutterkey_core)
target_link_libraries(whispercpptranscribertest PRIVATE whisper)

add_test(NAME testcommentarycheck
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-test-commentary.sh ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET clang-tidy)
    add_dependencies(clang-tidy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen whispercpptranscribertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
endif()

if(TARGET clazy)
    add_dependencies(clazy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen whispercpptranscribertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
endif()
