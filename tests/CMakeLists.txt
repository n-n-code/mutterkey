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

mutterkey_add_qt_test(modelpackagetest
    modelpackagetest.cpp
    ../src/transcription/modelcatalog.cpp
    ../src/transcription/modelpackage.cpp
    ../src/transcription/modelvalidator.cpp
    ../src/transcription/rawwhisperimporter.cpp
    ../src/transcription/rawwhisperprobe.cpp
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

mutterkey_add_qt_test(cpureferencetranscribertest
    cpureferencetranscribertest.cpp
    ../src/transcription/cpureferencemodel.cpp
    ../src/transcription/modelcatalog.cpp
    ../src/transcription/transcriptionengine.cpp
    ../src/transcription/modelpackage.cpp
    ../src/transcription/modelvalidator.cpp
    ../src/transcription/cpureferencetranscriber.cpp
    ../src/transcription/rawwhisperprobe.cpp
    ../src/transcription/runtimeselector.cpp
)

if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    mutterkey_add_qt_test(whispercpptranscribertest
        whispercpptranscribertest.cpp
        ../src/transcription/cpureferencemodel.cpp
        ../src/transcription/modelcatalog.cpp
        ../src/transcription/transcriptionengine.cpp
        ../src/transcription/modelpackage.cpp
        ../src/transcription/modelvalidator.cpp
        ../src/transcription/rawwhisperprobe.cpp
        ../src/transcription/cpureferencetranscriber.cpp
        ../src/transcription/runtimeselector.cpp
        ../src/transcription/whispercpptranscriber.cpp
    )
    target_compile_definitions(whispercpptranscribertest PRIVATE MUTTERKEY_WITH_LEGACY_WHISPER)
    target_link_libraries(whispercpptranscribertest PRIVATE whisper)
endif()

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

add_test(NAME testcommentarycheck
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-test-commentary.sh ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET clang-tidy)
    add_dependencies(clang-tidy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen modelpackagetest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen cpureferencetranscribertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
    if(TARGET whispercpptranscribertest_autogen)
        add_dependencies(clang-tidy whispercpptranscribertest_autogen)
    endif()
endif()

if(TARGET clazy)
    add_dependencies(clazy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen modelpackagetest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen cpureferencetranscribertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
    if(TARGET whispercpptranscribertest_autogen)
        add_dependencies(clazy whispercpptranscribertest_autogen)
    endif()
endif()
