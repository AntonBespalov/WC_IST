# Заглушки, чтобы CI мог различать L2_smoke и L2.
add_test(
  NAME L2_smoke_placeholder
  COMMAND ${CMAKE_COMMAND} -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/sil_summary_smoke.txt -P ${CMAKE_CURRENT_LIST_DIR}/sil_placeholder.cmake
)
set_tests_properties(L2_smoke_placeholder PROPERTIES LABELS "L2_smoke")

add_test(
  NAME L2_placeholder
  COMMAND ${CMAKE_COMMAND} -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/sil_summary.txt -P ${CMAKE_CURRENT_LIST_DIR}/sil_placeholder.cmake
)
set_tests_properties(L2_placeholder PROPERTIES LABELS "L2")
