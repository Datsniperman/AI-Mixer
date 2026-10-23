cmake_minimum_required(VERSION 3.22)
project(AIMixer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG:FULL")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG:FULL")
endif()

# JUCE 8 via FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(AIMIXER_COMMON_SOURCES
    Source/Common/Types.h
    Source/Common/SharedProtocol.h
    Source/Common/LockFreeFifo.h
    Source/Common/LoadTrace.h
    Source/Interprocess/SharedMemoryRegistry.h
    Source/Interprocess/SharedMemoryRegistry.cpp
    Source/Interprocess/InstanceNode.h
    Source/Interprocess/InstanceNode.cpp
    Source/Analysis/TrackNameParser.h
    Source/Analysis/TrackNameParser.cpp
    Source/Analysis/InstrumentClassifier.h
    Source/Analysis/InstrumentClassifier.cpp
    Source/DSP/Analyzer.h
    Source/DSP/Analyzer.cpp
    Source/DSP/SafeChain.h
    Source/DSP/SafeChain.cpp
    Source/Brain/MixSolver.h
    Source/Brain/MixSolver.cpp
    Source/Brain/TasteProfile.h
    Source/Brain/TasteProfile.cpp
    Source/Brain/ProfileManager.h
    Source/Brain/ProfileManager.cpp
    Source/DSP/ReverbEngine.h
    Source/DSP/ReverbEngine.cpp
    Source/DSP/DelayEngine.h
    Source/DSP/DelayEngine.cpp
    Source/DSP/SolverStage.h
    Source/DSP/SolverStage.cpp
    Source/DSP/PlaybookStage.h
    Source/DSP/PlaybookStage.cpp
    Source/DSP/BusStage.h
    Source/DSP/BusStage.cpp
    Source/Analysis/LoudnessMeter.h
    Source/Analysis/LoudnessMeter.cpp
    Source/DSP/MasterChain.h
    Source/DSP/MasterChain.cpp
)

set(AIMIXER_PLUGIN_SOURCES
    ${AIMIXER_COMMON_SOURCES}
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/UI/LookAndFeel.h
    Source/UI/LookAndFeel.cpp
    Source/UI/MasterMixView.h
    Source/UI/MasterMixView.cpp
    Source/UI/SourceView.h
    Source/UI/SourceView.cpp
    Source/UI/BusView.h
    Source/UI/BusView.cpp
)

# Build VST3 and Standalone
juce_add_plugin(AIMixer
    COMPANY_NAME "Churchfront"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE "ChFt"
    PLUGIN_CODE "Aimx"
    FORMATS VST3 Standalone
    PRODUCT_NAME "AI Mixer"
    VERSION "1.0.0"
    PLUGIN_DESCRIPTION "Real-time AI Mix Engine for modern worship and live production"
)

target_sources(AIMixer PRIVATE ${AIMIXER_PLUGIN_SOURCES})

target_compile_definitions(AIMixer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AIMixer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# MVP 1 Test Suite: 16 Instances Discovery, Rapid Churn, 0-Latency Verification
# ==============================================================================
add_executable(AIMixerTests
    Tests/RegistryTests.cpp
    ${AIMIXER_PLUGIN_SOURCES}
)

target_compile_definitions(AIMixerTests
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(AIMixerTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
