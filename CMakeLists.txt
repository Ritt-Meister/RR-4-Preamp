cmake_minimum_required(VERSION 3.22)
project(RR4Preamp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

if(DEFINED JUCE_LOCAL_DIR)
    add_subdirectory(${JUCE_LOCAL_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(RR_FORMATS AU VST3)
elseif(WIN32)
    set(RR_FORMATS VST3)
else()
    set(RR_FORMATS VST3)
endif()

juce_add_plugin(RR4Preamp
    COMPANY_NAME "RR"
    PLUGIN_MANUFACTURER_CODE Rrpl
    PLUGIN_CODE Rr04
    FORMATS ${RR_FORMATS}
    PRODUCT_NAME "RR-4 Preamp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx)

target_sources(RR4Preamp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(RR4Preamp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RR4Preamp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(DEFINED RR_BUILD_DSP_TEST)
    juce_add_console_app(DspTest PRODUCT_NAME "DspTest")
    target_sources(DspTest PRIVATE Tests/DspTest.cpp)
    target_include_directories(DspTest PRIVATE Source)
    target_compile_definitions(DspTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DspTest PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
endif()

if(DEFINED RR_BUILD_SNAPSHOT)
    juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
    target_sources(Snapshot PRIVATE Tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(Snapshot PRIVATE Source)
    target_compile_definitions(Snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_AU=0)
    target_link_libraries(Snapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
