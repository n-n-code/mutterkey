add_executable(nativecpubench
    main.cpp
    ../../src/asr/model/modelpackage.cpp
    ../../src/asr/model/modelvalidator.cpp
    ../../src/asr/model/rawwhisperprobe.cpp
    ../../src/asr/nativecpu/cpudecoderforward.cpp
    ../../src/asr/nativecpu/cpudecoderruntime.cpp
    ../../src/asr/nativecpu/cpudecodergraph.cpp
    ../../src/asr/nativecpu/cpuencoderforward.cpp
    ../../src/asr/nativecpu/cpufeatureextractor.cpp
    ../../src/asr/nativecpu/cpugreedysearch.cpp
    ../../src/asr/nativecpu/cpumelspectrogram.cpp
    ../../src/asr/nativecpu/cpumodelweights.cpp
    ../../src/asr/nativecpu/cpureferencemodel.cpp
    ../../src/asr/nativecpu/cputensor.cpp
    ../../src/asr/nativecpu/cputokensearch.cpp
    ../../src/asr/nativecpu/cputokenizer.cpp
    ../../src/asr/nativecpu/cpuwhispertokenizer.cpp
    ../../src/asr/nativecpu/cputimestamps.cpp
)

target_include_directories(nativecpubench PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(nativecpubench PRIVATE Qt6::Core)
