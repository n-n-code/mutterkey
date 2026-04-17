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
    ../src/asr/streaming/audiochunker.cpp
    ../src/asr/streaming/transcriptassembler.cpp
    ../src/asr/streaming/transcriptioncompat.cpp
)

mutterkey_add_qt_test(modelpackagetest
    modelpackagetest.cpp
    ../src/asr/model/modelcatalog.cpp
    ../src/asr/model/modelpackage.cpp
    ../src/asr/model/modelvalidator.cpp
    ../src/asr/model/rawwhisperimporter.cpp
    ../src/asr/model/rawwhisperprobe.cpp
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
    ../src/asr/streaming/audiochunker.cpp
    ../src/asr/streaming/transcriptassembler.cpp
    ../src/asr/streaming/transcriptioncompat.cpp
    ../src/asr/runtime/transcriptionworker.cpp
)

mutterkey_add_qt_test(cpureferencetranscribertest
    cpureferencetranscribertest.cpp
    ../src/asr/nativecpu/cpudecoderforward.cpp
    ../src/asr/nativecpu/cpudecoderruntime.cpp
    ../src/asr/nativecpu/cpudecodergraph.cpp
    ../src/asr/nativecpu/cpuencoderforward.cpp
    ../src/asr/nativecpu/cpufeatureextractor.cpp
    ../src/asr/nativecpu/cpugreedysearch.cpp
    ../src/asr/nativecpu/cpumelspectrogram.cpp
    ../src/asr/nativecpu/cpumodelweights.cpp
    ../src/asr/nativecpu/cpureferencemodel.cpp
    ../src/asr/nativecpu/cputensor.cpp
    ../src/asr/model/modelcatalog.cpp
    ../src/asr/runtime/transcriptionengine.cpp
    ../src/asr/model/modelpackage.cpp
    ../src/asr/model/modelvalidator.cpp
    ../src/asr/nativecpu/cpureferencetranscriber.cpp
    ../src/asr/nativecpu/cpusessionstate.cpp
    ../src/asr/nativecpu/cputokensearch.cpp
    ../src/asr/nativecpu/cputokenizer.cpp
    ../src/asr/nativecpu/cpuwhispertokenizer.cpp
    ../src/asr/nativecpu/cputimestamps.cpp
    ../src/asr/model/rawwhisperprobe.cpp
    ../src/asr/runtime/runtimeselector.cpp
)

mutterkey_add_qt_test(cpudecoderruntimetest
    cpudecoderruntimetest.cpp
    ../src/asr/nativecpu/cpudecoderforward.cpp
    ../src/asr/nativecpu/cpudecoderruntime.cpp
    ../src/asr/nativecpu/cpudecodergraph.cpp
    ../src/asr/nativecpu/cpuencoderforward.cpp
    ../src/asr/nativecpu/cpufeatureextractor.cpp
    ../src/asr/nativecpu/cpugreedysearch.cpp
    ../src/asr/nativecpu/cpumelspectrogram.cpp
    ../src/asr/nativecpu/cpumodelweights.cpp
    ../src/asr/nativecpu/cputensor.cpp
    ../src/asr/nativecpu/cputokensearch.cpp
    ../src/asr/nativecpu/cputokenizer.cpp
    ../src/asr/nativecpu/cpuwhispertokenizer.cpp
    ../src/asr/nativecpu/cputimestamps.cpp
)

mutterkey_add_qt_test(cputensortest
    cputensortest.cpp
    ../src/asr/nativecpu/cputensor.cpp
)

mutterkey_add_qt_test(cpumelspectrogramtest
    cpumelspectrogramtest.cpp
    ../src/asr/nativecpu/cpumelspectrogram.cpp
    ../src/asr/nativecpu/cputensor.cpp
)

