cmake_minimum_required(VERSION 3.22)
project(VocalForge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # Windows: ohne VC++-Redistributable lauffähig

# JUCE wird automatisch heruntergeladen (oder: Ordner "JUCE" neben diese Datei legen)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(VF_COPY_PLUGIN "Plugin nach dem Bauen in den System-VST3-Ordner kopieren" ON)

set(VF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VF_FORMATS AU)
endif()

juce_add_plugin(VocalForge
    COMPANY_NAME "VocalForge"
    PLUGIN_MANUFACTURER_CODE Vfrg
    PLUGIN_CODE Vfg1
    FORMATS ${VF_FORMATS}
    PRODUCT_NAME "VocalForge"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${VF_COPY_PLUGIN}
    VST3_CATEGORIES Fx Dynamics EQ Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(VocalForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VocalForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VocalForge
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Optionaler Offline-Test (cmake -DVF_TESTS=ON)
option(VF_TESTS "Build offline DSP test" OFF)
if(VF_TESTS)
    juce_add_console_app(VocalForgeTest PRODUCT_NAME "VocalForgeTest")
    target_sources(VocalForgeTest PRIVATE Tests/Test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(VocalForgeTest PRIVATE JucePlugin_Name="VocalForge" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(VocalForgeTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
