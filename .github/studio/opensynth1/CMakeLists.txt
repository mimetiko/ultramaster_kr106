cmake_minimum_required(VERSION 3.22)
project(OpenSynth1Windows VERSION 1.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(OpenSynth1
    COMPANY_NAME "mimetiko"
    BUNDLE_ID "org.mimetiko.Synth1"
    PLUGIN_MANUFACTURER_CODE Mmtk
    PLUGIN_CODE MtS1
    PRODUCT_NAME "Synth 1"
    VERSION "1.2.1"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
)
juce_generate_juce_header(OpenSynth1)

# Physically shaded, reproducible 2x interface assets.
juce_add_binary_data(Synth1PhysicalSkin SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../synth1-skin/synth1_knob.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../synth1-skin/synth1_fader.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../synth1-skin/synth1_grain.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../synth1-skin/synth1_dark_panel.png
)

target_sources(OpenSynth1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/QuickOptionPicker.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/QuickOptionPicker.h
    Source/OpenSynthFilter.h
)

target_compile_definitions(OpenSynth1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OpenSynth1 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_events
    juce::juce_core
    juce::juce_dsp
    Synth1PhysicalSkin
    PUBLIC juce::juce_recommended_config_flags
)
