cmake_minimum_required(VERSION 3.22)

project(SkizoDroidLive VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_binary_data(SkizoDroidAssets
    SOURCES
        Assets/skizodroid_logo_white.png
)

juce_add_gui_app(SkizoDroidLive
    PRODUCT_NAME "SkizoDroid Live"
    COMPANY_NAME "Tricky Labs"
)

target_sources(SkizoDroidLive PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
)

target_compile_definitions(SkizoDroidLive PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(SkizoDroidLive PRIVATE
    SkizoDroidAssets
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
