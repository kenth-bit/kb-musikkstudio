cmake_minimum_required(VERSION 3.22)

project(KBMusikkstudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KB_BUILD_TESTS "Bygg automatiske tester" ON)
option(KB_ENABLE_SANITIZERS "Slå på AddressSanitizer og UBSan (Linux/macOS)" OFF)
option(KB_WARNINGS_AS_ERRORS "Behandle advarsler som feil" OFF)

# JUCE festes til én bestemt versjon så bygget ikke endrer seg i det stille.
set(KB_JUCE_TAG "8.0.15" CACHE STRING "JUCE-versjon (git-tag)")
set(KB_JUCE_DIR "" CACHE PATH "Bruk en lokal JUCE-mappe i stedet for å laste ned")

if(KB_JUCE_DIR)
  add_subdirectory(${KB_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${KB_JUCE_TAG}
    GIT_SHALLOW ON
  )
  FetchContent_MakeAvailable(juce)
endif()

set(KB_SOURCES
  src/core/Logger.cpp
  src/core/Settings.cpp
  src/core/ParameterRegistry.cpp
  src/core/CrashHandler.cpp
  src/core/HelpTexts.cpp
  src/audio/AudioEngine.cpp
  src/audio/TestTone.cpp
  src/midi/MidiInputManager.cpp
  src/modules/ModuleRegistry.cpp
  src/app/StudioContext.cpp
  src/synth/SynthModule.cpp
  src/ui/Theme.cpp
  src/ui/Layout.cpp
  src/ui/ThemeLoader.cpp
  src/ui/HelpHub.cpp
  src/ui/KBLookAndFeel.cpp
  src/ui/Components.cpp
  src/ui/ConnectionPanel.cpp
  src/ui/MasterPanel.cpp
  src/ui/KeyboardPanel.cpp
  src/ui/LogPanel.cpp
  src/ui/SynthPanel.cpp
  src/ui/MainComponent.cpp
)

add_library(KBLib STATIC ${KB_SOURCES})
target_include_directories(KBLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ASIO-programvarepakken (SDK) fra Steinberg kan ikke følge med i prosjektet pga. lisensen.
# Pakk den ut i third_party/asiosdk, eller pek på den med -DKB_ASIO_SDK_DIR=...
# Uten SDK bygges programmet likevel, og bruker Windows Audio (WASAPI) i stedet.
set(KB_ASIO_SDK_DIR "$ENV{KB_ASIO_SDK_DIR}" CACHE PATH "Mappen med ASIO SDK (har common/iasiodrv.h)")
if(NOT KB_ASIO_SDK_DIR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asiosdk/common/iasiodrv.h)
  set(KB_ASIO_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asiosdk)
endif()

set(KB_HAVE_ASIO OFF)
if(WIN32)
  if(KB_ASIO_SDK_DIR AND EXISTS ${KB_ASIO_SDK_DIR}/common/iasiodrv.h)
    set(KB_HAVE_ASIO ON)
    message(STATUS "ASIO er slått på. SDK: ${KB_ASIO_SDK_DIR}")
    target_include_directories(KBLib PUBLIC
      ${KB_ASIO_SDK_DIR}/common ${KB_ASIO_SDK_DIR}/host ${KB_ASIO_SDK_DIR}/host/pc)
  else()
    message(WARNING "Fant ikke ASIO SDK. Programmet bygges med Windows Audio (WASAPI) i stedet for ASIO. "
                    "Se README.md, avsnittet om ASIO, for å slå på ASIO med Scarlett.")
  endif()
endif()

target_compile_definitions(KBLib PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_ASIO=$<BOOL:${KB_HAVE_ASIO}>
  JUCE_WASAPI=1
  JUCE_DIRECTSOUND=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_MODAL_LOOPS_PERMITTED=0
  KB_VERSION_STRING="${PROJECT_VERSION}"
  KB_SOURCE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_link_libraries(KBLib PUBLIC
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_utils
  juce::juce_gui_basics
  juce::juce_events
  juce::juce_core
  juce::juce_data_structures
  PRIVATE
  juce::juce_recommended_config_flags)

if(WIN32)
  target_link_libraries(KBLib PUBLIC winmm)
endif()

if(MSVC)
  target_compile_options(KBLib PUBLIC /W4 /utf-8 /wd4100 /wd4505)
  if(KB_WARNINGS_AS_ERRORS)
    target_compile_options(KBLib PUBLIC /WX)
  endif()
else()
  target_compile_options(KBLib PUBLIC -Wall -Wextra -Wno-unused-parameter)
  if(KB_WARNINGS_AS_ERRORS)
    target_compile_options(KBLib PUBLIC -Werror)
  endif()
endif()

if(KB_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(KBLib PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(KBLib PUBLIC -fsanitize=address,undefined)
endif()

# --- Selve programmet -------------------------------------------------------
set(KB_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico)
set(KB_ICON_ARGS "")
if(EXISTS ${KB_ICON_FILE})
  set(KB_ICON_ARGS ICON_BIG ${KB_ICON_FILE} ICON_SMALL ${KB_ICON_FILE})
endif()

juce_add_gui_app(KBMusikkstudio
  PRODUCT_NAME "KB Musikkstudio"
  COMPANY_NAME "KB"
  VERSION ${PROJECT_VERSION}
  ${KB_ICON_ARGS})

target_sources(KBMusikkstudio PRIVATE src/app/Main.cpp)
target_link_libraries(KBMusikkstudio PRIVATE
  KBLib
  juce::juce_recommended_config_flags)
if(KB_ENABLE_SANITIZERS AND NOT MSVC)
  target_link_options(KBMusikkstudio PRIVATE -fsanitize=address,undefined)
endif()

# Kopier resources/theme.json ved siden av programmet slik at F5 finner den.
add_custom_command(TARGET KBMusikkstudio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:KBMusikkstudio>/resources
  COMMENT "Kopierer resources/")

# --- Generator for docs/KNAPPEGUIDE.md --------------------------------------
add_executable(kb_docgen tools/docgen.cpp)
target_link_libraries(kb_docgen PRIVATE KBLib juce::juce_recommended_config_flags)
add_custom_target(docs
  COMMAND kb_docgen ${CMAKE_CURRENT_SOURCE_DIR}/docs/KNAPPEGUIDE.md
  DEPENDS kb_docgen
  COMMENT "Lager docs/KNAPPEGUIDE.md fra hjelpetekstene i koden")

# --- Tester -----------------------------------------------------------------
if(KB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
