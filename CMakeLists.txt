cmake_minimum_required(VERSION 3.22)
# Pin deployment requirements instead of inheriting the build machine's OS.
if(CMAKE_HOST_APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS distribution architectures")
endif()
project(SPAGlitch VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE checkout (optional)")
if(JUCE_SOURCE_DIR)
  add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 2cdfca8feb300fb424002ba2c2751569e5bacb64)
  FetchContent_MakeAvailable(JUCE)
endif()
set(formats VST3 Standalone)
if(APPLE)
  list(APPEND formats AU)
endif()
juce_add_plugin(SPAGlitch ICON_BIG Assets/branding/app-icon.png ICON_SMALL Assets/branding/app-icon.png COMPANY_NAME "Silverplatter Audio" BUNDLE_ID "com.silverplatteraudio.spaglitch" PLUGIN_MANUFACTURER_CODE Spau PLUGIN_CODE Spgl FORMATS ${formats} PRODUCT_NAME "SPAGlitch" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(SPAGlitch PRIVATE Source/Plugin.cpp Source/GlitchEngine.cpp Source/TubeModel.cpp Source/ContentLoader.cpp Source/PluginEditor.cpp Source/Drawer.cpp Source/MidiLearn.cpp Source/Randomizer.cpp Source/PresetManager.cpp Source/PresetBrowser.cpp Source/MidiLearnMenu.cpp
  Source/fx/FXChain.cpp Source/fx/FXParameters.cpp Source/fx/FXParamSnapshot.cpp Source/fx/FXTheme.cpp Source/fx/FXDisplays.cpp Source/fx/EqEditor.cpp Source/fx/FXControls.cpp Source/fx/FXSection.cpp)
target_compile_definitions(SPAGlitch PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
juce_add_binary_data(SpaArtwork SOURCES Assets/spa-calm.png
  "Assets/Presets/Aromatherapy Fault.spaglitch"
  "Assets/Presets/Body Polish.spaglitch"
  "Assets/Presets/Brine Pool.spaglitch"
  "Assets/Presets/Ceramic Static.spaglitch"
  "Assets/Presets/Charcoal Rinse.spaglitch"
  "Assets/Presets/Checkout Time.spaglitch"
  "Assets/Presets/Chlorine Dream.spaglitch"
  "Assets/Presets/Clay Mask Dry.spaglitch"
  "Assets/Presets/Closing Bell.spaglitch"
  "Assets/Presets/Cold Plunge.spaglitch"
  "Assets/Presets/Cold Towel Snap.spaglitch"
  "Assets/Presets/Compression Wrap.spaglitch"
  "Assets/Presets/Cotton Gown.spaglitch"
  "Assets/Presets/Cucumber Slice.spaglitch"
  "Assets/Presets/Cucumber Water.spaglitch"
  "Assets/Presets/Deep Tissue.spaglitch"
  "Assets/Presets/Eucalyptus Burst.spaglitch"
  "Assets/Presets/Exfoliant Grain.spaglitch"
  "Assets/Presets/Foil Wrap.spaglitch"
  "Assets/Presets/Gong Misfire.spaglitch"
  "Assets/Presets/Hot Stone Fracture.spaglitch"
  "Assets/Presets/Humidity Fault.spaglitch"
  "Assets/Presets/Hydrotherapy Jet.spaglitch"
  "Assets/Presets/Ice Fountain.spaglitch"
  "Assets/Presets/Infrared Cabin.spaglitch"
  "Assets/Presets/Jade Roller.spaglitch"
  "Assets/Presets/Kelp Wrap.spaglitch"
  "Assets/Presets/Late Cancellation.spaglitch"
  "Assets/Presets/Lavender Overdose.spaglitch"
  "Assets/Presets/Marble Bench.spaglitch"
  "Assets/Presets/Mineral Bath.spaglitch"
  "Assets/Presets/Mud Chamber.spaglitch"
  "Assets/Presets/Paraffin Dip.spaglitch"
  "Assets/Presets/Percussive Massage.spaglitch"
  "Assets/Presets/Pressure Suite.spaglitch"
  "Assets/Presets/Pumice Grind.spaglitch"
  "Assets/Presets/Quiet Room Violation.spaglitch"
  "Assets/Presets/Reception Chime.spaglitch"
  "Assets/Presets/Reflexology Map.spaglitch"
  "Assets/Presets/Robe Rustle.spaglitch"
  "Assets/Presets/Salt Scrub.spaglitch"
  "Assets/Presets/Sauna Overload.spaglitch"
  "Assets/Presets/Slipper Static.spaglitch"
  "Assets/Presets/Steam Room Collapse.spaglitch"
  "Assets/Presets/Steam Valve.spaglitch"
  "Assets/Presets/Thermal Blanket.spaglitch"
  "Assets/Presets/Towel Warmer.spaglitch"
  "Assets/Presets/Whale Song Buffer.spaglitch"
  Assets/spa-electric-1.png Assets/spa-electric-2.png Assets/spa-electric-3.png
  Assets/spa-electric-4.png Assets/spa-electric-5.png)
set_target_properties(SpaArtwork PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(SPAGlitch PUBLIC SpaArtwork juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
include(CTest)
option(SPAGLITCH_REFERENCE_HOST "Build the local Kontakt measurement host" OFF)
if(SPAGLITCH_REFERENCE_HOST)
  add_executable(KontaktProbe tools/KontaktProbe.cpp)
  target_compile_definitions(KontaktProbe PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(KontaktProbe PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
  add_executable(SPAGlitchRender tools/RenderNative.cpp)
  target_link_libraries(SPAGlitchRender PRIVATE SPAGlitch)
  juce_add_gui_app(GlitchReferenceHost PRODUCT_NAME "Glitch Reference Host")
  target_sources(GlitchReferenceHost PRIVATE tools/ReferenceHost.cpp)
  target_compile_definitions(GlitchReferenceHost PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(GlitchReferenceHost PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
if(BUILD_TESTING)
  add_executable(SPAGlitchTests)
  target_sources(SPAGlitchTests PRIVATE tests/ProcessorTests.cpp)
  target_link_libraries(SPAGlitchTests PRIVATE SPAGlitch)
  add_test(NAME processor COMMAND SPAGlitchTests)
  add_test(NAME measured_velocity COMMAND SPAGlitchTests --velocity-reference "${CMAKE_CURRENT_SOURCE_DIR}/docs/velocity-reference-results.json")
  add_test(NAME measured_lofi COMMAND SPAGlitchTests --lofi-reference "${CMAKE_CURRENT_SOURCE_DIR}/docs/lofi-transfer-fixtures.json")
  add_test(NAME measured_filter COMMAND SPAGlitchTests --filter-reference "${CMAKE_CURRENT_SOURCE_DIR}/docs/filter-transfer-fixtures.json")
endif()