mutterkey_add_qt_test(cpurealdecodertest
    cpurealdecodertest.cpp
    testutil/pcmwavreader.cpp
    ../src/asr/model/modelpackage.cpp
    ../src/asr/model/modelvalidator.cpp
    ../src/asr/model/rawwhisperprobe.cpp
    ../src/asr/nativecpu/cpudecoderforward.cpp
    ../src/asr/nativecpu/cpudecoderruntime.cpp
    ../src/asr/nativecpu/cpudecodergraph.cpp
    ../src/asr/nativecpu/cpuencoderforward.cpp
    ../src/asr/nativecpu/cpufeatureextractor.cpp
    ../src/asr/nativecpu/cpugreedysearch.cpp
    ../src/asr/nativecpu/cpumelspectrogram.cpp
    ../src/asr/nativecpu/cpumodelweights.cpp
    ../src/asr/nativecpu/cpureferencemodel.cpp
    ../src/asr/nativecpu/cputensor.cpp
    ../src/asr/nativecpu/cputokensearch.cpp
    ../src/asr/nativecpu/cputokenizer.cpp
    ../src/asr/nativecpu/cpuwhispertokenizer.cpp
    ../src/asr/nativecpu/cputimestamps.cpp
)
target_include_directories(cpurealdecodertest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(cpurealdecodertest PRIVATE
    MUTTERKEY_TEST_JFK_WAV_PATH="${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/samples/jfk.wav"
)

mutterkey_add_qt_test(cpuwhispertokenizertest
    cpuwhispertokenizertest.cpp
    ../src/asr/nativecpu/cputokenizer.cpp
    ../src/asr/nativecpu/cpuwhispertokenizer.cpp
)

if(MUTTERKEY_ENABLE_LEGACY_WHISPER)
    mutterkey_add_qt_test(whispercpptranscribertest
        whispercpptranscribertest.cpp
        ../src/asr/nativecpu/cpudecoderforward.cpp
        ../src/asr/nativecpu/cpudecoderruntime.cpp
        ../src/asr/nativecpu/cpudecodergraph.cpp
        ../src/asr/nativecpu/cpuencoderforward.cpp
        ../src/asr/nativecpu/cpufeatureextractor.cpp
        ../src/asr/nativecpu/cpugreedysearch.cpp
        ../src/asr/nativecpu/cpumelspectrogram.cpp
        ../src/asr/nativecpu/cpumodelweights.cpp
        ../src/asr/nativecpu/cpureferencemodel.cpp
        ../src/asr/nativecpu/cputensor.cpp
        ../src/asr/model/modelcatalog.cpp
        ../src/asr/runtime/transcriptionengine.cpp
        ../src/asr/model/modelpackage.cpp
        ../src/asr/model/modelvalidator.cpp
        ../src/asr/model/rawwhisperprobe.cpp
        ../src/asr/nativecpu/cpureferencetranscriber.cpp
        ../src/asr/nativecpu/cpusessionstate.cpp
        ../src/asr/nativecpu/cputokensearch.cpp
        ../src/asr/nativecpu/cputokenizer.cpp
        ../src/asr/nativecpu/cpuwhispertokenizer.cpp
        ../src/asr/nativecpu/cputimestamps.cpp
        ../src/asr/runtime/runtimeselector.cpp
        ../src/asr/legacy/whispercpptranscriber.cpp
    )
    target_compile_definitions(whispercpptranscribertest PRIVATE MUTTERKEY_WITH_LEGACY_WHISPER)
    target_link_libraries(whispercpptranscribertest PRIVATE whisper)

    mutterkey_add_qt_test(cpuwhisperparitytest
        cpuwhisperparitytest.cpp
        testutil/pcmwavreader.cpp
    )
    target_include_directories(cpuwhisperparitytest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(cpuwhisperparitytest PRIVATE
        MUTTERKEY_WITH_LEGACY_WHISPER
        MUTTERKEY_TEST_JFK_WAV_PATH="${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/samples/jfk.wav"
    )
    target_link_libraries(cpuwhisperparitytest PRIVATE mutterkey_core whisper)
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
    add_dependencies(clang-tidy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen modelpackagetest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen cpureferencetranscribertest_autogen cpudecoderruntimetest_autogen cputensortest_autogen cpumelspectrogramtest_autogen cpurealdecodertest_autogen cpuwhispertokenizertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
    if(TARGET whispercpptranscribertest_autogen)
        add_dependencies(clang-tidy whispercpptranscribertest_autogen)
    endif()
    if(TARGET cpuwhisperparitytest_autogen)
        add_dependencies(clang-tidy cpuwhisperparitytest_autogen)
    endif()
endif()

if(TARGET clazy)
    add_dependencies(clazy configtest_autogen commanddispatchtest_autogen recordingnormalizertest_autogen streamingtranscriptiontest_autogen modelpackagetest_autogen daemoncontrolprotocoltest_autogen daemoncontroltypestest_autogen daemoncontrolclientservertest_autogen transcriptionworkertest_autogen cpureferencetranscribertest_autogen cpudecoderruntimetest_autogen cputensortest_autogen cpumelspectrogramtest_autogen cpurealdecodertest_autogen cpuwhispertokenizertest_autogen platformlogicstest_autogen traystatuswindowtest_autogen)
    if(TARGET whispercpptranscribertest_autogen)
        add_dependencies(clazy whispercpptranscribertest_autogen)
    endif()
    if(TARGET cpuwhisperparitytest_autogen)
        add_dependencies(clazy cpuwhisperparitytest_autogen)
    endif()
endif()

add_test(NAME changecontractcheck
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/test-change-contracts.sh
)

add_test(NAME featurerecordlifecyclecheck
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/test-feature-record-lifecycle.sh
)

set_tests_properties(changecontractcheck featurerecordlifecyclecheck PROPERTIES
    LABELS workflow
)
