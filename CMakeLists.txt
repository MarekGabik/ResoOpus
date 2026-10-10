cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(ResoOpus VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(RESOOPUS_FORMATS VST3 AU Standalone)
    set(RESOOPUS_COPY TRUE)
else()
    set(RESOOPUS_FORMATS VST3 Standalone)
    set(RESOOPUS_COPY FALSE) # copying into Program Files needs admin rights
endif()

juce_add_plugin(ResoOpus
    COMPANY_NAME "Gavr"
    COMPANY_WEBSITE "https://gavr.cz"
    BUNDLE_ID "cz.gavr.resoopus"
    PLUGIN_MANUFACTURER_CODE Gavr
    PLUGIN_CODE RsOp
    PRODUCT_NAME "ResoOpus"
    FORMATS ${RESOOPUS_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${RESOOPUS_COPY}
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

set(RESOOPUS_SOURCES
    Source/Params/Parameters.cpp
    Source/DSP/ModMatrix.cpp
    Source/State/StateHistory.cpp
    Source/State/PresetManager.cpp
    Source/State/GlobalSettings.cpp
    Source/State/FactoryPresets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/Knob.cpp
    Source/UI/Widgets.cpp
    Source/UI/Pages.cpp
    Source/UI/Keyboard.cpp
    Source/UI/Bars.cpp
    Source/UI/PresetBrowser.cpp
    Source/UI/ModDrawer.cpp)

target_sources(ResoOpus PRIVATE ${RESOOPUS_SOURCES})
target_include_directories(ResoOpus PRIVATE Source)

target_compile_definitions(ResoOpus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ResoOpus
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test harness (not part of the plug-in): cmake -DRESOOPUS_TESTS=ON
option(RESOOPUS_TESTS "Build the offline test host" OFF)
option(RESOOPUS_SANITIZE "Build the test host with AddressSanitizer + UBSan" OFF)
if(RESOOPUS_TESTS)
    juce_add_console_app(ResoOpusTests PRODUCT_NAME "ResoOpusTests")
    target_sources(ResoOpusTests PRIVATE tools/TestHost/Main.cpp ${RESOOPUS_SOURCES})
    target_include_directories(ResoOpusTests PRIVATE Source)
    target_compile_definitions(ResoOpusTests PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="ResoOpus" JucePlugin_VersionString="0.1.0" JucePlugin_IsSynth=1)
    target_link_libraries(ResoOpusTests PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
    if(RESOOPUS_SANITIZE AND NOT MSVC)
        target_compile_options(ResoOpusTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ResoOpusTests PRIVATE -fsanitize=address,undefined)
    endif()
endif()
