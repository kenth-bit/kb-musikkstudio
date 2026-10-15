# Tester uten ekstra avhengigheter. Kjør alt med:  ctest --test-dir build --output-on-failure
add_executable(kb_tests
  TestMain.cpp
  NoteMathTests.cpp
  MidiParserTests.cpp
  FifoTests.cpp
  DspTests.cpp
  SynthTests.cpp
  StressTests.cpp
  CoreTests.cpp
  UiLogicTests.cpp
  EngineTests.cpp)
target_include_directories(kb_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kb_tests PRIVATE KBLib juce::juce_recommended_config_flags)

set(KB_TEST_GROUPS
  NoteMath MidiParser Fifo Envelope SawOscillator SoftLimiter Dsp LevelMeter TestTone
  Synth Stress ConnectionTracker ParameterRegistry HelpTexts ModuleRegistry Settings
  MidiInputManager MidiState Midi Layout ThemeLoader Theme Engine)

foreach(group ${KB_TEST_GROUPS})
  add_test(NAME ${group} COMMAND kb_tests ${group})
  set_tests_properties(${group} PROPERTIES TIMEOUT 600)
endforeach()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME Tekstkoding COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/sjekk_tekst.py)
endif()

add_test(NAME Knappeguide_er_oppdatert
  COMMAND ${CMAKE_COMMAND}
    -DDOCGEN=$<TARGET_FILE:kb_docgen>
    -DEXPECTED=${CMAKE_SOURCE_DIR}/docs/KNAPPEGUIDE.md
    -DACTUAL=${CMAKE_BINARY_DIR}/KNAPPEGUIDE.generert.md
    -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckDocs.cmake)
