# Заглушка, чтобы CTest/CI имели L1 label даже до появления реальных тестов.
add_test(NAME L1_placeholder COMMAND ${CMAKE_COMMAND} -E echo "TODO: добавить L1 unit tests")
set_tests_properties(L1_placeholder PROPERTIES LABELS "L1")
