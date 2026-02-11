add_executable(control_core_tests
  ${CMAKE_CURRENT_LIST_DIR}/control_core_tests.c
  ${CMAKE_CURRENT_LIST_DIR}/../../Fw/control/control_core.c
)

target_include_directories(control_core_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../Fw/control
)

target_compile_options(control_core_tests PRIVATE
  $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-std=gnu11>
)

add_test(NAME L1_control_core COMMAND control_core_tests)
set_tests_properties(L1_control_core PROPERTIES LABELS "L1")
