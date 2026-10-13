cmake_minimum_required(VERSION 3.22)
project(ChipSynth VERSION 0.6.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# ---------------- ChipSynth (ساز) ----------------
juce_add_plugin(ChipSynth
    COMPANY_NAME "ChipSynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Cstm
    PLUGIN_CODE Chsy
    FORMATS VST3 Standalone
    PRODUCT_NAME "ChipSynth"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ChipSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ChipEngine.h
    Source/Arp.h
    Source/Effects.h
    Source/Drums.h
    Source/Sampler.h
    Source/Sfx.h)

target_compile_definitions(ChipSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ChipSynth
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# ---------------- ChipFX (افکت وکال و میکس) ----------------
juce_add_plugin(ChipFX
    COMPANY_NAME "ChipSynth"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Cstm
    PLUGIN_CODE Chfx
    FORMATS VST3 Standalone
    PRODUCT_NAME "ChipFX"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ChipFX PRIVATE
    Source/FX/FxProcessor.cpp
    Source/FX/FxProcessor.h
    Source/FX/FxEngine.h
    Source/Effects.h
    Source/ChipEngine.h)

target_compile_definitions(ChipFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ChipFX
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
