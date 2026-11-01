cmake_minimum_required(VERSION 3.22)
project(zychanstrip VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# AU is only built on macOS
if(APPLE)
    set(PLUGIN_FORMATS AU)
else()
    set(PLUGIN_FORMATS "")
endif()

juce_add_plugin(zychanstrip
    COMPANY_NAME "ZYUSOF"
    PLUGIN_MANUFACTURER_CODE ZYUS
    PLUGIN_CODE Zych
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "zychanstrip"
    BUNDLE_ID "net.zyusof.zychanstrip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics EQ)

target_sources(zychanstrip PRIVATE
    Source/Modules.h
    Source/Meters.h
    Source/MeterPanel.h
    Source/MeterPanel.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(zychanstrip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(zychanstrip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Ship the adapted EQ core's MIT attribution with the installed Audio Unit.
if(APPLE)
    set_source_files_properties(THIRD_PARTY_NOTICES.md PROPERTIES
        HEADER_FILE_ONLY TRUE
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(zychanstrip_AU PRIVATE THIRD_PARTY_NOTICES.md)
endif()
