cmake_minimum_required(VERSION 3.22)

project(SDChord VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CI turns the copy off; locally the plugin lands straight in the DAW's plugin folder.
option(SDCH_COPY_PLUGIN "Copy the VST3 to SDCH_VST3_COPY_DIR after building" ON)
set(SDCH_VST3_COPY_DIR "D:/VST3Plugins" CACHE PATH "Folder the VST3 is copied to after building")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/JUCE JUCE)

juce_add_plugin(SDChord
    COMPANY_NAME "Pedro Marchiori"
    PLUGIN_MANUFACTURER_CODE Pmch
    PLUGIN_CODE Sdch
    FORMATS VST3 Standalone
    PRODUCT_NAME "SDChord"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${SDCH_COPY_PLUGIN}
    VST3_COPY_DIR "${SDCH_VST3_COPY_DIR}"
)

target_sources(SDChord
    PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
)

# The DSP core is header-only and has no JUCE in it, so it also builds for WebAssembly.
target_include_directories(SDChord PRIVATE dsp/include)

target_compile_definitions(SDChord
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SDChord
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Off-screen screenshot of the editor (tools/snapshot.cpp), built against the plugin's shared code.
juce_add_console_app(SDChordSnapshot PRODUCT_NAME "SDChordSnapshot")
target_sources(SDChordSnapshot PRIVATE tools/snapshot.cpp)
target_include_directories(SDChordSnapshot PRIVATE $<TARGET_PROPERTY:SDChord,INCLUDE_DIRECTORIES>)
target_compile_definitions(SDChordSnapshot PRIVATE $<TARGET_PROPERTY:SDChord,COMPILE_DEFINITIONS>)
target_link_libraries(SDChordSnapshot PRIVATE SDChord)
