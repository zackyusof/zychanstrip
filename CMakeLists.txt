cmake_minimum_required(VERSION 3.22)
project(zychanstrip VERSION 1.1.1)

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

# Pinned MIT NAM engine. Profile weights stay in build/ for local evaluation;
# T3K model redistribution requires the creators' permission.
FetchContent_Declare(namcore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
    SOURCE_SUBDIR no-build)
FetchContent_MakeAvailable(namcore)
if(NOT EXISTS "${CMAKE_BINARY_DIR}/nam-assets/EmbeddedNam.h")
    message(FATAL_ERROR "Run: python3 tools/nam/prepare_profiles.py ${CMAKE_BINARY_DIR}/nam-assets (local evaluation profiles)")
endif()

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
    # Install policy (2026-11-03): GLOBAL ONLY.
    # Never copy into ~/Library/Audio/Plug-Ins — two bundles of the same AU subtype
    # confuse the registrar, and the user copy silently shadows a newer global build.
    # Builds therefore install nowhere; `tools/install_global.sh` (or `ninja
    # install-global`) is the ONLY install path and it targets /Library + removes
    # any user-space copy.
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics EQ)

juce_add_binary_data(zychanstrip_wood_frame
    NAMESPACE ZychanStripWoodFrameData
    SOURCES
        Assets/wood_top.png
        Assets/wood_bottom.png
        Assets/wood_left.png
        Assets/wood_right.png)

target_sources(zychanstrip PRIVATE
    Source/ColourEngine.cpp
    Source/Modules.h
    Source/Meters.h
    Source/MeterPanel.h
    Source/MeterPanel.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

file(GLOB NAM_ENGINE_SOURCES "${namcore_SOURCE_DIR}/NAM/*.cpp" "${namcore_SOURCE_DIR}/NAM/*/*.cpp")
target_sources(zychanstrip PRIVATE ${NAM_ENGINE_SOURCES})
target_compile_features(zychanstrip PUBLIC cxx_std_20)
target_compile_definitions(zychanstrip PRIVATE NAM_ENABLE_A2_FAST)
target_include_directories(zychanstrip SYSTEM PRIVATE
    "${namcore_SOURCE_DIR}" "${namcore_SOURCE_DIR}/Dependencies/eigen"
    "${namcore_SOURCE_DIR}/Dependencies/nlohmann" "${CMAKE_BINARY_DIR}/nam-assets")

target_compile_definitions(zychanstrip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Version / channel label shown in the GUI header, immediately after the wordmark.
# Convention:  stable  ->  "v<PROJECT_VERSION>"            e.g. v1.1.1
#              beta    ->  "v<PROJECT_VERSION>-<BETA>"     e.g. v1.1.1-beta.1
# PROJECT_VERSION stays numeric (the bundle plist needs it); the prerelease suffix lives
# only here, so the GUI can never disagree with the tag it was built from.
set(ZYCHANSTRIP_BETA "beta.10" CACHE STRING
    "Prerelease suffix rendered in the GUI header (e.g. beta.1); empty = stable")
if(ZYCHANSTRIP_BETA)
    set(ZYCHANSTRIP_GUI_VERSION "v${PROJECT_VERSION}-${ZYCHANSTRIP_BETA}")
else()
    set(ZYCHANSTRIP_GUI_VERSION "v${PROJECT_VERSION}")
endif()
message(STATUS "zychanstrip GUI version label: ${ZYCHANSTRIP_GUI_VERSION}")

target_compile_definitions(zychanstrip PUBLIC
    ZYCHANSTRIP_VERSION_LABEL="${ZYCHANSTRIP_GUI_VERSION}")

target_link_libraries(zychanstrip
    PRIVATE
        zychanstrip_wood_frame
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

    # Global-only installer. Builds never write to ~/Library; this target is the one
    # sanctioned path into /Library/Audio/Plug-Ins/Components (asks for sudo itself).
    add_custom_target(install-global
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_global.sh"
        DEPENDS zychanstrip_AU
        USES_TERMINAL
        COMMENT "Installing zychanstrip AU system-wide (global only, user space refused)")
endif()
