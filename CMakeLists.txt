cmake_minimum_required(VERSION 3.22)
project(SkizoDroidLive VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this to your JUCE checkout if JUCE is not installed system-wide.
# Example: add_subdirectory(C:/JUCE JUCE)
find_package(JUCE CONFIG REQUIRED)

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
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
