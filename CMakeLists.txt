cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(CrateDig VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CRATEDIG_BUILD_TESTS "Build the unit test runner" ON)

# JUCE is fetched at configure time. To use a local checkout instead, pass
# -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(CrateDig
    PRODUCT_NAME                "CrateDig"
    COMPANY_NAME                "CrateDig"
    BUNDLE_ID                   "io.github.recklessboise.cratedig"
    PLUGIN_MANUFACTURER_CODE    Crdg
    PLUGIN_CODE                 Cdg1
    FORMATS                     AU VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    DESCRIPTION                 "Dig random vintage records and drag them into your DAW")

set(CRATEDIG_CORE_SOURCES
    Source/Core/SampleInfo.cpp
    Source/Core/ArchiveApi.cpp
    Source/Core/Analysis.cpp
    Source/Core/Exporter.cpp)

target_sources(CrateDig PRIVATE
    ${CRATEDIG_CORE_SOURCES}
    Source/Core/Digger.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/CrateLookAndFeel.cpp
    Source/UI/WaveformView.cpp
    Source/UI/DragTile.cpp)

target_include_directories(CrateDig PRIVATE Source)

set(CRATEDIG_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    CRATEDIG_VERSION="${PROJECT_VERSION}")

target_compile_definitions(CrateDig PUBLIC ${CRATEDIG_DEFINITIONS})

target_link_libraries(CrateDig
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests (JUCE UnitTest runner, no extra dependency)
# ---------------------------------------------------------------------------
if(CRATEDIG_BUILD_TESTS)
    juce_add_console_app(CrateDigTests PRODUCT_NAME "CrateDigTests")

    target_sources(CrateDigTests PRIVATE
        ${CRATEDIG_CORE_SOURCES}
        Tests/TestMain.cpp
        Tests/ArchiveApiTests.cpp
        Tests/AnalysisTests.cpp
        Tests/ExporterTests.cpp)

    target_include_directories(CrateDigTests PRIVATE Source)
    target_compile_definitions(CrateDigTests PRIVATE ${CRATEDIG_DEFINITIONS} JUCE_UNIT_TESTS=1)

    target_link_libraries(CrateDigTests
        PRIVATE
            juce::juce_core
            juce::juce_data_structures
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME CrateDigTests COMMAND CrateDigTests)

    # Hits archive.org for real, so it is not registered with ctest.
    juce_add_console_app(CrateDigSmoke PRODUCT_NAME "CrateDigSmoke")
    target_sources(CrateDigSmoke PRIVATE ${CRATEDIG_CORE_SOURCES} Source/Core/Digger.cpp Tests/DigSmokeTest.cpp)
    target_include_directories(CrateDigSmoke PRIVATE Source)
    target_compile_definitions(CrateDigSmoke PRIVATE ${CRATEDIG_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(CrateDigSmoke
        PRIVATE
            juce::juce_events
            juce::juce_data_structures
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Editor snapshot tool (renders the UI to PNG, used for docs)
# ---------------------------------------------------------------------------
option(CRATEDIG_BUILD_SNAPSHOT "Build the editor snapshot tool" OFF)

if(CRATEDIG_BUILD_SNAPSHOT)
    juce_add_console_app(CrateDigSnapshot PRODUCT_NAME "CrateDigSnapshot")
    target_sources(CrateDigSnapshot PRIVATE tools/Snapshot.cpp)
    # Reuse the plugin's compiled shared code; only borrow its include paths and definitions.
    target_include_directories(CrateDigSnapshot PRIVATE Source $<TARGET_PROPERTY:CrateDig,INCLUDE_DIRECTORIES>)
    target_compile_definitions(CrateDigSnapshot PRIVATE $<TARGET_PROPERTY:CrateDig,COMPILE_DEFINITIONS>)
    target_link_libraries(CrateDigSnapshot PRIVATE CrateDig juce::juce_recommended_config_flags)
endif()
