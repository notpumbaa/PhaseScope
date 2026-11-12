cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), runs on macOS 12 and later
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
project(PhaseScope VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded from GitHub automatically on first configure.
# Alternatively clone JUCE locally and pass -DJUCE_DIR=/path/to/JUCE.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PhaseScope
    COMPANY_NAME                "PhaseScope"
    PRODUCT_NAME                "PhaseScope"
    PLUGIN_MANUFACTURER_CODE    Phsc
    PLUGIN_CODE                 Phs1
    FORMATS                     VST3 AU Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Analyzer Tools
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(PhaseScope PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PhaseScope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(PhaseScope PRIVATE /utf-8)
endif()

target_link_libraries(PhaseScope
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: helper tool that renders the screenshots in docs/
option(PHASESCOPE_BUILD_SCREENSHOTS "Build the screenshot tool" OFF)
if(PHASESCOPE_BUILD_SCREENSHOTS)
    juce_add_console_app(PhaseScopeScreenshots PRODUCT_NAME "PhaseScopeScreenshots")
    target_sources(PhaseScopeScreenshots PRIVATE
        tools/Screenshots.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(PhaseScopeScreenshots PRIVATE
        JucePlugin_Name="PhaseScope"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(PhaseScopeScreenshots PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
